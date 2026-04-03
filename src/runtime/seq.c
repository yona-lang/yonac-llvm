/*
 * Seq — persistent immutable sequence.
 *
 * Two representations:
 *   Small (≤32 elements): flat array [length, elem0, elem1, ...]  (RC_TYPE_SEQ)
 *   Large (>32 elements): chunked list of RC-managed 32-element blocks (RC_TYPE_CHUNKED_SEQ)
 *
 * Time complexities:
 *   cons (prepend):  O(1)
 *   head:            O(1)
 *   tail:            O(1)
 *   snoc (append):   O(1) amortized (unique owner) / O(n) shared
 *   get(i):          O(1) small / O(n/32) large
 *   length:          O(1)
 *   concat:          O(n)
 */

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

extern void* rc_alloc(int64_t type_tag, size_t bytes);
extern void yona_rt_rc_inc(void* ptr);
extern void yona_rt_rc_dec(void* ptr);

#define RC_TYPE_SEQ 1
#define RC_TYPE_CHUNKED 11
#define CHUNK_SIZE 32
#define RC_ARENA_SENTINEL INT64_MAX
#define SEQ_HDR_SIZE 2

/* ===== Chunked Seq ===== */
/*
 * A chunked seq is a linked list of RC-managed chunks.
 * Each chunk holds up to CHUNK_SIZE elements.
 * Structural sharing: cons creates a new header pointing to existing chunks.
 *
 * Layout: [rc_header][total_length][count][elems...][next_ptr]
 */

typedef struct yona_chunk {
    int64_t total_length;   /* total elements in this chunk + all subsequent */
    int64_t offset;         /* index of first element in elems[] */
    int64_t count;          /* number of valid elements (from offset) */
    int64_t elems[CHUNK_SIZE];
    struct yona_chunk* next; /* next chunk (RC-managed, or NULL) */
} yona_chunk_t;

static yona_chunk_t* chunk_alloc(int64_t count, int64_t total) {
    yona_chunk_t* c = (yona_chunk_t*)rc_alloc(RC_TYPE_CHUNKED, sizeof(yona_chunk_t));
    c->total_length = total;
    c->offset = 0;
    c->count = count;
    c->next = NULL;
    return c;
}

static int is_chunked(int64_t* seq) {
    if (!seq) return 0;
    int64_t* header = seq - RC_HEADER_SIZE;
    return DECODE_TAG(header[1]) == RC_TYPE_CHUNKED;
}

/* ===== Public API ===== */

int64_t* yona_rt_seq_alloc(int64_t count) {
    int64_t* seq = (int64_t*)rc_alloc(RC_TYPE_SEQ, (SEQ_HDR_SIZE + count) * sizeof(int64_t));
    seq[0] = count;
    seq[1] = 0; /* heap_flag */
    return seq;
}

void yona_rt_seq_set_heap(int64_t* seq, int64_t flag) {
    seq[1] = flag;
}

int64_t yona_rt_seq_length(int64_t* seq) {
    if (is_chunked(seq)) return ((yona_chunk_t*)seq)->total_length;
    return seq[0];
}

int64_t yona_rt_seq_get(int64_t* seq, int64_t index) {
    if (!is_chunked(seq)) return seq[SEQ_HDR_SIZE + index];
    yona_chunk_t* c = (yona_chunk_t*)seq;
    while (c) {
        if (index < c->count) return c->elems[c->offset + index];
        index -= c->count;
        c = c->next;
    }
    return 0;
}

void yona_rt_seq_set(int64_t* seq, int64_t index, int64_t value) {
    /* Only for flat seqs during construction */
    seq[SEQ_HDR_SIZE + index] = value;
}

int64_t yona_rt_seq_is_empty(int64_t* seq) {
    if (!seq) return 1;
    if (is_chunked(seq)) return ((yona_chunk_t*)seq)->total_length == 0;
    return seq[0] == 0;
}

