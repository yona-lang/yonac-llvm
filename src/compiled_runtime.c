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
    int64_t* set = (int64_t*)malloc((count + 1) * sizeof(int64_t));
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
    int64_t* dict = (int64_t*)malloc((count * 2 + 1) * sizeof(int64_t));
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

void* yona_rt_adt_alloc(int8_t tag, int64_t num_fields) {
    int64_t* node = (int64_t*)malloc((1 + num_fields) * sizeof(int64_t));
    node[0] = (int64_t)tag;
    return node;
}

int8_t yona_rt_adt_get_tag(void* node) {
    return (int8_t)(((int64_t*)node)[0]);
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

void yona_rt_raise(int64_t symbol, const char* message) {
    if (yona_exc.depth == 0) {
        fprintf(stderr, "Unhandled exception: :%ld \"%s\"\n", symbol, message ? message : "");
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

typedef struct yona_task {
    yona_async_fn_t fn;
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

        /* Execute the task */
        int64_t result = task->fn(task->arg);

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
    free(c);
    return result;
}

/* Create a closure from a 2-arg function with 1 captured arg */
int64_t yona_rt_closure2_create(int64_t (*fn)(int64_t, int64_t), int64_t captured) {
    yona_closure2_t* c = (yona_closure2_t*)malloc(sizeof(yona_closure2_t));
    c->fn2 = fn;
    c->captured = captured;
    return (int64_t)(void*)c;
}
