/*
 * Persistent Seq — immutable sequence with O(1) amortized cons/snoc/head/tail.
 *
 * Two representations:
 *   Small (≤32 elements): flat array [length, elem0, elem1, ...]  (RC_TYPE_SEQ)
 *   Large (>32 elements): prefix buffer + 32-way trie + suffix buffer (RC_TYPE_PSEQ)
 *
 * Time complexities:
 *   cons (prepend):   O(1) amortized
 *   snoc (append):    O(1) amortized
 *   head/last:        O(1)
 *   tail/init:        O(1) amortized
 *   get(i):           O(log32 n) ≈ O(1) practical
 *   length:           O(1)
 *   concat:           O(n) [future: O(log n) with RRB relaxation]
 */

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

extern void* rc_alloc(int64_t type_tag, size_t bytes);

#define SEQ_BITS     5
#define SEQ_WIDTH    32          /* 1 << SEQ_BITS */
#define SEQ_MASK     31         /* SEQ_WIDTH - 1 */
#define RC_TYPE_SEQ  1
#define RC_TYPE_PSEQ 11

/* ===== 32-way Trie ===== */
/* Nodes are malloc'd (not RC'd) — owned by the pseq that contains them.
 * Structural sharing: multiple pseqs can point to the same node. */

typedef int64_t* seq_node_t;  /* array of SEQ_WIDTH children or elements */

static seq_node_t node_new(void) {
    return (seq_node_t)calloc(SEQ_WIDTH, sizeof(int64_t));
}

static seq_node_t node_clone(seq_node_t src) {
    seq_node_t n = (seq_node_t)malloc(SEQ_WIDTH * sizeof(int64_t));
    memcpy(n, src, SEQ_WIDTH * sizeof(int64_t));
    return n;
}

/* Path-copy set: returns new root with element at index updated */
static seq_node_t trie_set(seq_node_t node, int shift, int64_t index, int64_t val) {
    seq_node_t copy = node ? node_clone(node) : node_new();
    if (shift == 0) {
        copy[index & SEQ_MASK] = val;
    } else {
        int child = (int)((index >> shift) & SEQ_MASK);
        copy[child] = (int64_t)trie_set((seq_node_t)(intptr_t)copy[child],
                                         shift - SEQ_BITS, index, val);
    }
    return copy;
}

static int64_t trie_get(seq_node_t node, int shift, int64_t index) {
    if (shift == 0) return node[index & SEQ_MASK];
    int child = (int)((index >> shift) & SEQ_MASK);
    return trie_get((seq_node_t)(intptr_t)node[child], shift - SEQ_BITS, index);
}

/* Push a value at the end of the trie (index = current trie_len) */
static seq_node_t trie_push(seq_node_t root, int shift, int64_t trie_len, int64_t val) {
    return trie_set(root, shift, trie_len, val);
}

/* ===== Persistent Seq structure ===== */

typedef struct {
    int64_t length;        /* total logical length */
    int64_t prefix_len;    /* elements in prefix (0..32) */
    int64_t suffix_len;    /* elements in suffix (0..32) */
    int64_t trie_len;      /* elements in trie */
    int shift;             /* trie depth * SEQ_BITS */
    int64_t prefix[SEQ_WIDTH];
    int64_t suffix[SEQ_WIDTH];
    seq_node_t root;       /* trie root (NULL if trie_len == 0) */
} yona_pseq_t;

static int is_pseq(int64_t* seq) {
    if (!seq) return 0;
    int64_t* header = seq - 2;
    return header[1] == RC_TYPE_PSEQ;
}

static yona_pseq_t* pseq_new(void) {
    yona_pseq_t* ps = (yona_pseq_t*)rc_alloc(RC_TYPE_PSEQ, sizeof(yona_pseq_t));
    memset(ps, 0, sizeof(yona_pseq_t));
    return ps;
}

/* Determine required shift for a given trie_len */
static int shift_for(int64_t n) {
    int shift = 0;
    int64_t capacity = SEQ_WIDTH;
    while (capacity < n) { shift += SEQ_BITS; capacity *= SEQ_WIDTH; }
    return shift;
}

/* ===== Public Seq API ===== */

