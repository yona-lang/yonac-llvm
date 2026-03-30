/*
 * Yona Runtime Library
 *
 * Linked with compiled Yona programs. Provides runtime support
 * for operations that can't be expressed as pure LLVM IR:
 * printing, string operations, memory management.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <ctype.h>
#include <pthread.h>
#include <setjmp.h>
#include <time.h>
#include <unistd.h>
#if defined(__linux__) || defined(__APPLE__)
#include <execinfo.h>
#define YONA_HAS_BACKTRACE 1
#elif defined(_WIN32)
#include <windows.h>
#include <dbghelp.h>
#define YONA_HAS_BACKTRACE 1
#endif

/* ===== Reference Counting Memory Management ===== */
/*
 * RC Header Layout (hidden before the returned payload pointer):
 *   [refcount: int64_t][type_tag: int64_t][...payload...]
 *                                         ^-- returned pointer
 *
 * refcount is at ptr[-2], type_tag is at ptr[-1].
 */

#define RC_TYPE_SEQ     1
#define RC_TYPE_SET     2
#define RC_TYPE_DICT    3
#define RC_TYPE_ADT     4
#define RC_TYPE_CLOSURE 5
#define RC_TYPE_STRING  6

/* Internal: allocate with RC header, returns pointer to payload */
void* rc_alloc(int64_t type_tag, size_t payload_bytes) {
    int64_t* raw = (int64_t*)malloc(2 * sizeof(int64_t) + payload_bytes);
    raw[0] = 1;         /* refcount = 1 */
    raw[1] = type_tag;  /* type tag */
    return (void*)(raw + 2);  /* return pointer past header */
}

/* Public: increment refcount */
void yona_rt_rc_inc(void* ptr) {
    if (!ptr) return;
    int64_t* header = ((int64_t*)ptr) - 2;
    header[0]++;
}

/* Sentinel refcount for arena-allocated objects. rc_dec skips these. */
#define RC_ARENA_SENTINEL INT64_MAX

/* Public: decrement refcount; free when it reaches 0 */
/* Children are NOT recursively decremented here — the codegen is
 * responsible for decrementing pointer-typed children before the
 * container, since element types are only known at compile time. */
void yona_rt_rc_dec(void* ptr) {
    if (!ptr) return;
    int64_t* header = ((int64_t*)ptr) - 2;
    if (header[0] == RC_ARENA_SENTINEL) return;  /* arena-allocated, skip */
    header[0]--;
    if (header[0] <= 0) {
        free(header);  /* free from the real allocation start */
    }
}

/* ===== Arena Allocator ===== */
/*
 * Bump-allocated memory block freed in bulk. Non-escaping values use
 * this instead of malloc+RC for zero-overhead deallocation.
 *
 * Layout: [yona_arena_t header][...bump-allocated payloads...]
 * Each payload has the standard RC header but with refcount=SENTINEL.
 */

#define YONA_ARENA_DEFAULT_SIZE 4096

typedef struct yona_arena {
    char* base;
    char* cursor;
    char* end;
    struct yona_arena* next;  /* overflow chain */
} yona_arena_t;

void* yona_rt_arena_create(int64_t size) {
    if (size <= 0) size = YONA_ARENA_DEFAULT_SIZE;
    yona_arena_t* arena = (yona_arena_t*)malloc(sizeof(yona_arena_t) + size);
    arena->base = (char*)(arena + 1);
    arena->cursor = arena->base;
    arena->end = arena->base + size;
    arena->next = NULL;
    return arena;
}

void* yona_rt_arena_alloc(void* arena_ptr, int64_t type_tag, int64_t payload_bytes) {
    yona_arena_t* arena = (yona_arena_t*)arena_ptr;
    size_t total = 2 * sizeof(int64_t) + (size_t)payload_bytes;
    /* Align to 8 bytes */
    total = (total + 7) & ~7;

    /* Find an arena block with enough space */
    while (arena->cursor + total > arena->end) {
        if (!arena->next) {
            /* Allocate overflow block (at least total or default size) */
            int64_t new_size = total > YONA_ARENA_DEFAULT_SIZE ? (int64_t)total * 2 : YONA_ARENA_DEFAULT_SIZE;
            arena->next = (yona_arena_t*)yona_rt_arena_create(new_size);
        }
        arena = arena->next;
    }

    int64_t* raw = (int64_t*)arena->cursor;
    arena->cursor += total;
    raw[0] = RC_ARENA_SENTINEL;  /* sentinel: rc_dec will skip */
    raw[1] = type_tag;
    return (void*)(raw + 2);  /* return pointer past header */
}

void yona_rt_arena_destroy(void* arena_ptr) {
    yona_arena_t* arena = (yona_arena_t*)arena_ptr;
    while (arena) {
        yona_arena_t* next = arena->next;
        free(arena);
        arena = next;
    }
}

#define RC_TYPE_BOX     7
#define RC_TYPE_BYTES   8
#define RC_TYPE_CONS    9   /* Cons cell: {head: i64, tail: ptr} */

/* Forward declarations */
int64_t* yona_rt_seq_alloc(int64_t count);
void yona_rt_rc_inc(void* ptr);

/* ===== Cons Cells — O(1) linked list nodes ===== */
/*
 * Layout: [rc_header][head: i64][tail: ptr (cons cell or flat seq or NULL)]
 *
 * A cons cell is a linked list node. The tail points to either:
 *   - Another cons cell (RC_TYPE_CONS)
 *   - A flat sequence (RC_TYPE_SEQ) — for when cons is applied to a literal
 *   - NULL — empty list
 *
 * Functions that need flat arrays (length, get, print) convert cons chains
 * to flat arrays via yona_rt_cons_to_seq.
 */

typedef struct {
    int64_t head;
    void* tail;   /* cons cell, flat seq, or NULL */
} yona_cons_t;

#define RC_TYPE_LAZY_CONS 10  /* Lazy cons: {head, thunk_fn, env, cached_tail, evaluated} */

typedef struct {
    int64_t head;
    void* thunk_fn;     /* function pointer: (env) -> i64 or () -> i64 */
    void* env;          /* environment for thunk (NULL if no captures) */
    void* cached_tail;  /* cached result after forcing (NULL = not yet forced) */
    int64_t evaluated;  /* 0 = thunk not yet forced, 1 = cached_tail valid */
} yona_lazy_cons_t;

/* Create a lazy cons cell */
void* yona_rt_seq_lazy_cons(int64_t head, void* thunk_fn, void* env) {
    yona_lazy_cons_t* cell = (yona_lazy_cons_t*)rc_alloc(RC_TYPE_LAZY_CONS, sizeof(yona_lazy_cons_t));
    cell->head = head;
    cell->thunk_fn = thunk_fn;
    cell->env = env;
    cell->cached_tail = NULL;
    cell->evaluated = 0;
    return cell;
}

/* Force a lazy cons tail — evaluate thunk if needed, return cached tail */
static void* force_lazy_tail(yona_lazy_cons_t* cell) {
    if (cell->evaluated) return cell->cached_tail;
    /* Call thunk to produce the tail */
    if (cell->env) {
        typedef int64_t (*thunk_env_fn)(void*);
        int64_t result = ((thunk_env_fn)cell->thunk_fn)(cell->env);
        cell->cached_tail = (void*)(intptr_t)result;
    } else {
        typedef int64_t (*thunk_fn)(void);
        int64_t result = ((thunk_fn)cell->thunk_fn)();
        cell->cached_tail = (void*)(intptr_t)result;
    }
    cell->evaluated = 1;
    return cell->cached_tail;
}

static int is_lazy_cons(void* ptr) {
    if (!ptr) return 0;
    int64_t* header = ((int64_t*)ptr) - 2;
    return header[1] == RC_TYPE_LAZY_CONS;
}

/* Check if a pointer is a cons cell by examining its RC type tag */
static int is_cons_cell(void* ptr) {
    if (!ptr) return 0;
    int64_t* header = ((int64_t*)ptr) - 2;
    return header[1] == RC_TYPE_CONS;
}

/* Check if a pointer is a flat sequence */
static int is_flat_seq(void* ptr) {
    if (!ptr) return 0;
    int64_t* header = ((int64_t*)ptr) - 2;
    return header[1] == RC_TYPE_SEQ;
}

/* Count elements in a cons/lazy chain */
static int64_t cons_length(void* list) {
    int64_t len = 0;
    void* p = list;
    while (p) {
        if (is_cons_cell(p)) {
            len++;
            p = ((yona_cons_t*)p)->tail;
        } else if (is_lazy_cons(p)) {
            len++;
            p = force_lazy_tail((yona_lazy_cons_t*)p);
        } else if (is_flat_seq(p)) {
            len += ((int64_t*)p)[0];
            break;
        } else break;
    }
    return len;
}

/* Convert a cons/lazy chain to a flat sequence */
int64_t* yona_rt_cons_to_seq(void* list) {
    int64_t len = cons_length(list);
    int64_t* seq = yona_rt_seq_alloc(len);
    int64_t i = 1;
    void* p = list;
    while (p) {
        if (is_cons_cell(p)) {
            seq[i++] = ((yona_cons_t*)p)->head;
            p = ((yona_cons_t*)p)->tail;
        } else if (is_lazy_cons(p)) {
            seq[i++] = ((yona_lazy_cons_t*)p)->head;
            p = force_lazy_tail((yona_lazy_cons_t*)p);
        } else if (is_flat_seq(p)) {
            int64_t* flat = (int64_t*)p;
            int64_t flat_len = flat[0];
            memcpy(seq + i, flat + 1, (size_t)flat_len * sizeof(int64_t));
            i += flat_len;
            break;
        } else break;
    }
    return seq;
}

/* Get head of a list (cons, lazy cons, or flat seq) */
int64_t yona_rt_list_head(void* list) {
    if (is_cons_cell(list)) return ((yona_cons_t*)list)->head;
    if (is_lazy_cons(list)) return ((yona_lazy_cons_t*)list)->head;
    if (is_flat_seq(list)) return ((int64_t*)list)[1];
    return 0;
}

