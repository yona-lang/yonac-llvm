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

/* ===== Size-class pool allocator ===== */
/* Free-list pools for common allocation sizes. Avoids malloc/free overhead
 * for frequently allocated objects (closures, seq chunks, small ADTs).
 * Each pool is a linked list of freed blocks, reused on next alloc. */

#define POOL_CLASSES 5
static const size_t pool_sizes[POOL_CLASSES] = {
    32,   /* small closures (5-slot header) */
    64,   /* small ADTs, tuples, flat seqs */
    128,  /* medium closures with captures */
    296,  /* RBT trie nodes, leaves, head chunks (272-296 bytes with RC header) */
    608,  /* rbt_t root struct (592 bytes payload + 16 RC header) */
};

typedef struct pool_block {
    struct pool_block* next;
} pool_block_t;

static __thread pool_block_t* pool_freelist[POOL_CLASSES] = {0};

static int pool_class_for(size_t total_bytes) {
    for (int i = 0; i < POOL_CLASSES; i++) {
        if (total_bytes <= pool_sizes[i]) return i;
    }
    return -1; /* too large for pools */
}

/* Slab allocator: instead of individual malloc per block, allocate slabs
 * of many blocks at once. Blocks within a slab never go through glibc's
 * free/tcache, eliminating the tcache corruption issue. */
#define SLAB_BLOCKS 256  /* blocks per slab */

typedef struct slab {
    struct slab* next;
    char data[];  /* flexible array of SLAB_BLOCKS * block_size bytes */
} slab_t;

static __thread slab_t* pool_slabs[POOL_CLASSES] = {0};

static void pool_grow(int cls) {
    size_t block_size = pool_sizes[cls];
    slab_t* slab = (slab_t*)malloc(sizeof(slab_t) + SLAB_BLOCKS * block_size);
    slab->next = pool_slabs[cls];
    pool_slabs[cls] = slab;
    /* Link all blocks in the slab into the free list */
    for (int i = 0; i < SLAB_BLOCKS; i++) {
        pool_block_t* block = (pool_block_t*)(slab->data + i * block_size);
        block->next = pool_freelist[cls];
        pool_freelist[cls] = block;
    }
}

static void* pool_alloc(size_t total_bytes) {
    int cls = pool_class_for(total_bytes);
    if (cls < 0) return malloc(total_bytes);
    if (!pool_freelist[cls]) pool_grow(cls);
    pool_block_t* block = pool_freelist[cls];
    pool_freelist[cls] = block->next;
    return (void*)block;
}

static void pool_free(void* ptr, size_t total_bytes) {
    if (!ptr) return;
    int cls = pool_class_for(total_bytes);
    if (cls < 0) { free(ptr); return; }
    pool_block_t* block = (pool_block_t*)ptr;
    block->next = pool_freelist[cls];
    pool_freelist[cls] = block;
}

/* Internal: allocate with RC header, returns pointer to payload.
 * Header: [refcount, type_tag_and_pool_class, ...payload...]
 *                                              ^-- returned pointer
 * Pool class is encoded in the upper bits of the type_tag word:
 *   bits 0-7:  type tag (RC_TYPE_SEQ, etc.)
 *   bits 8-15: pool class index + 1 (0 = not pooled, 1-4 = class 0-3)
 * This avoids a 3rd header word while supporting pool_free. */
#define RC_HEADER_SIZE 2

#define ENCODE_TAG(tag, cls) ((tag) | (((int64_t)(cls) + 1) << 8))
#define DECODE_TAG(encoded) ((encoded) & 0xFF)
#define DECODE_POOL_CLASS(encoded) ((int)((encoded) >> 8) - 1)

void* rc_alloc(int64_t type_tag, size_t payload_bytes) {
    size_t total = RC_HEADER_SIZE * sizeof(int64_t) + payload_bytes;
    int cls = pool_class_for(total);
    int64_t* raw = (int64_t*)pool_alloc(total);
    raw[0] = 1;         /* refcount = 1 */
    raw[1] = ENCODE_TAG(type_tag, cls);  /* type tag + pool class */
    return (void*)(raw + RC_HEADER_SIZE);
}

