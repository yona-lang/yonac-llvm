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
    char* result = (char*)malloc(len_a + len_b + 1);
    memcpy(result, a, len_a);
    memcpy(result + len_a, b, len_b + 1);
    return result;
}

/* ===== Sequence (list) runtime ===== */

// Sequence: [length, elem0, elem1, ...]
// Stored as a heap-allocated array of i64 where index 0 is the length.

int64_t* yona_rt_seq_alloc(int64_t count) {
    int64_t* seq = (int64_t*)malloc((count + 1) * sizeof(int64_t));
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

int yona_rt_symbol_eq(const char* a, const char* b) {
    return strcmp(a, b) == 0;
}

void yona_rt_print_symbol(const char* name) {
    printf(":%s", name);
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

/* Std\String */
int64_t yona_Std_String__length(const char* s) { return (int64_t)strlen(s); }
const char* yona_Std_String__toUpperCase(const char* s) {
    size_t len = strlen(s);
    char* r = (char*)malloc(len + 1);
    for (size_t i = 0; i <= len; i++) r[i] = (char)toupper((unsigned char)s[i]);
    return r;
}
const char* yona_Std_String__toLowerCase(const char* s) {
    size_t len = strlen(s);
    char* r = (char*)malloc(len + 1);
    for (size_t i = 0; i <= len; i++) r[i] = (char)tolower((unsigned char)s[i]);
    return r;
}

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

/* Std\Types */
int64_t yona_Std_Types__toInt(const char* s) { return (int64_t)atoll(s); }
double yona_Std_Types__toFloat(const char* s) { return atof(s); }

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