/* Get tail of a list — returns a list pointer */
void* yona_rt_list_tail(void* list) {
    if (is_cons_cell(list)) return ((yona_cons_t*)list)->tail;
    if (is_lazy_cons(list)) return force_lazy_tail((yona_lazy_cons_t*)list);
    if (is_flat_seq(list)) {
        int64_t* flat = (int64_t*)list;
        int64_t len = flat[0];
        if (len <= 1) return NULL;
        int64_t* result = yona_rt_seq_alloc(len - 1);
        memcpy(result + 1, flat + 2, (size_t)(len - 1) * sizeof(int64_t));
        return result;
    }
    return NULL;
}

/* Get length of a list (cons or flat) */
int64_t yona_rt_list_length(void* list) {
    if (!list) return 0;
    if (is_flat_seq(list)) return ((int64_t*)list)[0];
    return cons_length(list);
}

/* Fast empty check — O(1) for cons, lazy cons, and flat */
int64_t yona_rt_seq_is_empty(int64_t* seq) {
    if (!seq) return 1;
    if (is_cons_cell((void*)seq)) return 0;
    if (is_lazy_cons((void*)seq)) return 0;
    return seq[0] == 0;
}

/* Forward declaration */
int64_t* yona_rt_seq_alloc(int64_t count);

/* ===== Bytes — length-prefixed byte buffer ===== */
/*
 * Layout: [rc_header][length: i64][byte0, byte1, ...]
 *                                  ^-- returned pointer
 * Unlike strings, Bytes can contain \0 and arbitrary binary data.
 * Length is stored at ptr[-1] (the i64 before the data pointer... no,
 * actually we store length at index 0 of the payload, same as SEQ).
 *
 * Actual layout: rc_alloc returns payload pointer.
 *   payload[0] = length (as i64)
 *   payload[1..] = bytes (packed as uint8_t, but stored after the i64 length)
 *
 * We use the same pattern as sequences: first i64 is length, then data.
 */

/* Allocate a Bytes buffer of the given size (uninitialized) */
void* yona_rt_bytes_alloc(int64_t size) {
    int64_t* buf = (int64_t*)rc_alloc(RC_TYPE_BYTES, sizeof(int64_t) + (size_t)size);
    buf[0] = size;
    return buf;
}

/* Get length of a Bytes buffer */
int64_t yona_rt_bytes_length(void* bytes) {
    return ((int64_t*)bytes)[0];
}

/* Get byte at index (returns 0-255) */
int64_t yona_rt_bytes_get(void* bytes, int64_t index) {
    int64_t len = ((int64_t*)bytes)[0];
    if (index < 0 || index >= len) return 0;
    uint8_t* data = (uint8_t*)((int64_t*)bytes + 1);
    return (int64_t)data[index];
}

/* Set byte at index */
void yona_rt_bytes_set(void* bytes, int64_t index, int64_t value) {
    int64_t len = ((int64_t*)bytes)[0];
    if (index < 0 || index >= len) return;
    uint8_t* data = (uint8_t*)((int64_t*)bytes + 1);
    data[index] = (uint8_t)(value & 0xFF);
}

/* Concatenate two Bytes buffers */
void* yona_rt_bytes_concat(void* a, void* b) {
    int64_t len_a = ((int64_t*)a)[0];
    int64_t len_b = ((int64_t*)b)[0];
    int64_t* result = (int64_t*)rc_alloc(RC_TYPE_BYTES, sizeof(int64_t) + (size_t)(len_a + len_b));
    result[0] = len_a + len_b;
    uint8_t* dest = (uint8_t*)(result + 1);
    memcpy(dest, (uint8_t*)((int64_t*)a + 1), (size_t)len_a);
    memcpy(dest + len_a, (uint8_t*)((int64_t*)b + 1), (size_t)len_b);
    return result;
}

/* Slice: bytes[start..start+len] */
void* yona_rt_bytes_slice(void* bytes, int64_t start, int64_t len) {
    int64_t total = ((int64_t*)bytes)[0];
    if (start < 0) start = 0;
    if (start + len > total) len = total - start;
    if (len <= 0) return yona_rt_bytes_alloc(0);
    int64_t* result = (int64_t*)rc_alloc(RC_TYPE_BYTES, sizeof(int64_t) + (size_t)len);
    result[0] = len;
    uint8_t* src = (uint8_t*)((int64_t*)bytes + 1) + start;
    memcpy((uint8_t*)(result + 1), src, (size_t)len);
    return result;
}

/* Convert String to Bytes (copies, no null terminator in output) */
void* yona_rt_bytes_from_string(const char* s) {
    size_t len = strlen(s);
    int64_t* buf = (int64_t*)rc_alloc(RC_TYPE_BYTES, sizeof(int64_t) + len);
    buf[0] = (int64_t)len;
    memcpy((uint8_t*)(buf + 1), s, len);
    return buf;
}

/* Convert Bytes to String (adds null terminator) */
const char* yona_rt_bytes_to_string(void* bytes) {
    int64_t len = ((int64_t*)bytes)[0];
    char* s = (char*)rc_alloc(RC_TYPE_STRING, (size_t)len + 1);
    memcpy(s, (uint8_t*)((int64_t*)bytes + 1), (size_t)len);
    s[len] = '\0';
    return s;
}

/* Create Bytes from a list of integers (each 0-255) */
void* yona_rt_bytes_from_seq(int64_t* seq) {
    int64_t len = seq[0];
    int64_t* buf = (int64_t*)rc_alloc(RC_TYPE_BYTES, sizeof(int64_t) + (size_t)len);
    buf[0] = len;
    uint8_t* data = (uint8_t*)(buf + 1);
    for (int64_t i = 0; i < len; i++)
        data[i] = (uint8_t)(seq[i + 1] & 0xFF);
    return buf;
}

/* Convert Bytes to a list of integers (each 0-255) */
int64_t* yona_rt_bytes_to_seq(void* bytes) {
    int64_t len = ((int64_t*)bytes)[0];
    int64_t* seq = yona_rt_seq_alloc(len);
    uint8_t* data = (uint8_t*)((int64_t*)bytes + 1);
    for (int64_t i = 0; i < len; i++)
        seq[i + 1] = (int64_t)data[i];
    return seq;
}

/* Print Bytes as hex for debugging */
void yona_rt_print_bytes(void* bytes) {
    int64_t len = ((int64_t*)bytes)[0];
    uint8_t* data = (uint8_t*)((int64_t*)bytes + 1);
    printf("<<");
    for (int64_t i = 0; i < len; i++) {
        if (i > 0) printf(", ");
        printf("%d", data[i]);
    }
    printf(">>");
}

/* Std\Bytes module aliases */
void* yona_Std_Bytes__alloc(int64_t s)          { return yona_rt_bytes_alloc(s); }
int64_t yona_Std_Bytes__length(void* b)         { return yona_rt_bytes_length(b); }
int64_t yona_Std_Bytes__get(void* b, int64_t i) { return yona_rt_bytes_get(b, i); }
void yona_Std_Bytes__set(void* b, int64_t i, int64_t v) { yona_rt_bytes_set(b, i, v); }
void* yona_Std_Bytes__concat(void* a, void* b)  { return yona_rt_bytes_concat(a, b); }
void* yona_Std_Bytes__slice(void* b, int64_t s, int64_t l) { return yona_rt_bytes_slice(b, s, l); }
void* yona_Std_Bytes__fromString(const char* s) { return yona_rt_bytes_from_string(s); }
const char* yona_Std_Bytes__toString(void* b)   { return yona_rt_bytes_to_string(b); }
void* yona_Std_Bytes__fromSeq(int64_t* s)       { return yona_rt_bytes_from_seq(s); }
int64_t* yona_Std_Bytes__toSeq(void* b)         { return yona_rt_bytes_to_seq(b); }

/* Box: heap-allocate arbitrary data (for tuples in collections) */
void* yona_rt_box(const void* data, int64_t size) {
    void* box = rc_alloc(RC_TYPE_BOX, (size_t)size);
    memcpy(box, data, (size_t)size);
    return box;
}

/* Unbox: just returns the pointer (data is at the payload position) */
/* No runtime function needed — codegen does inttoptr + load directly */

/* Public: allocate an RC-managed string buffer (for platform layer) */
void* yona_rt_rc_alloc_string(size_t bytes) {
    return rc_alloc(RC_TYPE_STRING, bytes);
}

void yona_rt_print_int(int64_t value) {
    printf("%ld", value);
}

void yona_rt_print_float(double value) {
    printf("%g", value);
}

void yona_rt_print_string(const char* value) {
    printf("%s", value);
}

void yona_rt_print_bool(int value) {
    printf("%s", value ? "true" : "false");
}

void yona_rt_print_newline(void) {
    printf("\n");
}

char* yona_rt_string_concat(const char* a, const char* b) {
    size_t len_a = strlen(a);
    size_t len_b = strlen(b);
    char* result = (char*)rc_alloc(RC_TYPE_STRING, len_a + len_b + 1);
    memcpy(result, a, len_a);
    memcpy(result + len_a, b, len_b + 1);
    return result;
}

/* ===== Sequence (list) runtime ===== */

// Sequence: [length, elem0, elem1, ...]
// Stored as a heap-allocated array of i64 where index 0 is the length.

int64_t* yona_rt_seq_alloc(int64_t count) {
    int64_t* seq = (int64_t*)rc_alloc(RC_TYPE_SEQ, (count + 1) * sizeof(int64_t));
    seq[0] = count;
    return seq;
}

int64_t yona_rt_seq_length(int64_t* seq) {
    return yona_rt_list_length((void*)seq);
}

int64_t yona_rt_seq_get(int64_t* seq, int64_t index) {
    if (is_flat_seq((void*)seq)) return seq[index + 1];
    /* Cons chain: walk to the index-th element */
    void* p = (void*)seq;
    int64_t i = 0;
    while (p && i < index) {
        if (is_cons_cell(p)) { p = ((yona_cons_t*)p)->tail; i++; }
        else if (is_flat_seq(p)) { return ((int64_t*)p)[index - i + 1]; }
        else return 0;
    }
    return yona_rt_list_head(p);
}

