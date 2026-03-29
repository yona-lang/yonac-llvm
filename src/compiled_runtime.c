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
static void* rc_alloc(int64_t type_tag, size_t payload_bytes) {
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
    return seq[0];
}

int64_t yona_rt_seq_get(int64_t* seq, int64_t index) {
    return seq[index + 1];
}

void yona_rt_seq_set(int64_t* seq, int64_t index, int64_t value) {
    seq[index + 1] = value;
}

void yona_rt_print_seq(int64_t* seq) {
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
    int64_t old_len = seq[0];
    int64_t* result = yona_rt_seq_alloc(old_len + 1);
    result[1] = elem;
    memcpy(result + 2, seq + 1, old_len * sizeof(int64_t));
    return result;
}

// Join: concatenate two sequences
int64_t* yona_rt_seq_join(int64_t* a, int64_t* b) {
    int64_t len_a = a[0];
    int64_t len_b = b[0];
    int64_t* result = yona_rt_seq_alloc(len_a + len_b);
    memcpy(result + 1, a + 1, len_a * sizeof(int64_t));
    memcpy(result + 1 + len_a, b + 1, len_b * sizeof(int64_t));
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
int64_t yona_rt_seq_head(int64_t* seq) {
    return seq[1];
}

// Tail: all elements except first
int64_t* yona_rt_seq_tail(int64_t* seq) {
    int64_t old_len = seq[0];
    if (old_len <= 0) return yona_rt_seq_alloc(0);
    int64_t* result = yona_rt_seq_alloc(old_len - 1);
    memcpy(result + 1, seq + 2, (old_len - 1) * sizeof(int64_t));
    return result;
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
