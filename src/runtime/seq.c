/*
 * Seq — flat immutable array with unique-owner optimizations.
 *
 * Layout: [rc_header][length][elem0][elem1]...
 *
 * Time complexities:
 *   alloc, get, set, length, head, is_empty:  O(1)
 *   cons (prepend):  O(1) amortized (unique owner realloc) / O(n) shared
 *   tail:            O(1) amortized (unique owner shift) / O(n) shared
 *   join (concat):   O(n)
 *
 * Future: persistent trie (finger tree / RRB-tree) for O(1) amortized
 * cons/tail on shared sequences. See docs/todo-list.md.
 */

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

extern void* rc_alloc(int64_t type_tag, size_t bytes);

#define RC_TYPE_SEQ 1
#define RC_ARENA_SENTINEL INT64_MAX

int64_t* yona_rt_seq_alloc(int64_t count) {
    int64_t* seq = (int64_t*)rc_alloc(RC_TYPE_SEQ, (count + 1) * sizeof(int64_t));
    seq[0] = count;
    return seq;
}

int64_t yona_rt_seq_length(int64_t* seq) { return seq[0]; }
int64_t yona_rt_seq_get(int64_t* seq, int64_t index) { return seq[index + 1]; }
void    yona_rt_seq_set(int64_t* seq, int64_t index, int64_t value) { seq[index + 1] = value; }
int64_t yona_rt_seq_head(int64_t* seq) { return seq[1]; }

int64_t yona_rt_seq_is_empty(int64_t* seq) {
    if (!seq) return 1;
    return seq[0] == 0;
}

int64_t* yona_rt_seq_cons(int64_t elem, int64_t* seq) {
    int64_t len = seq[0];
    int64_t* header = seq - 2;

    if (header[0] == 1 && header[0] != RC_ARENA_SENTINEL) {
        /* Uniquely owned — realloc in place, shift right, prepend */
        size_t new_payload = ((size_t)(len + 1) + 1) * sizeof(int64_t);
        int64_t* new_hdr = (int64_t*)realloc(header, 2 * sizeof(int64_t) + new_payload);
        int64_t* ns = new_hdr + 2;
        memmove(ns + 2, ns + 1, (size_t)len * sizeof(int64_t));
        ns[0] = len + 1;
        ns[1] = elem;
        return ns;
    }

    /* Shared — copy */
    int64_t* result = yona_rt_seq_alloc(len + 1);
    result[1] = elem;
    memcpy(result + 2, seq + 1, (size_t)len * sizeof(int64_t));
    return result;
}

int64_t* yona_rt_seq_tail(int64_t* seq) {
    int64_t len = seq[0];
    if (len <= 1) return yona_rt_seq_alloc(0);
    int64_t* header = seq - 2;

    if (header[0] == 1 && header[0] != RC_ARENA_SENTINEL) {
        /* Uniquely owned — shift left in place */
        memmove(seq + 1, seq + 2, (size_t)(len - 1) * sizeof(int64_t));
        seq[0] = len - 1;
        return seq;
    }

    /* Shared — copy */
    int64_t* result = yona_rt_seq_alloc(len - 1);
    memcpy(result + 1, seq + 2, (size_t)(len - 1) * sizeof(int64_t));
    return result;
}

int64_t* yona_rt_seq_join(int64_t* a, int64_t* b) {
    int64_t la = a[0], lb = b[0];
    if (la == 0) return b;
    if (lb == 0) return a;

    int64_t* header_a = a - 2;
    if (header_a[0] == 1 && header_a[0] != RC_ARENA_SENTINEL) {
        /* Uniquely owned a — extend in place */
        size_t new_payload = ((size_t)(la + lb) + 1) * sizeof(int64_t);
        int64_t* new_hdr = (int64_t*)realloc(header_a, 2 * sizeof(int64_t) + new_payload);
        int64_t* ns = new_hdr + 2;
        memcpy(ns + 1 + la, b + 1, (size_t)lb * sizeof(int64_t));
        ns[0] = la + lb;
        return ns;
    }

    int64_t* result = yona_rt_seq_alloc(la + lb);
    memcpy(result + 1, a + 1, (size_t)la * sizeof(int64_t));
    memcpy(result + 1 + la, b + 1, (size_t)lb * sizeof(int64_t));
    return result;
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