void yona_rt_seq_set(int64_t* seq, int64_t index, int64_t value) {
    seq[index + 1] = value;
}

void yona_rt_print_seq(int64_t* seq) {
    if (is_cons_cell((void*)seq) || is_lazy_cons((void*)seq)) {
        int64_t* flat = yona_rt_cons_to_seq((void*)seq);
        yona_rt_print_seq(flat);
        return;
    }
    int64_t len = seq[0];
    printf("[");
    for (int64_t i = 0; i < len; i++) {
        if (i > 0) printf(", ");
        printf("%ld", seq[i + 1]);
    }
    printf("]");
}

// Cons: prepend element to sequence
int64_t* yona_rt_seq_cons(int64_t elem, int64_t* seq) {
    /* O(1) cons: allocate a cons cell pointing to the existing list */
    yona_cons_t* cell = (yona_cons_t*)rc_alloc(RC_TYPE_CONS, sizeof(yona_cons_t));
    cell->head = elem;
    cell->tail = (void*)seq;
    /* rc_inc the tail since the cons cell now holds a reference */
    if (seq) yona_rt_rc_inc((void*)seq);
    return (int64_t*)cell;
}

// Join: concatenate two sequences
int64_t* yona_rt_seq_join(int64_t* a, int64_t* b) {
    /* Materialize cons chains to flat arrays */
    if (is_cons_cell((void*)a)) a = yona_rt_cons_to_seq((void*)a);
    if (is_cons_cell((void*)b)) b = yona_rt_cons_to_seq((void*)b);

    int64_t len_a = a[0];
    int64_t len_b = b[0];
    int64_t* header_a = a - 2;

    if (header_a[0] == 1 && header_a[0] != RC_ARENA_SENTINEL) {
        size_t new_payload = ((size_t)(len_a + len_b) + 1) * sizeof(int64_t);
        int64_t* new_header = (int64_t*)realloc(header_a, 2 * sizeof(int64_t) + new_payload);
        int64_t* new_a = new_header + 2;
        memcpy(new_a + 1 + len_a, b + 1, (size_t)len_b * sizeof(int64_t));
        new_a[0] = len_a + len_b;
        return new_a;
    }

    int64_t* result = yona_rt_seq_alloc(len_a + len_b);
    memcpy(result + 1, a + 1, (size_t)len_a * sizeof(int64_t));
    memcpy(result + 1 + len_a, b + 1, (size_t)len_b * sizeof(int64_t));
    return result;
}

/* ===== Symbol runtime ===== */
/* Symbols are interned to i64 IDs at compile time. Comparison is icmp eq. */
/* Print takes the string name (resolved by the compiler from the symbol table). */

void yona_rt_print_symbol(const char* name) {
    printf(":%s", name);
}

/* ===== Set runtime ===== */
/* Set: [count, elem0, elem1, ...] — same layout as sequence.         */
/* Elements are i64 (the element type is known at compile time).       */
/* Deduplication is the caller's responsibility at construction time.  */

int64_t* yona_rt_set_alloc(int64_t count) {
    int64_t* set = (int64_t*)rc_alloc(RC_TYPE_SET, (count + 1) * sizeof(int64_t));
    set[0] = count;
    return set;
}

void yona_rt_set_put(int64_t* set, int64_t index, int64_t value) {
    set[index + 1] = value;
}

void yona_rt_print_set(int64_t* set) {
    int64_t len = set[0];
    printf("{");
    for (int64_t i = 0; i < len; i++) {
        if (i > 0) printf(", ");
        printf("%ld", set[i + 1]);
    }
    printf("}");
}

/* ===== Dict runtime ===== */
/* Dict: [count, key0, val0, key1, val1, ...] — interleaved keys and values. */
/* Both keys and values are i64 (their types are known at compile time).     */

int64_t* yona_rt_dict_alloc(int64_t count) {
    int64_t* dict = (int64_t*)rc_alloc(RC_TYPE_DICT, (count * 2 + 1) * sizeof(int64_t));
    dict[0] = count;
    return dict;
}

void yona_rt_dict_set(int64_t* dict, int64_t index, int64_t key, int64_t value) {
    dict[index * 2 + 1] = key;
    dict[index * 2 + 2] = value;
}

void yona_rt_print_dict(int64_t* dict) {
    int64_t len = dict[0];
    printf("{");
    for (int64_t i = 0; i < len; i++) {
        if (i > 0) printf(", ");
        printf("%ld: %ld", dict[i * 2 + 1], dict[i * 2 + 2]);
    }
    printf("}");
}

/* ===== ADT runtime (recursive types) ===== */
/* Heap-allocated ADT nodes: [tag (i8), field0 (i64), field1 (i64), ...] */
/* Used for recursive types like List a = Cons a (List a) | Nil        */

void* yona_rt_adt_alloc(int64_t tag, int64_t num_fields) {
    int64_t* node = (int64_t*)rc_alloc(RC_TYPE_ADT, (1 + num_fields) * sizeof(int64_t));
    node[0] = tag;
    return node;
}

int64_t yona_rt_adt_get_tag(void* node) {
    return ((int64_t*)node)[0];
}

int64_t yona_rt_adt_get_field(void* node, int64_t index) {
    return ((int64_t*)node)[index + 1];
}

void yona_rt_adt_set_field(void* node, int64_t index, int64_t value) {
    ((int64_t*)node)[index + 1] = value;
}

/* ===== Exception handling (setjmp/longjmp) ===== */

#define YONA_MAX_TRY_DEPTH 256

typedef struct {
    int64_t symbol;
    const char* message;
} yona_exception_t;

typedef struct {
    jmp_buf buf[YONA_MAX_TRY_DEPTH];
    int depth;
    yona_exception_t current;
} yona_exc_state_t;

static _Thread_local yona_exc_state_t yona_exc = { .depth = 0 };

// yona_rt_try_push: push a jmp_buf slot, return pointer to it.
// The caller must call setjmp on the returned pointer directly
// (setjmp must execute in the caller's stack frame).
void* yona_rt_try_push(void) {
    if (yona_exc.depth >= YONA_MAX_TRY_DEPTH) {
        fprintf(stderr, "Fatal: try/catch nesting depth exceeded\n");
        abort();
    }
    return &yona_exc.buf[yona_exc.depth++];
}

void yona_rt_try_end(void) {
    if (yona_exc.depth > 0) yona_exc.depth--;
}

void yona_rt_print_stacktrace(void) {
#if defined(__linux__) || defined(__APPLE__)
    void* frames[64];
    int n = backtrace(frames, 64);
    char** syms = backtrace_symbols(frames, n);
    if (syms) {
        fprintf(stderr, "Stack trace:\n");
        for (int i = 2; i < n; i++)
            fprintf(stderr, "  %s\n", syms[i]);
        free(syms);
    }
#elif defined(_WIN32)
    void* frames[64];
    HANDLE process = GetCurrentProcess();
    SymInitialize(process, NULL, TRUE);
    WORD n = CaptureStackBackTrace(2, 62, frames, NULL);
    fprintf(stderr, "Stack trace:\n");
    SYMBOL_INFO* sym = (SYMBOL_INFO*)calloc(sizeof(SYMBOL_INFO) + 256, 1);
    sym->MaxNameLen = 255;
    sym->SizeOfStruct = sizeof(SYMBOL_INFO);
    for (WORD i = 0; i < n; i++) {
        SymFromAddr(process, (DWORD64)frames[i], 0, sym);
        fprintf(stderr, "  %s\n", sym->Name);
    }
    free(sym);
#endif
}

void yona_rt_raise(int64_t symbol, const char* message) {
    if (yona_exc.depth == 0) {
        /* For display: message contains "symbol_name\0actual_message" if from codegen,
           but since we get them separately, just print both */
        fprintf(stderr, "Unhandled exception: \"%s\"\n", message ? message : "");
        yona_rt_print_stacktrace();
        abort();
    }
    yona_exc.current.symbol = symbol;
    yona_exc.current.message = message;
    yona_exc.depth--;
    longjmp(yona_exc.buf[yona_exc.depth], 1);
}

int64_t yona_rt_get_exception_symbol(void) {
    return yona_exc.current.symbol;
}

const char* yona_rt_get_exception_message(void) {
    return yona_exc.current.message;
}

/* Forward declarations for runtime functions used by shims */
int64_t* yona_rt_seq_alloc(int64_t count);
int64_t* yona_rt_seq_tail(int64_t* seq);

/* ===== Native stdlib shims ===== */
/* Pure C implementations of common stdlib functions for compiled code. */
/* These use the same mangled names as compiled Yona modules. */

/* Std\Math */
int64_t yona_Std_Math__abs(int64_t x) { return x < 0 ? -x : x; }
int64_t yona_Std_Math__max(int64_t a, int64_t b) { return a > b ? a : b; }
int64_t yona_Std_Math__min(int64_t a, int64_t b) { return a < b ? a : b; }
int64_t yona_Std_Math__factorial(int64_t n) {
    int64_t r = 1;
    for (int64_t i = 2; i <= n; i++) r *= i;
    return r;
}
double yona_Std_Math__sqrt(double x) { return sqrt(x); }
double yona_Std_Math__sin(double x) { return sin(x); }
double yona_Std_Math__cos(double x) { return cos(x); }

/* Std\String — pure string operations, no I/O */

int64_t yona_Std_String__length(const char* s) {
    return (int64_t)strlen(s);
}

const char* yona_Std_String__toUpperCase(const char* s) {
    size_t len = strlen(s);
    char* r = (char*)rc_alloc(RC_TYPE_STRING, len + 1);
    for (size_t i = 0; i <= len; i++) r[i] = (char)toupper((unsigned char)s[i]);
    return r;
}

const char* yona_Std_String__toLowerCase(const char* s) {
    size_t len = strlen(s);
    char* r = (char*)rc_alloc(RC_TYPE_STRING, len + 1);
    for (size_t i = 0; i <= len; i++) r[i] = (char)tolower((unsigned char)s[i]);
    return r;
}