int64_t yona_rt_seq_head(int64_t* seq) {
    if (is_chunked(seq)) {
        yona_chunk_t* c = (yona_chunk_t*)seq;
        return c->elems[c->offset];
    }
    return seq[SEQ_HDR_SIZE];
}

/* Cons (prepend) — O(1) */
int64_t* yona_rt_seq_cons(int64_t elem, int64_t* seq) {
    int64_t len = yona_rt_seq_length(seq);

    if (is_chunked(seq)) {
        yona_chunk_t* old = (yona_chunk_t*)seq;
        int64_t* hdr = ((int64_t*)old) - 2;

        /* Unique owner with space before offset — write in place */
        if (__atomic_load_n(&hdr[0], __ATOMIC_ACQUIRE) == 1 && old->offset > 0) {
            old->offset--;
            old->count++;
            old->total_length++;
            old->elems[old->offset] = elem;
            return (int64_t*)old;
        }

        /* Room in chunk (offset > 0 or count < CHUNK_SIZE) — path-copy */
        if (old->offset > 0) {
            yona_chunk_t* c = chunk_alloc(old->count + 1, len + 1);
            c->offset = old->offset - 1;
            c->elems[c->offset] = elem;
            memcpy(c->elems + old->offset, old->elems + old->offset,
                   (size_t)old->count * sizeof(int64_t));
            c->next = old->next;
            if (c->next) yona_rt_rc_inc(c->next);
            return (int64_t*)c;
        }

        /* No room — create new chunk with elem at the END (offset=31) so
         * future cons operations can prepend without copying */
        yona_chunk_t* c = chunk_alloc(1, len + 1);
        c->offset = CHUNK_SIZE - 1;
        c->elems[CHUNK_SIZE - 1] = elem;
        c->next = old;
        yona_rt_rc_inc(old);
        return (int64_t*)c;
    }

    /* Flat seq: if small, stay flat. Unique-owner realloc only for
     * non-pooled blocks (pool blocks can't be realloc'd). */
    if (len < CHUNK_SIZE) {
        int64_t* header = seq - RC_HEADER_SIZE;
        int64_t rc_val = __atomic_load_n(&header[0], __ATOMIC_ACQUIRE);
        int pool_cls = DECODE_POOL_CLASS(header[1]);
        if (rc_val == 1 && rc_val != RC_ARENA_SENTINEL && pool_cls < 0) {
            /* Non-pooled unique owner: realloc in place */
            size_t new_payload = (SEQ_HDR_SIZE + len + 1) * sizeof(int64_t);
            int64_t* new_hdr = (int64_t*)realloc(header, RC_HEADER_SIZE * sizeof(int64_t) + new_payload);
            int64_t* ns = new_hdr + RC_HEADER_SIZE;
            memmove(ns + SEQ_HDR_SIZE + 1, ns + SEQ_HDR_SIZE, (size_t)len * sizeof(int64_t));
            ns[0] = len + 1;
            ns[SEQ_HDR_SIZE] = elem;
            return ns;
        }
        int64_t* result = yona_rt_seq_alloc(len + 1);
        result[1] = seq[1]; /* copy heap_flag */
        result[SEQ_HDR_SIZE] = elem;
        memcpy(result + SEQ_HDR_SIZE + 1, seq + SEQ_HDR_SIZE, (size_t)len * sizeof(int64_t));
        return result;
    }

    /* Flat seq at CHUNK_SIZE: convert to chunked.
     * Old flat becomes a body chunk, elem goes in a head chunk at high offset
     * (so future cons can prepend without copying). */
    yona_chunk_t* body = chunk_alloc(len, len);
    memcpy(body->elems, seq + SEQ_HDR_SIZE, (size_t)len * sizeof(int64_t));

    yona_chunk_t* head = chunk_alloc(1, len + 1);
    head->offset = CHUNK_SIZE - 1;
    head->elems[CHUNK_SIZE - 1] = elem;
    head->next = body;

    return (int64_t*)head;
}