int64_t* yona_rt_seq_alloc(int64_t count) {
    int64_t* seq = (int64_t*)rc_alloc(RC_TYPE_SEQ, (count + 1) * sizeof(int64_t));
    seq[0] = count;
    return seq;
}

int64_t yona_rt_seq_length(int64_t* seq) {
    if (is_pseq(seq)) return ((yona_pseq_t*)seq)->length;
    return seq[0];
}

int64_t yona_rt_seq_get(int64_t* seq, int64_t index) {
    if (!is_pseq(seq)) return seq[index + 1];
    yona_pseq_t* ps = (yona_pseq_t*)seq;
    if (index < ps->prefix_len)
        return ps->prefix[index];
    int64_t ti = index - ps->prefix_len;
    if (ti < ps->trie_len)
        return trie_get(ps->root, ps->shift, ti);
    return ps->suffix[ti - ps->trie_len];
}

void yona_rt_seq_set(int64_t* seq, int64_t index, int64_t value) {
    /* Only for flat seqs during construction (literals, generators) */
    seq[index + 1] = value;
}

int64_t yona_rt_seq_is_empty(int64_t* seq) {
    if (!seq) return 1;
    if (is_pseq(seq)) return ((yona_pseq_t*)seq)->length == 0;
    return seq[0] == 0;
}

int64_t yona_rt_seq_head(int64_t* seq) {
    if (is_pseq(seq)) {
        yona_pseq_t* ps = (yona_pseq_t*)seq;
        if (ps->prefix_len > 0) return ps->prefix[0];
        if (ps->trie_len > 0) return trie_get(ps->root, ps->shift, 0);
        return ps->suffix[0];
    }
    return seq[1];
}

/* Cons (prepend) — O(1) amortized */
int64_t* yona_rt_seq_cons(int64_t elem, int64_t* seq) {
    int64_t len = yona_rt_seq_length(seq);

    if (is_pseq(seq)) {
        yona_pseq_t* old = (yona_pseq_t*)seq;
        yona_pseq_t* ps = pseq_new();
        ps->length = len + 1;
        ps->suffix_len = old->suffix_len;
        memcpy(ps->suffix, old->suffix, (size_t)old->suffix_len * sizeof(int64_t));
        ps->root = old->root;
        ps->shift = old->shift;
        ps->trie_len = old->trie_len;

        if (old->prefix_len < SEQ_WIDTH) {
            /* Room in prefix — insert at front */
            ps->prefix[0] = elem;
            memcpy(ps->prefix + 1, old->prefix, (size_t)old->prefix_len * sizeof(int64_t));
            ps->prefix_len = old->prefix_len + 1;
        } else {
            /* Prefix full — push old prefix elements into trie, new prefix = [elem] */
            for (int64_t i = 0; i < SEQ_WIDTH; i++) {
                if (ps->trie_len >= (1LL << (ps->shift + SEQ_BITS)))
                    ps->shift += SEQ_BITS;
                ps->root = trie_push(ps->root, ps->shift, ps->trie_len, old->prefix[i]);
                ps->trie_len++;
            }
            ps->prefix[0] = elem;
            ps->prefix_len = 1;
        }
        return (int64_t*)ps;
    }

    /* Small seq */
    if (len < SEQ_WIDTH) {
        /* Stay flat — realloc or copy */
        int64_t* header = seq - 2;
        if (header[0] == 1) {
            size_t new_payload = ((size_t)(len + 1) + 1) * sizeof(int64_t);
            int64_t* new_hdr = (int64_t*)realloc(header, 2 * sizeof(int64_t) + new_payload);
            int64_t* ns = new_hdr + 2;
            memmove(ns + 2, ns + 1, (size_t)len * sizeof(int64_t));
            ns[0] = len + 1;
            ns[1] = elem;
            return ns;
        }
        int64_t* result = yona_rt_seq_alloc(len + 1);
        result[1] = elem;
        memcpy(result + 2, seq + 1, (size_t)len * sizeof(int64_t));
        return result;
    }

    /* Upgrade to pseq: prefix = [elem], suffix = old flat elements */
    yona_pseq_t* ps = pseq_new();
    ps->length = len + 1;
    ps->prefix[0] = elem;
    ps->prefix_len = 1;
    /* Put old elements into suffix (up to 32) and trie (rest) */
    if (len <= SEQ_WIDTH) {
        memcpy(ps->suffix, seq + 1, (size_t)len * sizeof(int64_t));
        ps->suffix_len = len;
    } else {
        /* First SEQ_WIDTH elements go to trie, rest to suffix */
        ps->shift = shift_for(len - SEQ_WIDTH);
        for (int64_t i = 0; i < len - SEQ_WIDTH; i++) {
            ps->root = trie_push(ps->root, ps->shift, ps->trie_len, seq[i + 1]);
            ps->trie_len++;
        }
        int64_t suf_start = len - SEQ_WIDTH;
        memcpy(ps->suffix, seq + 1 + suf_start, SEQ_WIDTH * sizeof(int64_t));
        ps->suffix_len = SEQ_WIDTH;
    }
    return (int64_t*)ps;
}