const char* yona_Std_String__trim(const char* s) {
    const char* start = s;
    while (*start && (*start == ' ' || *start == '\t' || *start == '\n' || *start == '\r')) start++;
    const char* end = s + strlen(s) - 1;
    while (end > start && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) end--;
    size_t len = end - start + 1;
    char* r = (char*)rc_alloc(RC_TYPE_STRING, len + 1);
    memcpy(r, start, len);
    r[len] = '\0';
    return r;
}

int64_t yona_Std_String__indexOf(const char* needle, const char* haystack) {
    const char* p = strstr(haystack, needle);
    return p ? (int64_t)(p - haystack) : -1;
}

int64_t yona_Std_String__contains(const char* needle, const char* haystack) {
    return strstr(haystack, needle) != NULL;
}

int64_t yona_Std_String__startsWith(const char* prefix, const char* s) {
    size_t plen = strlen(prefix);
    return strncmp(s, prefix, plen) == 0;
}

int64_t yona_Std_String__endsWith(const char* suffix, const char* s) {
    size_t slen = strlen(s);
    size_t xlen = strlen(suffix);
    if (xlen > slen) return 0;
    return strcmp(s + slen - xlen, suffix) == 0;
}

const char* yona_Std_String__substring(const char* s, int64_t start, int64_t len) {
    size_t slen = strlen(s);
    if (start < 0) start = 0;
    if ((size_t)start >= slen) { char* r = (char*)rc_alloc(RC_TYPE_STRING, 1); r[0] = '\0'; return r; }
    if (len < 0 || (size_t)(start + len) > slen) len = slen - start;
    char* r = (char*)rc_alloc(RC_TYPE_STRING, len + 1);
    memcpy(r, s + start, len);
    r[len] = '\0';
    return r;
}

const char* yona_Std_String__replace(const char* old, const char* new_s, const char* s) {
    size_t olen = strlen(old);
    size_t nlen = strlen(new_s);
    size_t slen = strlen(s);
    if (olen == 0) {
        size_t l = strlen(s);
        char* r = (char*)rc_alloc(RC_TYPE_STRING, l + 1);
        memcpy(r, s, l + 1);
        return r;
    }
    /* Count occurrences */
    size_t count = 0;
    const char* p = s;
    while ((p = strstr(p, old)) != NULL) { count++; p += olen; }
    /* Build result */
    size_t rlen = slen + count * (nlen - olen);
    char* r = (char*)rc_alloc(RC_TYPE_STRING, rlen + 1);
    char* w = r;
    p = s;
    while (*p) {
        if (strncmp(p, old, olen) == 0) {
            memcpy(w, new_s, nlen);
            w += nlen;
            p += olen;
        } else {
            *w++ = *p++;
        }
    }
    *w = '\0';
    return r;
}

int64_t* yona_Std_String__split(const char* delim, const char* s) {
    size_t dlen = strlen(delim);
    if (dlen == 0) {
        /* Split into individual characters */
        size_t slen = strlen(s);
        int64_t* result = yona_rt_seq_alloc(slen);
        for (size_t i = 0; i < slen; i++) {
            char* ch = (char*)rc_alloc(RC_TYPE_STRING, 2);
            ch[0] = s[i]; ch[1] = '\0';
            result[i + 1] = (int64_t)ch;
        }
        return result;
    }
    /* Count splits */
    size_t count = 1;
    const char* p = s;
    while ((p = strstr(p, delim)) != NULL) { count++; p += dlen; }
    int64_t* result = yona_rt_seq_alloc(count);
    p = s;
    for (size_t i = 0; i < count; i++) {
        const char* next = strstr(p, delim);
        size_t len = next ? (size_t)(next - p) : strlen(p);
        char* part = (char*)rc_alloc(RC_TYPE_STRING, len + 1);
        memcpy(part, p, len);
        part[len] = '\0';
        result[i + 1] = (int64_t)part;
        p = next ? next + dlen : p + len;
    }
    return result;
}

const char* yona_Std_String__join(const char* sep, int64_t* seq) {
    int64_t n = seq[0];
    if (n == 0) { char* r = (char*)rc_alloc(RC_TYPE_STRING, 1); r[0] = '\0'; return r; }
    size_t seplen = strlen(sep);
    /* Calculate total length */
    size_t total = 0;
    for (int64_t i = 0; i < n; i++) {
        const char* part = (const char*)seq[i + 1];
        total += strlen(part);
        if (i > 0) total += seplen;
    }
    char* r = (char*)rc_alloc(RC_TYPE_STRING, total + 1);
    char* w = r;
    for (int64_t i = 0; i < n; i++) {
        if (i > 0) { memcpy(w, sep, seplen); w += seplen; }
        const char* part = (const char*)seq[i + 1];
        size_t plen = strlen(part);
        memcpy(w, part, plen);
        w += plen;
    }
    *w = '\0';
    return r;
}

int64_t yona_Std_String__charAt(const char* s, int64_t idx) {
    size_t len = strlen(s);
    if (idx < 0 || (size_t)idx >= len) return 0;
    return (int64_t)(unsigned char)s[idx];
}

const char* yona_Std_String__padLeft(int64_t width, const char* pad, const char* s) {
    size_t slen = strlen(s);
    if ((int64_t)slen >= width) { char* r = (char*)rc_alloc(RC_TYPE_STRING, slen + 1); memcpy(r, s, slen + 1); return r; }
    size_t plen = strlen(pad);
    if (plen == 0) { char* r = (char*)rc_alloc(RC_TYPE_STRING, slen + 1); memcpy(r, s, slen + 1); return r; }
    char* r = (char*)rc_alloc(RC_TYPE_STRING, width + 1);
    size_t fill = width - slen;
    for (size_t i = 0; i < fill; i++) r[i] = pad[i % plen];
    memcpy(r + fill, s, slen + 1);
    return r;
}

const char* yona_Std_String__padRight(int64_t width, const char* pad, const char* s) {
    size_t slen = strlen(s);
    if ((int64_t)slen >= width) { char* r = (char*)rc_alloc(RC_TYPE_STRING, slen + 1); memcpy(r, s, slen + 1); return r; }
    size_t plen = strlen(pad);
    if (plen == 0) { char* r = (char*)rc_alloc(RC_TYPE_STRING, slen + 1); memcpy(r, s, slen + 1); return r; }
    char* r = (char*)rc_alloc(RC_TYPE_STRING, width + 1);
    memcpy(r, s, slen);
    size_t fill = width - slen;
    for (size_t i = 0; i < fill; i++) r[slen + i] = pad[i % plen];
    r[width] = '\0';
    return r;
}

const char* yona_Std_String__reverse(const char* s) {
    size_t len = strlen(s);
    char* r = (char*)rc_alloc(RC_TYPE_STRING, len + 1);
    for (size_t i = 0; i < len; i++) r[i] = s[len - 1 - i];
    r[len] = '\0';
    return r;
}

int64_t yona_Std_String__toInt(const char* s) { return (int64_t)atoll(s); }
double yona_Std_String__toFloat(const char* s) { return atof(s); }

int64_t yona_Std_String__isEmpty(const char* s) { return s[0] == '\0'; }

const char* yona_Std_String__repeat(int64_t n, const char* s) {
    size_t len = strlen(s);
    size_t total = len * (size_t)n;
    char* r = (char*)rc_alloc(RC_TYPE_STRING, total + 1);
    for (int64_t i = 0; i < n; i++) memcpy(r + i * len, s, len);
    r[total] = '\0';
    return r;
}

const char* yona_Std_String__take(int64_t n, const char* s) {
    size_t len = strlen(s);
    if ((size_t)n >= len) n = (int64_t)len;
    if (n < 0) n = 0;
    char* r = (char*)rc_alloc(RC_TYPE_STRING, (size_t)n + 1);
    memcpy(r, s, (size_t)n);
    r[n] = '\0';
    return r;
}

const char* yona_Std_String__drop(int64_t n, const char* s) {
    size_t len = strlen(s);
    if ((size_t)n >= len) return (const char*)rc_alloc(RC_TYPE_STRING, 1);
    if (n < 0) n = 0;
    size_t new_len = len - (size_t)n;
    char* r = (char*)rc_alloc(RC_TYPE_STRING, new_len + 1);
    memcpy(r, s + n, new_len + 1);
    return r;
}

int64_t yona_Std_String__count(const char* needle, const char* haystack) {
    if (needle[0] == '\0') return 0;
    int64_t count = 0;
    size_t nlen = strlen(needle);
    const char* p = haystack;
    while ((p = strstr(p, needle)) != NULL) { count++; p += nlen; }
    return count;
}

int64_t* yona_Std_String__lines(const char* s) {
    /* Split by newline */
    return yona_Std_String__split("\n", s);
}

const char* yona_Std_String__unlines(int64_t* seq) {
    return yona_Std_String__join("\n", seq);
}

int64_t* yona_Std_String__chars(const char* s) {
    size_t len = strlen(s);
    int64_t* seq = yona_rt_seq_alloc((int64_t)len);
    for (size_t i = 0; i < len; i++) seq[i + 1] = (int64_t)(unsigned char)s[i];
    return seq;
}

const char* yona_Std_String__fromChars(int64_t* seq) {
    int64_t len = seq[0];
    char* r = (char*)rc_alloc(RC_TYPE_STRING, (size_t)len + 1);
    for (int64_t i = 0; i < len; i++) r[i] = (char)seq[i + 1];
    r[len] = '\0';
    return r;
}

/* Std\Encoding — base64, hex, URL encoding */

static const char b64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

const char* yona_Std_Encoding__base64Encode(const char* s) {
    size_t len = strlen(s);
    size_t out_len = 4 * ((len + 2) / 3);
    char* r = (char*)rc_alloc(RC_TYPE_STRING, out_len + 1);
    size_t j = 0;
    for (size_t i = 0; i < len; i += 3) {
        uint32_t a = (uint8_t)s[i];
        uint32_t b = (i + 1 < len) ? (uint8_t)s[i + 1] : 0;
        uint32_t c = (i + 2 < len) ? (uint8_t)s[i + 2] : 0;
        uint32_t triple = (a << 16) | (b << 8) | c;
        r[j++] = b64_table[(triple >> 18) & 0x3F];
        r[j++] = b64_table[(triple >> 12) & 0x3F];
        r[j++] = (i + 1 < len) ? b64_table[(triple >> 6) & 0x3F] : '=';
        r[j++] = (i + 2 < len) ? b64_table[triple & 0x3F] : '=';
    }
    r[j] = '\0';
    return r;
}