/* Public: increment refcount (atomic for thread safety) */
void yona_rt_rc_inc(void* ptr) {
    if (__builtin_expect(!ptr, 0)) return;
    int64_t* header = ((int64_t*)ptr) - RC_HEADER_SIZE;
    __atomic_fetch_add(&header[0], 1, __ATOMIC_RELAXED);
}

/* Sentinel refcount for arena-allocated objects. rc_dec skips these. */
#define RC_ARENA_SENTINEL INT64_MAX

/* Forward declarations for recursive RC types */
#define RC_TYPE_CHUNKED_FWD 11  /* legacy chunked seq (unused, kept for tag safety) */
#define RC_TYPE_RBT_FWD      12 /* RBT seq root struct */
#define RC_TYPE_RBT_NODE_FWD 13 /* RBT internal node (32 child pointers) */
#define RC_TYPE_RBT_LEAF_FWD 14 /* RBT leaf node (heap_flag + 32 elements) */

/* Public: decrement refcount; free when it reaches 0.
 * Recursively rc_dec pointer-typed children for known container types. */
void yona_rt_rc_dec(void* ptr) {
    if (__builtin_expect(!ptr, 0)) return;
    int64_t* header = ((int64_t*)ptr) - RC_HEADER_SIZE;
    int64_t rc = __atomic_load_n(&header[0], __ATOMIC_RELAXED);
    if (__builtin_expect(rc == RC_ARENA_SENTINEL, 0)) return;
    int64_t old = __atomic_fetch_sub(&header[0], 1, __ATOMIC_ACQ_REL);
    if (__builtin_expect(old <= 1, 0)) {
        int64_t encoded_tag = header[1];
        int64_t type_tag = DECODE_TAG(encoded_tag);
        int pool_cls = DECODE_POOL_CLASS(encoded_tag);
        int64_t* payload = (int64_t*)ptr;

        if (type_tag == RC_TYPE_SEQ) {
            /* Flat seq: rc_dec elements if heap_flag set.
             * Layout: [count, heap_flag, elem0, ...] */
            int64_t count = payload[0];
            int64_t heap_flag = payload[1];
            if (heap_flag) {
                for (int64_t i = 0; i < count; i++) {
                    int64_t val = payload[2 + i];
                    if (val) yona_rt_rc_dec((void*)(intptr_t)val);
                }
            }
        } else if (type_tag == RC_TYPE_CHUNKED_FWD) {
            /* Legacy chunked seq: rc_dec next chunk pointer */
            typedef struct { int64_t tl, off, cnt; int64_t e[32]; void* next; } chunk_layout_t;
            chunk_layout_t* chunk = (chunk_layout_t*)ptr;
            if (chunk->next) yona_rt_rc_dec(chunk->next);
        } else if (type_tag == RC_TYPE_RBT_FWD) {
            /* RBT seq root: rc_dec elements in head/tail buffers, head chain, trie.
             * Layout matches rbt_t in seq.c:
             *   [0] length, [1] heap_flag, [2] head_off, [3] head_cnt,
             *   [4..35] head_buf[32],
             *   [36] head_next (ptr), [37] head_chain_len,
             *   [38] tail_cnt, [39..70] tail_buf[32],
             *   [71] back_shift, [72] back_root (ptr),
             *   [73] back_size, [74] back_off */
            int64_t hf = payload[1];
            int64_t head_off = payload[2];
            int64_t head_cnt = payload[3];
            int64_t tail_cnt = payload[38];
            if (hf) {
                for (int64_t i = 0; i < head_cnt; i++) {
                    int64_t v = payload[4 + head_off + i];
                    if (v) yona_rt_rc_dec((void*)(intptr_t)v);
                }
                for (int64_t i = 0; i < tail_cnt; i++) {
                    int64_t v = payload[39 + i];
                    if (v) yona_rt_rc_dec((void*)(intptr_t)v);
                }
            }
            /* Walk head chain and rc_dec elements if heap-typed */
            void* head_next = *(void**)&payload[36];
            if (hf && head_next) {
                void* cur = head_next;
                while (cur) {
                    int64_t* cp = (int64_t*)cur;
                    int64_t coff = cp[0], ccnt = cp[1];
                    for (int64_t i = 0; i < ccnt; i++) {
                        int64_t v = cp[2 + coff + i];
                        if (v) yona_rt_rc_dec((void*)(intptr_t)v);
                    }
                    cur = *(void**)&cp[2 + 32];
                }
            }
            if (head_next) yona_rt_rc_dec(head_next);
            void* back_root = *(void**)&payload[72];
            if (back_root) yona_rt_rc_dec(back_root);
        } else if (type_tag == RC_TYPE_RBT_NODE_FWD) {
            /* RBT internal node: rc_dec all non-null children. */
            for (int i = 0; i < 32; i++) {
                int64_t child = payload[i];
                if (child) yona_rt_rc_dec((void*)(intptr_t)child);
            }
        } else if (type_tag == 15 /* RC_TYPE_RBT_CHUNK */) {
            /* RBT head chain chunk: [offset, count, elems[32], next_ptr]
             * rc_dec next chunk. Elements are rc_dec'd by the rbt_t destructor
             * when walking the chain (it knows the heap_flag). */
            /* Note: for simplicity, don't rc_dec elements here. The rbt_t
             * destructor walks the head chain and handles element cleanup.
             * But if a chunk is freed independently (e.g. after tail detaches
             * it), we must handle elements. We use a conservative approach:
             * always rc_dec the next pointer. */
            void* next = *(void**)&payload[2 + 32]; /* after offset, count, elems[32] */
            if (next) yona_rt_rc_dec(next);
        } else if (type_tag == RC_TYPE_RBT_LEAF_FWD) {
            /* RBT leaf node: rc_dec elements if heap_flag set.
             * Layout: [heap_flag, elem0, ..., elem31] */
            int64_t hf = payload[0];
            if (hf) {
                for (int i = 0; i < 32; i++) {
                    int64_t v = payload[1 + i];
                    if (v) yona_rt_rc_dec((void*)(intptr_t)v);
                }
            }
        } else if (type_tag == RC_TYPE_CLOSURE) {
            /* Closure: rc_dec heap-typed captures using the heap_mask.
             * Layout: [fn_ptr, ret_type, arity, num_captures, heap_mask, cap0, ...] */
            int64_t num_caps = payload[3];
            int64_t heap_mask = payload[4];
            for (int64_t ci = 0; ci < num_caps && ci < 64; ci++) {
                if (heap_mask & ((int64_t)1 << ci)) {
                    int64_t cap_val = payload[5 + ci];
                    if (cap_val) yona_rt_rc_dec((void*)(intptr_t)cap_val);
                }
            }
        } else if (type_tag == RC_TYPE_ADT) {
            /* ADT: rc_dec heap-typed fields using heap_mask.
             * Layout: [tag, num_fields, heap_mask, field0, ...] */
            int64_t num_fields = payload[1];
            int64_t heap_mask = payload[2];
            for (int64_t fi = 0; fi < num_fields && fi < 64; fi++) {
                if (heap_mask & ((int64_t)1 << fi)) {
                    int64_t field_val = payload[3 + fi];
                    if (field_val) yona_rt_rc_dec((void*)(intptr_t)field_val);
                }
            }
        } else if (type_tag == RC_TYPE_DICT) {
            /* HAMT node: rc_dec child sub-nodes.
             * Children are at payload[data_count*2 + i] where
             * data_count = popcount(payload[0] & 0xFFFFFFFF) (datamap).
             * node_count = popcount(payload[1] & 0xFFFFFFFF) (nodemap). */
            int dc = __builtin_popcountll((uint64_t)payload[0]);
            int nc = __builtin_popcountll((uint64_t)payload[1]);
            for (int i = 0; i < nc; i++) {
                int64_t child = payload[3 + dc * 2 + i];
                if (child) yona_rt_rc_dec((void*)(intptr_t)child);
            }
        } else if (type_tag == RC_TYPE_SET) {
            /* Set: rc_dec all elements if heap_flag is set.
             * Layout: [count, heap_flag, elem0, ...] */
            int64_t count = payload[0];
            int64_t heap_flag = payload[1];
            if (heap_flag) {
                for (int64_t i = 0; i < count; i++) {
                    int64_t val = payload[2 + i];
                    if (val) yona_rt_rc_dec((void*)(intptr_t)val);
                }
            }
        } else if (type_tag == 9 /* RC_TYPE_TUPLE */) {
            /* Tuple: rc_dec heap-typed elements using heap_mask.
             * Layout: [num_elements, heap_mask, elem0, ...] */
            int64_t num_elems = payload[0];
            int64_t heap_mask = payload[1];
            for (int64_t ei = 0; ei < num_elems && ei < 64; ei++) {
                if (heap_mask & ((int64_t)1 << ei)) {
                    int64_t elem_val = payload[2 + ei];
                    if (elem_val) yona_rt_rc_dec((void*)(intptr_t)elem_val);
                }
            }
        } else if (type_tag == 17 /* RC_TYPE_PROCESS */) {
            /* Process handle: close pipe fds, reap zombie.
             * yona_process_destroy is defined in os_linux.c. */
            extern void yona_process_destroy(void* proc) __attribute__((weak));
            if (yona_process_destroy) yona_process_destroy(ptr);
        } else if (type_tag == 16 /* RC_TYPE_REGEX */) {
            /* Regex handle: free the PCRE2 compiled pattern.
             * Layout: [pcre2_code* code]
             * yona_regex_free_code is a weak symbol — if regex.c isn't
             * linked (PCRE2 unavailable), this is a no-op. */
            void* code = *(void**)payload;
            if (code) {
                extern void yona_regex_free_code(void* code) __attribute__((weak));
                if (yona_regex_free_code)
                    yona_regex_free_code(code);
            }
        }
        if (pool_cls >= 0)
            pool_free(header, pool_sizes[pool_cls]);
        else
            free(header);
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
    size_t total = RC_HEADER_SIZE * sizeof(int64_t) + (size_t)payload_bytes;
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
    return (void*)(raw + RC_HEADER_SIZE);  /* return pointer past header */
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
/* ===== Persistent Seq ===== */
#include "runtime/seq.c"

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

/* Tuple: heap-allocate i64 array with element metadata.
 * Layout: [num_elements, heap_mask, elem0, elem1, ...]
 * Uses RC_TYPE_TUPLE (9) for recursive destruction. */
#define RC_TYPE_TUPLE 9

void* yona_rt_tuple_alloc(int64_t num_elements) {
    int64_t* tuple = (int64_t*)rc_alloc(RC_TYPE_TUPLE, (2 + num_elements) * sizeof(int64_t));
    tuple[0] = num_elements;
    tuple[1] = 0; /* heap_mask */
    return tuple;
}

void yona_rt_tuple_set_heap_mask(void* tuple, int64_t mask) {
    ((int64_t*)tuple)[1] = mask;
}

void yona_rt_tuple_set(void* tuple, int64_t index, int64_t value) {
    ((int64_t*)tuple)[2 + index] = value;
}

int64_t yona_rt_tuple_get(void* tuple, int64_t index) {
    return ((int64_t*)tuple)[2 + index];
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

/* Seq functions are in runtime/seq.c (included above) */

/* ===== Symbol runtime ===== */
/* Symbols are interned to i64 IDs at compile time. Comparison is icmp eq. */
/* Print takes the string name (resolved by the compiler from the symbol table). */

void yona_rt_print_symbol(const char* name) {
    printf(":%s", name);
}

/* ===== Set runtime — legacy flat array ===== */
/* Flat set layout: [count, heap_flag, elem0, elem1, ...]
 * Used only during literal construction. HAMT-based operations below. */

int64_t* yona_rt_set_alloc(int64_t count) {
    int64_t* set = (int64_t*)rc_alloc(RC_TYPE_SET, (count + 2) * sizeof(int64_t));
    set[0] = count;
    set[1] = 0;
    return set;
}

void yona_rt_set_set_heap(int64_t* set, int64_t flag) {
    set[1] = flag;
}

void yona_rt_set_put(int64_t* set, int64_t index, int64_t value) {
    set[index + 2] = value;
}

/* ===== Dict runtime (HAMT-based) ===== */
/* Persistent hash map using Hash Array Mapped Trie.
 * O(1) amortized lookup/insert, structural sharing for immutability. */
#include "runtime/hamt.c"

/* Legacy API wrappers — codegen uses these names.
 * dict_alloc creates empty, dict_set does persistent put (returns new dict). */

int64_t* yona_rt_dict_alloc(int64_t count) {
    (void)count;
    return (int64_t*)yona_rt_hamt_empty();
}

void yona_rt_dict_set_heap(int64_t* dict, int64_t key_heap, int64_t val_heap) {
    (void)dict; (void)key_heap; (void)val_heap;
    /* HAMT handles RC via its tree structure. No-op for HAMT. */
}

/* Persistent insert: returns NEW dict. Old dict is unchanged.
 * Handles empty set {} (RC_TYPE_SET) gracefully — creates fresh HAMT. */
int64_t* yona_rt_dict_put(int64_t* dict, int64_t key, int64_t value) {
    /* Check if the input is actually a SET (empty {} parsed as SetExpr) */
    if (dict) {
        int64_t* header = dict - RC_HEADER_SIZE;
        int64_t tag = DECODE_TAG(header[1]);
        if (tag == RC_TYPE_SET) {
            /* Empty set — treat as empty dict */
            dict = (int64_t*)yona_rt_hamt_empty();
        }
    }
    return (int64_t*)yona_rt_hamt_put((hamt_node_t*)dict, key, value);
}

/* Lookup: returns value for key, or default_val if not found. */
int64_t yona_rt_dict_get(int64_t* dict, int64_t key, int64_t default_val) {
    return yona_rt_hamt_get((hamt_node_t*)dict, key, default_val);
}

int64_t yona_rt_dict_size(int64_t* dict) {
    return yona_rt_hamt_size((hamt_node_t*)dict);
}

int64_t yona_rt_dict_contains(int64_t* dict, int64_t key) {
    return yona_rt_hamt_contains((hamt_node_t*)dict, key);
}

int64_t* yona_rt_dict_keys(int64_t* dict) {
    return yona_rt_hamt_keys((hamt_node_t*)dict);
}

/* Legacy dict_set — for codegen compatibility. Mutates in place (only safe
 * during construction when refcount=1). NOT persistent. */
void yona_rt_dict_set(int64_t* dict, int64_t index, int64_t key, int64_t value) {
    /* For HAMT, we can't mutate. This is only called during dict literal
     * construction, so we use the legacy flat path. The codegen should be
     * updated to use dict_put for persistent inserts. For now, this is a
     * compatibility shim that puts entries into the HAMT. */
    (void)index; /* index not meaningful for HAMT */
    /* This doesn't work with HAMT — codegen must use dict_put instead. */
}

void yona_rt_print_dict(int64_t* dict) {
    yona_rt_hamt_print((hamt_node_t*)dict);
}

/* ===== Set runtime — HAMT-based persistent operations ===== */
/* Uses the same HAMT as dicts. Elements stored as keys with value=1. */

static hamt_node_t* set_ensure_hamt(int64_t* set) {
    if (!set) return yona_rt_hamt_empty();
    int64_t* header = set - RC_HEADER_SIZE;
    int64_t tag = DECODE_TAG(header[1]);
    if (tag == RC_TYPE_DICT) return (hamt_node_t*)set;
    hamt_node_t* h = yona_rt_hamt_empty();
    int64_t count = set[0];
    for (int64_t i = 0; i < count; i++)
        h = yona_rt_hamt_put(h, set[i + 2], 1);
    return h;
}

int64_t* yona_rt_set_insert(int64_t* set, int64_t elem) {
    return (int64_t*)yona_rt_hamt_put(set_ensure_hamt(set), elem, 1);
}

int64_t yona_rt_set_contains(int64_t* set, int64_t elem) {
    if (!set) return 0;
    int64_t* header = set - RC_HEADER_SIZE;
    int64_t tag = DECODE_TAG(header[1]);
    if (tag == RC_TYPE_DICT) return yona_rt_hamt_contains((hamt_node_t*)set, elem);
    int64_t count = set[0];
    for (int64_t i = 0; i < count; i++)
        if (set[i + 2] == elem) return 1;
    return 0;
}

int64_t yona_rt_set_size(int64_t* set) {
    if (!set) return 0;
    int64_t* header = set - RC_HEADER_SIZE;
    int64_t tag = DECODE_TAG(header[1]);
    if (tag == RC_TYPE_DICT) return yona_rt_hamt_size((hamt_node_t*)set);
    return set[0];
}

int64_t* yona_rt_set_elements(int64_t* set) {
    if (!set) return yona_rt_seq_alloc(0);
    int64_t* header = set - RC_HEADER_SIZE;
    int64_t tag = DECODE_TAG(header[1]);
    if (tag == RC_TYPE_DICT) return yona_rt_hamt_keys((hamt_node_t*)set);
    int64_t count = set[0];
    int64_t* seq = yona_rt_seq_alloc(count);
    for (int64_t i = 0; i < count; i++) seq[2 + i] = set[2 + i];
    return seq;
}

int64_t* yona_rt_set_union(int64_t* a, int64_t* b) {
    hamt_node_t* ha = set_ensure_hamt(a);
    int64_t* be = yona_rt_set_elements(b);
    for (int64_t i = 0; i < be[0]; i++)
        ha = yona_rt_hamt_put(ha, be[2 + i], 1);
    return (int64_t*)ha;
}

int64_t* yona_rt_set_intersection(int64_t* a, int64_t* b) {
    hamt_node_t* r = yona_rt_hamt_empty();
    int64_t* ae = yona_rt_set_elements(a);
    for (int64_t i = 0; i < ae[0]; i++) {
        int64_t e = ae[2 + i];
        if (yona_rt_set_contains(b, e)) r = yona_rt_hamt_put(r, e, 1);
    }
    return (int64_t*)r;
}

int64_t* yona_rt_set_difference(int64_t* a, int64_t* b) {
    hamt_node_t* r = yona_rt_hamt_empty();
    int64_t* ae = yona_rt_set_elements(a);
    for (int64_t i = 0; i < ae[0]; i++) {
        int64_t e = ae[2 + i];
        if (!yona_rt_set_contains(b, e)) r = yona_rt_hamt_put(r, e, 1);
    }
    return (int64_t*)r;
}

void yona_rt_print_set(int64_t* set) {
    if (!set) { printf("{}"); return; }
    int64_t* header = set - RC_HEADER_SIZE;
    int64_t tag = DECODE_TAG(header[1]);
    if (tag == RC_TYPE_DICT) {
        int64_t* elems = yona_rt_hamt_keys((hamt_node_t*)set);
        int64_t len = elems[0];
        printf("{");
        for (int64_t i = 0; i < len; i++) {
            if (i > 0) printf(", ");
            printf("%ld", elems[2 + i]);
        }
        printf("}");
        return;
    }
    int64_t len = set[0];
    printf("{");
    for (int64_t i = 0; i < len; i++) {
        if (i > 0) printf(", ");
        printf("%ld", set[i + 2]);
    }
    printf("}");
}

/* ===== ADT runtime (recursive types) ===== */
/* Heap-allocated ADT nodes: [tag (i8), field0 (i64), field1 (i64), ...] */
/* Used for recursive types like List a = Cons a (List a) | Nil        */

/* ADT layout (recursive, heap-allocated):
 * [tag, num_fields, heap_mask, field0, field1, ...]
 * tag: constructor tag (from adt_constructors_)
 * num_fields: number of fields
 * heap_mask: bitmask of which fields are heap-typed (for recursive rc_dec)
 * Fields start at index 3.
 */
#define ADT_HDR_SIZE 3  /* tag, num_fields, heap_mask */

void* yona_rt_adt_alloc(int64_t tag, int64_t num_fields) {
    int64_t* node = (int64_t*)rc_alloc(RC_TYPE_ADT, (ADT_HDR_SIZE + num_fields) * sizeof(int64_t));
    node[0] = tag;
    node[1] = num_fields;
    node[2] = 0; /* heap_mask — set by codegen */
    return node;
}

void yona_rt_adt_set_heap_mask(void* node, int64_t mask) {
    ((int64_t*)node)[2] = mask;
}

int64_t yona_rt_adt_get_tag(void* node) {
    return ((int64_t*)node)[0];
}

int64_t yona_rt_adt_get_field(void* node, int64_t index) {
    return ((int64_t*)node)[ADT_HDR_SIZE + index];
}

void yona_rt_adt_set_field(void* node, int64_t index, int64_t value) {
    ((int64_t*)node)[ADT_HDR_SIZE + index] = value;
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
/* Register a direct result for io_await fallback when io_uring is unavailable */
extern int64_t yona_io_register_direct_result(void* result);
#define io_register_direct_result yona_io_register_direct_result

int64_t yona_Std_File__readFile(const char* path) {
    int64_t id = yona_platform_read_file_submit(path);
    if (id > 0) return id; /* uring ID = promise */
    /* io_uring unavailable: read synchronously, wrap result for io_await */
    char* data = yona_platform_read_file(path);
    return io_register_direct_result(data);
}
int64_t yona_Std_File__writeFile(const char* path, const char* content) {
    int64_t id = yona_platform_write_file_submit(path, content);
    if (id > 0) return id; /* uring ID = promise */
    int64_t ok = yona_platform_write_file(path, content) == 0 ? 1 : 0;
    return io_register_direct_result((void*)(intptr_t)ok);
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

/* readLines: read file, split by newlines, return seq of strings */
int64_t* yona_Std_File__readLines(const char* path) {
    extern char* yona_platform_read_file(const char* path);
    char* content = yona_platform_read_file(path);
    if (!content || content[0] == '\0') {
        return yona_rt_seq_alloc(0);
    }

    /* Count lines */
    int64_t count = 1;
    for (char* p = content; *p; p++)
        if (*p == '\n') count++;
    /* Remove trailing empty line if file ends with \n */
    size_t len = strlen(content);
    if (len > 0 && content[len-1] == '\n') count--;

    int64_t* seq = yona_rt_seq_alloc(count);
    /* Set heap_flag: elements are strings (heap-typed) */
    seq[1] = 1;

    char* line_start = content;
    int64_t idx = 0;
    for (char* p = content; ; p++) {
        if (*p == '\n' || *p == '\0') {
            size_t line_len = (size_t)(p - line_start);
            char* line = (char*)yona_rt_rc_alloc_string(line_len + 1);
            memcpy(line, line_start, line_len);
            line[line_len] = '\0';
            seq[2 + idx] = (int64_t)(intptr_t)line; /* SEQ_HDR_SIZE=2 */
            idx++;
            if (*p == '\0') break;
            line_start = p + 1;
        }
    }
    seq[0] = idx; /* actual line count (may differ from initial count) */
    yona_rt_rc_dec(content); /* free the file content */
    return seq;
}

/* ===== Std\Set — persistent hash set (HAMT-backed) ===== */
int64_t* yona_Std_Set__insert(int64_t* set, int64_t elem) {
    return yona_rt_set_insert(set, elem);
}
int64_t yona_Std_Set__contains(int64_t* set, int64_t elem) {
    return yona_rt_set_contains(set, elem);
}
int64_t yona_Std_Set__size(int64_t* set) {
    return yona_rt_set_size(set);
}
int64_t* yona_Std_Set__elements(int64_t* set) {
    return yona_rt_set_elements(set);
}
int64_t* yona_Std_Set__union(int64_t* a, int64_t* b) {
    return yona_rt_set_union(a, b);
}
int64_t* yona_Std_Set__intersection(int64_t* a, int64_t* b) {
    return yona_rt_set_intersection(a, b);
}
int64_t* yona_Std_Set__difference(int64_t* a, int64_t* b) {
    return yona_rt_set_difference(a, b);
}

/* ===== Std\Dict — persistent hash map (HAMT) ===== */
int64_t* yona_Std_Dict__put(int64_t* dict, int64_t key, int64_t value) {
    return yona_rt_dict_put(dict, key, value);
}
int64_t yona_Std_Dict__get(int64_t* dict, int64_t key, int64_t default_val) {
    return yona_rt_dict_get(dict, key, default_val);
}
int64_t yona_Std_Dict__contains(int64_t* dict, int64_t key) {
    return yona_rt_dict_contains(dict, key);
}
int64_t yona_Std_Dict__size(int64_t* dict) {
    return yona_rt_dict_size(dict);
}
int64_t* yona_Std_Dict__keys(int64_t* dict) {
    return yona_rt_dict_keys(dict);
}

/* Binary file I/O */
int64_t yona_Std_File__readFileBytes(const char* path) {
    extern int64_t yona_platform_read_file_bytes_submit(const char* path);
    int64_t id = yona_platform_read_file_bytes_submit(path);
    if (id > 0) return id;
    extern void* yona_rt_bytes_from_string(const char* s);
    void* bytes = yona_rt_bytes_from_string(yona_platform_read_file(path));
    return io_register_direct_result(bytes);
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
const char* yona_Std_Process__exec(const char* cmd) {
    return yona_platform_exec(cmd);
}
int64_t yona_Std_Process__execStatus(const char* cmd) {
    return yona_platform_exec_status(cmd);
}
int64_t yona_Std_Process__setenv(const char* name, const char* value) {
    return yona_platform_setenv(name, value);
}
const char* yona_Std_Process__hostname(void) {
    return yona_platform_hostname();
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
int64_t yona_Std_List__length(int64_t* seq) { return yona_rt_seq_length(seq); }
int64_t yona_Std_List__head(int64_t* seq) { return yona_rt_seq_head(seq); }
int64_t* yona_Std_List__tail(int64_t* seq) { return yona_rt_seq_tail(seq); }
int64_t* yona_Std_List__reverse(int64_t* seq) {
    int64_t len = yona_rt_seq_length(seq);
    int64_t* r = yona_rt_seq_alloc(len);
    for (int64_t i = 0; i < len; i++) r[i + 1] = yona_rt_seq_get(seq, len - 1 - i);
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

/* seq_head and seq_tail are in runtime/seq.c */

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
/* Layout: int64_t array [fn_ptr, ret_type, arity, num_captures, heap_mask, cap0, cap1, ...]
 * The function takes (void* env, args...) where env is the closure itself.
 * Slot 0: function pointer (as int64_t)
 * Slot 1: return CType tag (INT=0, FLOAT=1, ..., ADT=12)
 * Slot 2: number of user arguments (excluding env)
 * Slot 3: number of captures
 * Slot 4: heap_mask — bitmask of which captures are heap-typed (for recursive rc_dec)
 * Captures are stored starting at index 5.
 */

#define CLOSURE_HDR_SIZE 5  /* fn_ptr, ret_type, arity, num_captures, heap_mask */

void* yona_rt_closure_create(void* fn_ptr, int64_t ret_type, int64_t arity, int64_t num_captures) {
    int64_t* closure = (int64_t*)rc_alloc(RC_TYPE_CLOSURE, (CLOSURE_HDR_SIZE + num_captures) * sizeof(int64_t));
    closure[0] = (int64_t)(intptr_t)fn_ptr;
    closure[1] = ret_type;
    closure[2] = arity;
    closure[3] = num_captures;
    closure[4] = 0; /* heap_mask — set by codegen via closure_set_heap_mask */
    return closure;
}

void yona_rt_closure_set_heap_mask(void* closure, int64_t mask) {
    ((int64_t*)closure)[4] = mask;
}

void yona_rt_closure_set_cap(void* closure, int64_t idx, int64_t val) {
    ((int64_t*)closure)[CLOSURE_HDR_SIZE + idx] = val;
}

int64_t yona_rt_closure_get_cap(void* closure, int64_t idx) {
    return ((int64_t*)closure)[CLOSURE_HDR_SIZE + idx];
}