/* Tail — O(1) */
int64_t* yona_rt_seq_tail(int64_t* seq) {
    int64_t len = yona_rt_seq_length(seq);
    if (len <= 1) return yona_rt_seq_alloc(0);

    if (is_chunked(seq)) {
        yona_chunk_t* old = (yona_chunk_t*)seq;
        int64_t* hdr = ((int64_t*)old) - 2;

        if (old->count == 1) {
            /* Last element in this chunk — tail is the next chunk */
            if (old->next) {
                yona_rt_rc_inc(old->next);
                return (int64_t*)old->next;
            }
            return yona_rt_seq_alloc(0);
        }

        /* Unique owner — just bump offset */
        if (__atomic_load_n(&hdr[0], __ATOMIC_ACQUIRE) == 1) {
            old->offset++;
            old->count--;
            old->total_length--;
            return (int64_t*)old;
        }

        /* Shared — create new chunk header with adjusted offset */
        yona_chunk_t* c = chunk_alloc(old->count - 1, len - 1);
        c->offset = old->offset + 1;
        memcpy(c->elems, old->elems, (size_t)(c->offset + c->count) * sizeof(int64_t));
        c->next = old->next;
        if (c->next) yona_rt_rc_inc(c->next);
        return (int64_t*)c;
    }

    /* Flat seq */
    int64_t* header = seq - 2;
    if (__atomic_load_n(&header[0], __ATOMIC_ACQUIRE) == 1 && header[0] != RC_ARENA_SENTINEL) {
        memmove(seq + SEQ_HDR_SIZE, seq + SEQ_HDR_SIZE + 1, (size_t)(len - 1) * sizeof(int64_t));
        seq[0] = len - 1;
        return seq;
    }
    int64_t* result = yona_rt_seq_alloc(len - 1);
    result[1] = seq[1]; /* copy heap_flag */
    memcpy(result + SEQ_HDR_SIZE, seq + SEQ_HDR_SIZE + 1, (size_t)(len - 1) * sizeof(int64_t));
    return result;
}

/* Join (concat) — O(n) */
int64_t* yona_rt_seq_join(int64_t* a, int64_t* b) {
    int64_t la = yona_rt_seq_length(a);
    int64_t lb = yona_rt_seq_length(b);
    if (la == 0) return b;
    if (lb == 0) return a;

    /* Flatten both to flat array — simplest correct approach */
    int64_t* result = yona_rt_seq_alloc(la + lb);
    for (int64_t i = 0; i < la; i++) result[SEQ_HDR_SIZE + i] = yona_rt_seq_get(a, i);
    for (int64_t i = 0; i < lb; i++) result[SEQ_HDR_SIZE + la + i] = yona_rt_seq_get(b, i);
    return result;
}

/* Snoc (append) — O(n) general, O(1) for flat unique owner */
int64_t* yona_rt_seq_snoc(int64_t* seq, int64_t elem) {
    int64_t len = yona_rt_seq_length(seq);

    if (!is_chunked(seq) && len < CHUNK_SIZE) {
        /* Flat seq with room: allocate new with one more element */
        int64_t* result = yona_rt_seq_alloc(len + 1);
        memcpy(result + SEQ_HDR_SIZE, seq + SEQ_HDR_SIZE, (size_t)len * sizeof(int64_t));
        result[SEQ_HDR_SIZE + len] = elem;
        result[1] = seq[1]; /* copy heap_flag */
        return result;
    }

    /* General case: join seq with [elem] */
    int64_t* single = yona_rt_seq_alloc(1);
    single[SEQ_HDR_SIZE] = elem;
    return yona_rt_seq_join(seq, single);
}

/* Print */
void yona_rt_print_seq(int64_t* seq) {
    int64_t len = yona_rt_seq_length(seq);
    printf("[");
    for (int64_t i = 0; i < len; i++) {
        if (i > 0) printf(", ");
        printf("%ld", yona_rt_seq_get(seq, i));
    }
    printf("]");
}