static int b64_decode_char(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

const char* yona_Std_Encoding__base64Decode(const char* s) {
    size_t len = strlen(s);
    size_t out_len = 3 * len / 4;
    char* r = (char*)rc_alloc(RC_TYPE_STRING, out_len + 1);
    size_t j = 0;
    for (size_t i = 0; i < len; i += 4) {
        int a = b64_decode_char(s[i]);
        int b = (i + 1 < len) ? b64_decode_char(s[i + 1]) : 0;
        int c = (i + 2 < len) ? b64_decode_char(s[i + 2]) : 0;
        int d = (i + 3 < len) ? b64_decode_char(s[i + 3]) : 0;
        if (a < 0) a = 0; if (b < 0) b = 0;
        uint32_t triple = ((uint32_t)a << 18) | ((uint32_t)b << 12) | ((uint32_t)c << 6) | (uint32_t)d;
        r[j++] = (triple >> 16) & 0xFF;
        if (i + 2 < len && s[i + 2] != '=') r[j++] = (triple >> 8) & 0xFF;
        if (i + 3 < len && s[i + 3] != '=') r[j++] = triple & 0xFF;
    }
    r[j] = '\0';
    return r;
}

static const char hex_chars[] = "0123456789abcdef";

const char* yona_Std_Encoding__hexEncode(const char* s) {
    size_t len = strlen(s);
    char* r = (char*)rc_alloc(RC_TYPE_STRING, len * 2 + 1);
    for (size_t i = 0; i < len; i++) {
        r[i * 2] = hex_chars[((uint8_t)s[i] >> 4) & 0xF];
        r[i * 2 + 1] = hex_chars[(uint8_t)s[i] & 0xF];
    }
    r[len * 2] = '\0';
    return r;
}

static int hex_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
}

const char* yona_Std_Encoding__hexDecode(const char* s) {
    size_t len = strlen(s);
    size_t out_len = len / 2;
    char* r = (char*)rc_alloc(RC_TYPE_STRING, out_len + 1);
    for (size_t i = 0; i < out_len; i++)
        r[i] = (char)((hex_val(s[i * 2]) << 4) | hex_val(s[i * 2 + 1]));
    r[out_len] = '\0';
    return r;
}

const char* yona_Std_Encoding__urlEncode(const char* s) {
    size_t len = strlen(s);
    /* Worst case: every char is encoded as %XX (3x) */
    char* r = (char*)rc_alloc(RC_TYPE_STRING, len * 3 + 1);
    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)s[i];
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            r[j++] = c;
        } else {
            r[j++] = '%';
            r[j++] = hex_chars[(c >> 4) & 0xF];
            r[j++] = hex_chars[c & 0xF];
        }
    }
    r[j] = '\0';
    return r;
}

const char* yona_Std_Encoding__urlDecode(const char* s) {
    size_t len = strlen(s);
    char* r = (char*)rc_alloc(RC_TYPE_STRING, len + 1);
    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        if (s[i] == '%' && i + 2 < len) {
            r[j++] = (char)((hex_val(s[i + 1]) << 4) | hex_val(s[i + 2]));
            i += 2;
        } else if (s[i] == '+') {
            r[j++] = ' ';
        } else {
            r[j++] = s[i];
        }
    }
    r[j] = '\0';
    return r;
}

const char* yona_Std_Encoding__htmlEscape(const char* s) {
    size_t len = strlen(s);
    /* Worst case: every char becomes &amp; (5x) */
    char* r = (char*)rc_alloc(RC_TYPE_STRING, len * 6 + 1);
    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        switch (s[i]) {
            case '&':  memcpy(r + j, "&amp;", 5);  j += 5; break;
            case '<':  memcpy(r + j, "&lt;", 4);   j += 4; break;
            case '>':  memcpy(r + j, "&gt;", 4);   j += 4; break;
            case '"':  memcpy(r + j, "&quot;", 6); j += 6; break;
            case '\'': memcpy(r + j, "&#39;", 5);  j += 5; break;
            default:   r[j++] = s[i]; break;
        }
    }
    r[j] = '\0';
    return r;
}

/* ===== Platform I/O wrappers ===== */
/* Platform-specific implementations are in src/runtime/platform/ and
 * compiled as separate translation units. See CMakeLists.txt. */
#include "runtime/platform.h"

/* yona_rt_io_await is implemented in platform/file_linux.c (uses io_uring) */

/* Resource cleanup for `with` expression. Closes file descriptors,
 * sockets, or other resources based on the value type. */
void yona_rt_close(int64_t handle) {
    /* Only close valid-looking file descriptors (small positive integers).
     * Arbitrary i64 values (pointers, large ints) are not fds. */
    if (handle > 2 && handle < 65536)
        close((int)handle);
}

/* Std\IO */
void yona_Std_IO__print(const char* s)   { printf("%s", s); fflush(stdout); }
void yona_Std_IO__println(const char* s) { printf("%s\n", s); fflush(stdout); }
void yona_Std_IO__eprint(const char* s)  { fprintf(stderr, "%s", s); fflush(stderr); }
void yona_Std_IO__eprintln(const char* s){ fprintf(stderr, "%s\n", s); fflush(stderr); }
const char* yona_Std_IO__readLine(void)  { return yona_platform_read_line(); }
void yona_Std_IO__printInt(int64_t n)    { printf("%ld", n); fflush(stdout); }
void yona_Std_IO__printFloat(double f)   { printf("%g", f); fflush(stdout); }

/* Std\File — async ops return uring ID, sync ops return directly */
int64_t yona_Std_File__readFile(const char* path) {
    int64_t id = yona_platform_read_file_submit(path);
    if (id > 0) return id; /* uring ID = promise */
    /* Fallback: sync read, return the string pointer as i64 */
    return (int64_t)(intptr_t)yona_platform_read_file(path);
}
int64_t yona_Std_File__writeFile(const char* path, const char* content) {
    int64_t id = yona_platform_write_file_submit(path, content);
    if (id > 0) return id; /* uring ID = promise */
    return yona_platform_write_file(path, content) == 0 ? 1 : 0;
}
int64_t yona_Std_File__appendFile(const char* path, const char* content) {
    return yona_platform_append_file(path, content) == 0 ? 1 : 0;
}
int64_t yona_Std_File__exists(const char* path) {
    return yona_platform_file_exists(path);
}
int64_t yona_Std_File__remove(const char* path) {
    return yona_platform_remove_file(path) == 0 ? 1 : 0;
}
int64_t yona_Std_File__size(const char* path) {
    return yona_platform_file_size(path);
}
int64_t* yona_Std_File__listDir(const char* path) {
    return yona_platform_list_dir(path);
}

/* Binary file I/O */
int64_t yona_Std_File__readFileBytes(const char* path) {
    extern int64_t yona_platform_read_file_bytes_submit(const char* path);
    int64_t id = yona_platform_read_file_bytes_submit(path);
    if (id > 0) return id;
    /* Fallback: sync read to Bytes */
    extern void* yona_rt_bytes_from_string(const char* s);
    return (int64_t)(intptr_t)yona_rt_bytes_from_string(yona_platform_read_file(path));
}

int64_t yona_Std_File__writeFileBytes(const char* path, void* bytes) {
    int64_t* b = (int64_t*)bytes;
    int64_t len = b[0];
    uint8_t* data = (uint8_t*)(b + 1);
    /* Sync write for binary data */
    FILE* f = fopen(path, "wb");
    if (!f) return 0;
    size_t w = fwrite(data, 1, (size_t)len, f);
    fclose(f);
    return (w == (size_t)len) ? 1 : 0;
}

/* Std\Process */
const char* yona_Std_Process__getenv(const char* name) {
    return yona_platform_getenv(name);
}
const char* yona_Std_Process__getcwd(void) {
    return yona_platform_getcwd();
}
int64_t yona_Std_Process__exit(int64_t code) {
    exit((int)code);
    return 0; /* unreachable */
}

/* ===== Std\Http — HTTP client ===== */

/* Build an HTTP/1.1 request string */
const char* yona_Std_Http__buildRequest(const char* method, const char* host,
                                         const char* path, const char* body) {
    size_t body_len = body ? strlen(body) : 0;
    size_t host_len = strlen(host);
    size_t path_len = strlen(path);
    size_t method_len = strlen(method);

    /* Calculate total size */
    size_t total = method_len + 1 + path_len + 11 /* " HTTP/1.1\r\n" */
        + 6 + host_len + 2 /* "Host: ...\r\n" */
        + 24 /* "Connection: close\r\n" */
        + 19 /* "User-Agent: Yona\r\n" */;
    if (body_len > 0) {
        total += 32 /* "Content-Length: NNN\r\n" */
            + 48 /* "Content-Type: application/x-www-form-urlencoded\r\n" */
            + 2 + body_len;
    } else {
        total += 2; /* final \r\n */
    }

    char* r = (char*)rc_alloc(RC_TYPE_STRING, total + 64);
    int n;
    if (body_len > 0) {
        n = snprintf(r, total + 64,
            "%s %s HTTP/1.1\r\n"
            "Host: %s\r\n"
            "User-Agent: Yona\r\n"
            "Connection: close\r\n"
            "Content-Length: %zu\r\n"
            "Content-Type: application/x-www-form-urlencoded\r\n"
            "\r\n"
            "%s",
            method, path, host, body_len, body);
    } else {
        n = snprintf(r, total + 64,
            "%s %s HTTP/1.1\r\n"
            "Host: %s\r\n"
            "User-Agent: Yona\r\n"
            "Connection: close\r\n"
            "\r\n",
            method, path, host);
    }
    r[n] = '\0';
    return r;
}