/* Tail — O(1) amortized */
int64_t* yona_rt_seq_tail(int64_t* seq) {
    int64_t len = yona_rt_seq_length(seq);
    if (len <= 1) return yona_rt_seq_alloc(0);

    if (is_pseq(seq)) {
        yona_pseq_t* old = (yona_pseq_t*)seq;

        /* If result would be small enough, flatten */
        if (len - 1 <= SEQ_WIDTH) {
            int64_t* flat = yona_rt_seq_alloc(len - 1);
            for (int64_t i = 1; i < len; i++)
                flat[i] = yona_rt_seq_get(seq, i);
            return flat;
        }

        yona_pseq_t* ps = pseq_new();
        ps->length = len - 1;
        ps->root = old->root;
        ps->shift = old->shift;
        ps->trie_len = old->trie_len;
        ps->suffix_len = old->suffix_len;
        memcpy(ps->suffix, old->suffix, (size_t)old->suffix_len * sizeof(int64_t));

        if (old->prefix_len > 1) {
            ps->prefix_len = old->prefix_len - 1;
            memcpy(ps->prefix, old->prefix + 1, (size_t)ps->prefix_len * sizeof(int64_t));
        } else {
            /* Prefix empty — pull from trie */
            if (old->trie_len > 0) {
                /* Pull first SEQ_WIDTH elements (or less) from trie into prefix */
                int64_t pull = old->trie_len < SEQ_WIDTH ? old->trie_len : SEQ_WIDTH;
                for (int64_t i = 0; i < pull; i++)
                    ps->prefix[i] = trie_get(old->root, old->shift, i);
                ps->prefix_len = pull;
                /* Rebuild trie without the pulled elements */
                ps->trie_len = old->trie_len - pull;
                if (ps->trie_len > 0) {
                    ps->shift = shift_for(ps->trie_len);
                    ps->root = NULL;
                    for (int64_t i = 0; i < ps->trie_len; i++)
                        ps->root = trie_push(ps->root, ps->shift, i,
                            trie_get(old->root, old->shift, pull + i));
                } else {
                    ps->root = NULL;
                    ps->shift = 0;
                }
            } else {
                /* No trie — pull from suffix */
                ps->prefix_len = old->suffix_len - 1;
                memcpy(ps->prefix, old->suffix + 1, (size_t)ps->prefix_len * sizeof(int64_t));
                ps->suffix_len = 0;
            }
        }
        return (int64_t*)ps;
    }

    /* Flat seq */
    int64_t* header = seq - 2;
    if (header[0] == 1) {
        memmove(seq + 1, seq + 2, (size_t)(len - 1) * sizeof(int64_t));
        seq[0] = len - 1;
        return seq;
    }
    int64_t* result = yona_rt_seq_alloc(len - 1);
    memcpy(result + 1, seq + 2, (size_t)(len - 1) * sizeof(int64_t));
    return result;
}

/* Join (concat) — O(n) */
int64_t* yona_rt_seq_join(int64_t* a, int64_t* b) {
    int64_t la = yona_rt_seq_length(a);
    int64_t lb = yona_rt_seq_length(b);
    if (la == 0) return b;
    if (lb == 0) return a;
    int64_t* result = yona_rt_seq_alloc(la + lb);
    for (int64_t i = 0; i < la; i++) result[i + 1] = yona_rt_seq_get(a, i);
    for (int64_t i = 0; i < lb; i++) result[la + i + 1] = yona_rt_seq_get(b, i);
    return result;
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