/* Parse HTTP status code from response string */
int64_t yona_Std_Http__parseStatus(const char* response) {
    /* HTTP/1.1 200 OK\r\n... */
    if (strncmp(response, "HTTP/", 5) != 0) return 0;
    const char* sp = strchr(response, ' ');
    if (!sp) return 0;
    return (int64_t)atoi(sp + 1);
}

/* Extract HTTP response body (after \r\n\r\n) */
const char* yona_Std_Http__parseBody(const char* response) {
    const char* body = strstr(response, "\r\n\r\n");
    if (!body) {
        char* r = (char*)rc_alloc(RC_TYPE_STRING, 1);
        r[0] = '\0';
        return r;
    }
    body += 4;
    size_t len = strlen(body);
    char* r = (char*)rc_alloc(RC_TYPE_STRING, len + 1);
    memcpy(r, body, len + 1);
    return r;
}

/* Extract a specific HTTP header value */
const char* yona_Std_Http__getHeader(const char* name, const char* response) {
    size_t name_len = strlen(name);
    const char* p = response;
    while ((p = strstr(p, name)) != NULL) {
        /* Check it's at start of line */
        if (p == response || *(p - 1) == '\n') {
            p += name_len;
            if (*p == ':') {
                p++;
                while (*p == ' ') p++;
                const char* end = strstr(p, "\r\n");
                size_t len = end ? (size_t)(end - p) : strlen(p);
                char* r = (char*)rc_alloc(RC_TYPE_STRING, len + 1);
                memcpy(r, p, len);
                r[len] = '\0';
                return r;
            }
        }
        p++;
    }
    char* r = (char*)rc_alloc(RC_TYPE_STRING, 1);
    r[0] = '\0';
    return r;
}

/* Parse URL into (host, port, path) — returns seq of 3 elements [host, port, path] */
int64_t* yona_Std_Http__parseUrl(const char* url) {
    int64_t* result = yona_rt_seq_alloc(3);
    int port = 80;
    const char* host_start = url;
    const char* path_start = "/";

    /* Skip scheme */
    if (strncmp(url, "http://", 7) == 0) { host_start = url + 7; port = 80; }
    else if (strncmp(url, "https://", 8) == 0) { host_start = url + 8; port = 443; }

    /* Find path */
    const char* slash = strchr(host_start, '/');
    size_t host_len;
    if (slash) {
        host_len = (size_t)(slash - host_start);
        path_start = slash;
    } else {
        host_len = strlen(host_start);
    }

    /* Check for port in host */
    const char* colon = (const char*)memchr(host_start, ':', host_len);
    if (colon) {
        port = atoi(colon + 1);
        host_len = (size_t)(colon - host_start);
    }

    char* host = (char*)rc_alloc(RC_TYPE_STRING, host_len + 1);
    memcpy(host, host_start, host_len);
    host[host_len] = '\0';

    size_t path_len = strlen(path_start);
    char* path = (char*)rc_alloc(RC_TYPE_STRING, path_len + 1);
    memcpy(path, path_start, path_len + 1);

    result[1] = (int64_t)(intptr_t)host;
    result[2] = (int64_t)port;
    result[3] = (int64_t)(intptr_t)path;
    return result;
}

/* yona_Std_Http__httpGet is implemented in platform/net_linux.c (uses io_uring) */

/* Std\Random — pseudo-random number generation */

static int yona_random_initialized = 0;

static void yona_random_init(void) {
    if (!yona_random_initialized) {
        srand((unsigned)time(NULL));
        yona_random_initialized = 1;
    }
}

int64_t yona_Std_Random__int(int64_t lo, int64_t hi) {
    yona_random_init();
    if (lo >= hi) return lo;
    return lo + (int64_t)(rand() % (hi - lo + 1));
}

double yona_Std_Random__float(void) {
    yona_random_init();
    return (double)rand() / (double)RAND_MAX;
}

int64_t yona_Std_Random__choice(int64_t* seq) {
    yona_random_init();
    int64_t len = seq[0];
    if (len <= 0) return 0;
    return seq[1 + (int64_t)(rand() % len)];
}

int64_t* yona_Std_Random__shuffle(int64_t* seq) {
    yona_random_init();
    int64_t len = seq[0];
    int64_t* result = yona_rt_seq_alloc(len);
    memcpy(result + 1, seq + 1, len * sizeof(int64_t));
    /* Fisher-Yates shuffle */
    for (int64_t i = len - 1; i > 0; i--) {
        int64_t j = (int64_t)(rand() % (i + 1));
        int64_t tmp = result[1 + i];
        result[1 + i] = result[1 + j];
        result[1 + j] = tmp;
    }
    return result;
}

/* ===== Std\Time — time measurement and utilities ===== */

#include <sys/time.h>

/* Current time as epoch milliseconds */
int64_t yona_Std_Time__now(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000 + (int64_t)tv.tv_usec / 1000;
}

/* Current time as epoch microseconds */
int64_t yona_Std_Time__nowMicros(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000000 + (int64_t)tv.tv_usec;
}

/* Current time as epoch seconds */
int64_t yona_Std_Time__epoch(void) {
    return (int64_t)time(NULL);
}

/* Sleep for N milliseconds */
void yona_Std_Time__sleep(int64_t ms) {
    usleep((useconds_t)(ms * 1000));
}

/* Format epoch seconds as ISO 8601 string (YYYY-MM-DD HH:MM:SS) */
const char* yona_Std_Time__format(int64_t epoch_secs) {
    time_t t = (time_t)epoch_secs;
    struct tm* tm = localtime(&t);
    char* r = (char*)rc_alloc(RC_TYPE_STRING, 20);
    strftime(r, 20, "%Y-%m-%d %H:%M:%S", tm);
    return r;
}

/* Elapsed milliseconds between two now() timestamps */
int64_t yona_Std_Time__elapsed(int64_t start, int64_t end) {
    return end - start;
}

/* ===== Std\Path — file path manipulation ===== */

const char* yona_Std_Path__join(const char* a, const char* b) {
    size_t la = strlen(a), lb = strlen(b);
    /* Skip trailing slash on a */
    if (la > 0 && a[la-1] == '/') la--;
    /* Skip leading slash on b */
    const char* bs = b;
    if (lb > 0 && b[0] == '/') { bs++; lb--; }
    char* r = (char*)rc_alloc(RC_TYPE_STRING, la + 1 + lb + 1);
    memcpy(r, a, la);
    r[la] = '/';
    memcpy(r + la + 1, bs, lb);
    r[la + 1 + lb] = '\0';
    return r;
}

const char* yona_Std_Path__dirname(const char* path) {
    size_t len = strlen(path);
    /* Find last slash */
    const char* last = NULL;
    for (size_t i = 0; i < len; i++) if (path[i] == '/') last = path + i;
    if (!last) { char* r = (char*)rc_alloc(RC_TYPE_STRING, 2); r[0] = '.'; r[1] = '\0'; return r; }
    size_t dlen = (size_t)(last - path);
    if (dlen == 0) dlen = 1; /* root "/" */
    char* r = (char*)rc_alloc(RC_TYPE_STRING, dlen + 1);
    memcpy(r, path, dlen);
    r[dlen] = '\0';
    return r;
}

const char* yona_Std_Path__basename(const char* path) {
    size_t len = strlen(path);
    const char* last = path;
    for (size_t i = 0; i < len; i++) if (path[i] == '/') last = path + i + 1;
    size_t blen = strlen(last);
    char* r = (char*)rc_alloc(RC_TYPE_STRING, blen + 1);
    memcpy(r, last, blen + 1);
    return r;
}

const char* yona_Std_Path__extension(const char* path) {
    const char* base = path;
    size_t len = strlen(path);
    for (size_t i = 0; i < len; i++) if (path[i] == '/') base = path + i + 1;
    const char* dot = NULL;
    for (const char* p = base; *p; p++) if (*p == '.') dot = p;
    if (!dot || dot == base) { char* r = (char*)rc_alloc(RC_TYPE_STRING, 1); r[0] = '\0'; return r; }
    size_t elen = strlen(dot);
    char* r = (char*)rc_alloc(RC_TYPE_STRING, elen + 1);
    memcpy(r, dot, elen + 1);
    return r;
}

const char* yona_Std_Path__withExtension(const char* path, const char* ext) {
    /* Find last dot in basename */
    const char* base = path;
    size_t len = strlen(path);
    for (size_t i = 0; i < len; i++) if (path[i] == '/') base = path + i + 1;
    const char* dot = NULL;
    for (const char* p = base; *p; p++) if (*p == '.') dot = p;
    size_t stem_len = dot ? (size_t)(dot - path) : len;
    size_t elen = strlen(ext);
    char* r = (char*)rc_alloc(RC_TYPE_STRING, stem_len + elen + 1);
    memcpy(r, path, stem_len);
    memcpy(r + stem_len, ext, elen + 1);
    return r;
}

int64_t yona_Std_Path__isAbsolute(const char* path) {
    return path[0] == '/' ? 1 : 0;
}

/* ===== Std\FloatMath — floating-point math (wrappers for math.h) ===== */

double yona_Std_FloatMath__sqrt(double x)  { return sqrt(x); }
double yona_Std_FloatMath__sin(double x)   { return sin(x); }
double yona_Std_FloatMath__cos(double x)   { return cos(x); }
double yona_Std_FloatMath__tan(double x)   { return tan(x); }
double yona_Std_FloatMath__log(double x)   { return log(x); }
double yona_Std_FloatMath__exp(double x)   { return exp(x); }
double yona_Std_FloatMath__floor(double x) { return floor(x); }
double yona_Std_FloatMath__ceil(double x)  { return ceil(x); }
double yona_Std_FloatMath__round(double x) { return round(x); }
double yona_Std_FloatMath__pi(void)        { return 3.14159265358979323846; }

/* ===== Std\Format — string formatting ===== */

/* Simple placeholder format: replace {} with arguments in order.
 * Takes a format string and a sequence of string arguments. */
const char* yona_Std_Format__format(const char* fmt, int64_t* args) {
    int64_t argc = args[0];
    int64_t argi = 0;
    size_t flen = strlen(fmt);

    /* First pass: calculate output size */
    size_t out_size = 0;
    for (size_t i = 0; i < flen; i++) {
        if (fmt[i] == '{' && i + 1 < flen && fmt[i+1] == '}' && argi < argc) {
            const char* arg = (const char*)(intptr_t)args[argi + 1];
            out_size += strlen(arg);
            argi++;
            i++; /* skip } */
        } else {
            out_size++;
        }
    }

    /* Second pass: build output */
    char* r = (char*)rc_alloc(RC_TYPE_STRING, out_size + 1);
    argi = 0;
    size_t j = 0;
    for (size_t i = 0; i < flen; i++) {
        if (fmt[i] == '{' && i + 1 < flen && fmt[i+1] == '}' && argi < argc) {
            const char* arg = (const char*)(intptr_t)args[argi + 1];
            size_t alen = strlen(arg);
            memcpy(r + j, arg, alen);
            j += alen;
            argi++;
            i++;
        } else {
            r[j++] = fmt[i];
        }
    }
    r[j] = '\0';
    return r;
}

/* ===== Std\Json — minimal JSON parser/stringifier ===== */

/* JSON value types as tagged tuples:
 *   (:json_null)
 *   (:json_bool, 0|1)
 *   (:json_int, i64)
 *   (:json_string, ptr)
 *   (:json_array, seq_ptr)    — seq of json values
 *   (:json_object, seq_ptr)   — seq of (key, value) pairs
 */

/* Stringify a JSON-like structure to a string.
 * Takes a Yona value and produces a JSON string representation. */
const char* yona_Std_Json__stringify(int64_t value) {
    /* Simple: just convert the int to a string for now.
     * Full JSON stringify requires recursive ADT traversal which needs
     * the codegen to pass type info. For Phase 5, we provide basic
     * int/string/bool/seq serialization. */
    char* r = (char*)rc_alloc(RC_TYPE_STRING, 32);
    snprintf(r, 32, "%ld", value);
    return r;
}

const char* yona_Std_Json__stringifyString(const char* s) {
    size_t len = strlen(s);
    /* Worst case: every char needs escaping (\uXXXX = 6 chars) + quotes */
    char* r = (char*)rc_alloc(RC_TYPE_STRING, len * 6 + 3);
    size_t j = 0;
    r[j++] = '"';
    for (size_t i = 0; i < len; i++) {
        switch (s[i]) {
            case '"':  r[j++] = '\\'; r[j++] = '"';  break;
            case '\\': r[j++] = '\\'; r[j++] = '\\'; break;
            case '\n': r[j++] = '\\'; r[j++] = 'n';  break;
            case '\r': r[j++] = '\\'; r[j++] = 'r';  break;
            case '\t': r[j++] = '\\'; r[j++] = 't';  break;
            default:   r[j++] = s[i]; break;
        }
    }
    r[j++] = '"';
    r[j] = '\0';
    return r;
}

const char* yona_Std_Json__stringifyBool(int64_t b) {
    const char* src = b ? "true" : "false";
    size_t len = strlen(src);
    char* r = (char*)rc_alloc(RC_TYPE_STRING, len + 1);
    memcpy(r, src, len + 1);
    return r;
}

const char* yona_Std_Json__stringifyFloat(double f) {
    char* r = (char*)rc_alloc(RC_TYPE_STRING, 64);
    snprintf(r, 64, "%g", f);
    return r;
}

const char* yona_Std_Json__null(void) {
    char* r = (char*)rc_alloc(RC_TYPE_STRING, 5);
    memcpy(r, "null", 5);
    return r;
}

/* Parse a JSON number from string. Returns the integer value. */
int64_t yona_Std_Json__parseInt(const char* s) {
    return (int64_t)atoll(s);
}

double yona_Std_Json__parseFloat(const char* s) {
    return atof(s);
}

/* ===== Std\Crypto — hashing and random bytes ===== */

/* SHA-256 implementation (standalone, no openssl dependency) */
static const uint32_t sha256_k[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

#define SHA256_ROTR(x,n) (((x) >> (n)) | ((x) << (32-(n))))
#define SHA256_CH(x,y,z)  (((x) & (y)) ^ (~(x) & (z)))
#define SHA256_MAJ(x,y,z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define SHA256_EP0(x)  (SHA256_ROTR(x,2) ^ SHA256_ROTR(x,13) ^ SHA256_ROTR(x,22))
#define SHA256_EP1(x)  (SHA256_ROTR(x,6) ^ SHA256_ROTR(x,11) ^ SHA256_ROTR(x,25))
#define SHA256_SIG0(x) (SHA256_ROTR(x,7) ^ SHA256_ROTR(x,18) ^ ((x) >> 3))
#define SHA256_SIG1(x) (SHA256_ROTR(x,17) ^ SHA256_ROTR(x,19) ^ ((x) >> 10))

const char* yona_Std_Crypto__sha256(const char* input) {
    size_t len = strlen(input);
    /* Padding */
    size_t new_len = ((len + 8) / 64 + 1) * 64;
    uint8_t* msg = (uint8_t*)calloc(new_len, 1);
    memcpy(msg, input, len);
    msg[len] = 0x80;
    uint64_t bits = (uint64_t)len * 8;
    for (int i = 0; i < 8; i++) msg[new_len - 1 - i] = (uint8_t)(bits >> (i * 8));

    uint32_t h[8] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    };

    for (size_t chunk = 0; chunk < new_len; chunk += 64) {
        uint32_t w[64];
        for (int i = 0; i < 16; i++)
            w[i] = ((uint32_t)msg[chunk+i*4]<<24) | ((uint32_t)msg[chunk+i*4+1]<<16) |
                   ((uint32_t)msg[chunk+i*4+2]<<8) | (uint32_t)msg[chunk+i*4+3];
        for (int i = 16; i < 64; i++)
            w[i] = SHA256_SIG1(w[i-2]) + w[i-7] + SHA256_SIG0(w[i-15]) + w[i-16];

        uint32_t a=h[0],b=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],hh=h[7];
        for (int i = 0; i < 64; i++) {
            uint32_t t1 = hh + SHA256_EP1(e) + SHA256_CH(e,f,g) + sha256_k[i] + w[i];
            uint32_t t2 = SHA256_EP0(a) + SHA256_MAJ(a,b,c);
            hh=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
        }
        h[0]+=a; h[1]+=b; h[2]+=c; h[3]+=d; h[4]+=e; h[5]+=f; h[6]+=g; h[7]+=hh;
    }
    free(msg);

    char* r = (char*)rc_alloc(RC_TYPE_STRING, 65);
    for (int i = 0; i < 8; i++)
        snprintf(r + i*8, 9, "%08x", h[i]);
    return r;
}

const char* yona_Std_Crypto__randomBytes(int64_t n) {
    char* r = (char*)rc_alloc(RC_TYPE_STRING, (size_t)n + 1);
    FILE* f = fopen("/dev/urandom", "rb");
    if (f) { fread(r, 1, (size_t)n, f); fclose(f); }
    r[n] = '\0';
    return r;
}

const char* yona_Std_Crypto__randomHex(int64_t n) {
    char* bytes = (char*)malloc((size_t)n);
    FILE* f = fopen("/dev/urandom", "rb");
    if (f) { fread(bytes, 1, (size_t)n, f); fclose(f); }
    char* r = (char*)rc_alloc(RC_TYPE_STRING, (size_t)n * 2 + 1);
    static const char hex[] = "0123456789abcdef";
    for (int64_t i = 0; i < n; i++) {
        r[i*2]   = hex[((uint8_t)bytes[i] >> 4) & 0xF];
        r[i*2+1] = hex[(uint8_t)bytes[i] & 0xF];
    }
    r[n*2] = '\0';
    free(bytes);
    return r;
}

const char* yona_Std_Crypto__uuid4(void) {
    uint8_t bytes[16];
    FILE* f = fopen("/dev/urandom", "rb");
    if (f) { fread(bytes, 1, 16, f); fclose(f); }
    bytes[6] = (bytes[6] & 0x0f) | 0x40; /* version 4 */
    bytes[8] = (bytes[8] & 0x3f) | 0x80; /* variant 1 */
    char* r = (char*)rc_alloc(RC_TYPE_STRING, 37);
    snprintf(r, 37, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        bytes[0],bytes[1],bytes[2],bytes[3],bytes[4],bytes[5],bytes[6],bytes[7],
        bytes[8],bytes[9],bytes[10],bytes[11],bytes[12],bytes[13],bytes[14],bytes[15]);
    return r;
}

/* ===== Std\Log — structured logging ===== */

static int64_t yona_log_level = 1; /* 0=DEBUG, 1=INFO, 2=WARN, 3=ERROR */
static const char* yona_log_level_names[] = {"DEBUG", "INFO", "WARN", "ERROR"};

void yona_Std_Log__setLevel(int64_t level) { yona_log_level = level; }
int64_t yona_Std_Log__getLevel(void) { return yona_log_level; }

static void yona_log_emit(int64_t level, const char* msg) {
    if (level < yona_log_level) return;
    time_t now = time(NULL);
    struct tm* tm = localtime(&now);
    char ts[20];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", tm);
    fprintf(stderr, "[%s] %s: %s\n", ts, yona_log_level_names[level], msg);
    fflush(stderr);
}

void yona_Std_Log__debug(const char* msg) { yona_log_emit(0, msg); }
void yona_Std_Log__info(const char* msg)  { yona_log_emit(1, msg); }
void yona_Std_Log__warn(const char* msg)  { yona_log_emit(2, msg); }
void yona_Std_Log__error(const char* msg) { yona_log_emit(3, msg); }

/* Std\List */
int64_t yona_Std_List__length(int64_t* seq) { return seq[0]; }
int64_t yona_Std_List__head(int64_t* seq) { return seq[1]; }
int64_t* yona_Std_List__tail(int64_t* seq) { return yona_rt_seq_tail(seq); }
int64_t* yona_Std_List__reverse(int64_t* seq) {
    int64_t len = seq[0];
    int64_t* r = yona_rt_seq_alloc(len);
    for (int64_t i = 0; i < len; i++) r[i + 1] = seq[len - i];
    return r;
}

/* Std\Types — type conversions */
int64_t yona_Std_Types__toInt(const char* s) { return (int64_t)atoll(s); }
double yona_Std_Types__toFloat(const char* s) { return atof(s); }
const char* yona_Std_Types__intToString(int64_t n) {
    char* r = (char*)rc_alloc(RC_TYPE_STRING, 32);
    snprintf(r, 32, "%ld", n);
    return r;
}
const char* yona_Std_Types__floatToString(double f) {
    char* r = (char*)rc_alloc(RC_TYPE_STRING, 32);
    snprintf(r, 32, "%g", f);
    return r;
}
const char* yona_Std_Types__boolToString(int64_t b) {
    const char* src = b ? "true" : "false";
    size_t len = strlen(src);
    char* r = (char*)rc_alloc(RC_TYPE_STRING, len + 1);
    memcpy(r, src, len + 1);
    return r;
}

// Head: first element
// Head: first element (handles both cons cells and flat sequences)
int64_t yona_rt_seq_head(int64_t* seq) {
    return yona_rt_list_head((void*)seq);
}

// Tail: all elements except first (handles both cons cells and flat sequences)
int64_t* yona_rt_seq_tail(int64_t* seq) {
    return (int64_t*)yona_rt_list_tail((void*)seq);
}

/* ===== Async Runtime =====
 *
 * Fixed-size thread pool with work queue. Async functions submit tasks
 * to the pool and return a promise handle immediately (non-blocking).
 * Promises are awaited lazily at use sites via yona_rt_async_await.
 *
 * Parallel let is automatic: multiple async calls in a let block each
 * submit to the pool and return instantly. All tasks run concurrently.
 * Auto-await at use sites blocks only when the value is actually needed.
 */

#include <unistd.h>

#define YONA_POOL_SIZE 8

typedef struct {
    int64_t result;
    _Atomic int completed;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} yona_promise_t;

typedef int64_t (*yona_async_fn_t)(int64_t);
typedef int64_t (*yona_thunk_fn_t)(void);

typedef struct yona_task {
    yona_async_fn_t fn;     /* single-arg function (legacy) */
    yona_thunk_fn_t thunk;  /* zero-arg thunk (multi-arg via closure) */
    int64_t arg;
    yona_promise_t* promise;
    struct yona_task* next;
} yona_task_t;

/* Thread pool state */
static pthread_t yona_pool_threads[YONA_POOL_SIZE];
static yona_task_t* yona_task_head = NULL;
static yona_task_t* yona_task_tail = NULL;
static pthread_mutex_t yona_pool_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t yona_pool_cond = PTHREAD_COND_INITIALIZER;
static _Atomic int yona_pool_initialized = 0;

static void* yona_pool_worker(void* unused) {
    (void)unused;
    while (1) {
        pthread_mutex_lock(&yona_pool_mutex);
        while (!yona_task_head) {
            pthread_cond_wait(&yona_pool_cond, &yona_pool_mutex);
        }
        yona_task_t* task = yona_task_head;
        yona_task_head = task->next;
        if (!yona_task_head) yona_task_tail = NULL;
        pthread_mutex_unlock(&yona_pool_mutex);

        /* Execute the task — either single-arg or thunk mode */
        int64_t result = task->thunk ? task->thunk() : task->fn(task->arg);

        /* Fulfill the promise */
        pthread_mutex_lock(&task->promise->mutex);
        task->promise->result = result;
        task->promise->completed = 1;
        pthread_cond_signal(&task->promise->cond);
        pthread_mutex_unlock(&task->promise->mutex);

        free(task);
    }
    return NULL;
}

static void yona_pool_init(void) {
    if (yona_pool_initialized) return;
    yona_pool_initialized = 1;
    for (int i = 0; i < YONA_POOL_SIZE; i++) {
        pthread_create(&yona_pool_threads[i], NULL, yona_pool_worker, NULL);
        /* Detach — pool threads run for the lifetime of the process */
        pthread_detach(yona_pool_threads[i]);
    }
}

/* Generic async: takes a thunk (zero-arg function returning i64).
 * The codegen generates thunks that capture multi-arg function calls. */
typedef int64_t (*yona_thunk_t)(void);

typedef struct yona_thunk_task {
    yona_thunk_t thunk;
    yona_promise_t* promise;
    struct yona_thunk_task* next;
} yona_thunk_task_t;

static void* yona_pool_worker_thunk(void* unused);

yona_promise_t* yona_rt_async_call_thunk(yona_thunk_t thunk) {
    yona_pool_init();

    yona_promise_t* promise = (yona_promise_t*)malloc(sizeof(yona_promise_t));
    promise->result = 0;
    promise->completed = 0;
    pthread_mutex_init(&promise->mutex, NULL);
    pthread_cond_init(&promise->cond, NULL);

    yona_task_t* task = (yona_task_t*)malloc(sizeof(yona_task_t));
    task->fn = NULL;
    task->thunk = thunk;
    task->arg = 0;
    task->promise = promise;
    task->next = NULL;

    pthread_mutex_lock(&yona_pool_mutex);
    if (yona_task_tail) {
        yona_task_tail->next = task;
    } else {
        yona_task_head = task;
    }
    yona_task_tail = task;
    pthread_cond_signal(&yona_pool_cond);
    pthread_mutex_unlock(&yona_pool_mutex);

    return promise;
}

/* Submit async work to thread pool. Returns promise immediately (non-blocking). */
yona_promise_t* yona_rt_async_call(yona_async_fn_t fn, int64_t arg) {
    yona_pool_init();

    yona_promise_t* promise = (yona_promise_t*)malloc(sizeof(yona_promise_t));
    promise->result = 0;
    promise->completed = 0;
    pthread_mutex_init(&promise->mutex, NULL);
    pthread_cond_init(&promise->cond, NULL);

    yona_task_t* task = (yona_task_t*)malloc(sizeof(yona_task_t));
    task->fn = fn;
    task->thunk = NULL;
    task->arg = arg;
    task->promise = promise;
    task->next = NULL;

    pthread_mutex_lock(&yona_pool_mutex);
    if (yona_task_tail) {
        yona_task_tail->next = task;
    } else {
        yona_task_head = task;
    }
    yona_task_tail = task;
    pthread_cond_signal(&yona_pool_cond);
    pthread_mutex_unlock(&yona_pool_mutex);

    return promise; /* Returns immediately — non-blocking */
}

/* Await: block until promise completes, return result, free promise. */
int64_t yona_rt_async_await(yona_promise_t* promise) {
    pthread_mutex_lock(&promise->mutex);
    while (!promise->completed) {
        pthread_cond_wait(&promise->cond, &promise->mutex);
    }
    int64_t result = promise->result;
    pthread_mutex_unlock(&promise->mutex);

    pthread_mutex_destroy(&promise->mutex);
    pthread_cond_destroy(&promise->cond);
    free(promise);

    return result;
}

/* Test helper: async function that sleeps for N milliseconds then returns N */
int64_t yona_test_slow_identity(int64_t ms) {
    usleep((useconds_t)(ms * 1000));
    return ms;
}

/* Test helper: multi-arg async function — adds two numbers with a delay */
int64_t yona_test_slow_add(int64_t a, int64_t b) {
    usleep(10000); /* 10ms */
    return a + b;
}

/* ===== Partial Application (Closures) ===== */
/* A closure captures a function pointer and one partial argument.
 * When called with the remaining argument, it invokes the original
 * function with both args.
 *
 * For functions with arity > 2, closures chain:
 * add 1 → closure(add, 1) → call with 2 → closure(add_1, 2) → call with 3 → add(1,2,3)
 */

typedef struct {
    int64_t (*fn2)(int64_t, int64_t);  /* original 2-arg function */
    int64_t captured;                   /* first captured argument */
} yona_closure2_t;

typedef struct {
    int64_t (*fn3)(int64_t, int64_t, int64_t);
    int64_t captured1;
    int64_t captured2;
} yona_closure3_t;

/* Apply a 2-arg closure: closure was created from fn(captured, ?), now called with arg */
int64_t yona_rt_closure2_apply(int64_t closure_ptr, int64_t arg) {
    yona_closure2_t* c = (yona_closure2_t*)(void*)closure_ptr;
    int64_t result = c->fn2(c->captured, arg);
    yona_rt_rc_dec(c);
    return result;
}

/* Create a closure from a 2-arg function with 1 captured arg */
int64_t yona_rt_closure2_create(int64_t (*fn)(int64_t, int64_t), int64_t captured) {
    yona_closure2_t* c = (yona_closure2_t*)rc_alloc(RC_TYPE_CLOSURE, sizeof(yona_closure2_t));
    c->fn2 = fn;
    c->captured = captured;
    return (int64_t)(void*)c;
}

/* ===== General Closures (env-passing) ===== */
/* Layout: int64_t array [fn_ptr, ret_type_tag, arity, cap0, cap1, ...]
 * The function takes (void* env, args...) where env is the closure itself.
 * Slot 0: function pointer (as int64_t)
 * Slot 1: return CType tag (INT=0, FLOAT=1, ..., ADT=12)
 * Slot 2: number of user arguments (excluding env)
 * Captures are stored starting at index 3.
 */

void* yona_rt_closure_create(void* fn_ptr, int64_t ret_type, int64_t arity, int64_t num_captures) {
    int64_t* closure = (int64_t*)rc_alloc(RC_TYPE_CLOSURE, (3 + num_captures) * sizeof(int64_t));
    closure[0] = (int64_t)(intptr_t)fn_ptr;
    closure[1] = ret_type;
    closure[2] = arity;
    return closure;
}

void yona_rt_closure_set_cap(void* closure, int64_t idx, int64_t val) {
    ((int64_t*)closure)[3 + idx] = val;
}

int64_t yona_rt_closure_get_cap(void* closure, int64_t idx) {
    return ((int64_t*)closure)[3 + idx];
}
