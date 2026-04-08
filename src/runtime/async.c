/* ===== Async Runtime =====
 *
 * Fixed-size thread pool with work queue. Async functions submit tasks
 * to the pool and return a promise handle immediately (non-blocking).
 * Promises are awaited lazily at use sites via yona_rt_async_await.
 *
 * Structured concurrency: task groups track child promises. If one child
 * fails, siblings are cancelled (thread pool: skip execution; io_uring:
 * IORING_OP_ASYNC_CANCEL). Error propagated to parent via group_await_all.
 */

#include <unistd.h>

#define YONA_POOL_SIZE 8
#define YONA_GROUP_INITIAL_CAP 8

/* Forward declarations for exception handling (exceptions.c) */
void* yona_rt_try_push(void);
void yona_rt_try_end(void);
void yona_rt_raise(int64_t symbol, const char* message);
int64_t yona_rt_get_exception_symbol(void);
const char* yona_rt_get_exception_message(void);

typedef struct {
    int64_t result;
    int completed;             /* accessed via __atomic builtins */
    int error;                 /* 1 if completed with error */
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} yona_promise_t;

typedef int64_t (*yona_async_fn_t)(int64_t);
typedef int64_t (*yona_thunk_fn_t)(void);

/* ===== Task Groups (Structured Concurrency) ===== */

typedef struct yona_task_group {
    int cancelled;       /* accessed via __atomic builtins */
    int pending_count;   /* accessed via __atomic builtins */
    /* Thread pool children */
    yona_promise_t** children;
    int child_count, child_cap;
    /* io_uring children */
    uint64_t* io_children;
    int io_child_count, io_child_cap;
    /* Error from first failing child */
    int64_t first_error_symbol;
    const char* first_error_msg;
    int has_error;
    /* Synchronization */
    pthread_mutex_t mutex;
    pthread_cond_t done_cond;
} yona_task_group_t;

yona_task_group_t* yona_rt_group_begin(void) {
    yona_task_group_t* g = (yona_task_group_t*)calloc(1, sizeof(yona_task_group_t));
    g->child_cap = YONA_GROUP_INITIAL_CAP;
    g->children = (yona_promise_t**)malloc(g->child_cap * sizeof(yona_promise_t*));
    g->io_child_cap = YONA_GROUP_INITIAL_CAP;
    g->io_children = (uint64_t*)malloc(g->io_child_cap * sizeof(uint64_t));
    pthread_mutex_init(&g->mutex, NULL);
    pthread_cond_init(&g->done_cond, NULL);
    return g;
}

void yona_rt_group_register(yona_task_group_t* g, yona_promise_t* p) {
    if (!g) return;
    pthread_mutex_lock(&g->mutex);
    if (g->child_count >= g->child_cap) {
        g->child_cap *= 2;
        g->children = (yona_promise_t**)realloc(g->children, g->child_cap * sizeof(yona_promise_t*));
    }
    g->children[g->child_count++] = p;
    __atomic_fetch_add(&g->pending_count, 1, __ATOMIC_SEQ_CST);
    pthread_mutex_unlock(&g->mutex);
}

void yona_rt_group_register_io(yona_task_group_t* g, uint64_t io_id) {
    if (!g) return;
    pthread_mutex_lock(&g->mutex);
    if (g->io_child_count >= g->io_child_cap) {
        g->io_child_cap *= 2;
        g->io_children = (uint64_t*)realloc(g->io_children, g->io_child_cap * sizeof(uint64_t));
    }
    g->io_children[g->io_child_count++] = io_id;
    __atomic_fetch_add(&g->pending_count, 1, __ATOMIC_SEQ_CST);
    pthread_mutex_unlock(&g->mutex);
}

/* Cancel: set flag + submit IORING_OP_ASYNC_CANCEL for io children */
void yona_rt_group_cancel(yona_task_group_t* g);  /* forward decl — implemented after uring include */

int yona_rt_group_is_cancelled(yona_task_group_t* g) {
    if (!g) return 0;
    return __atomic_load_n(&g->cancelled, __ATOMIC_SEQ_CST);
}

/* Await all children, then re-raise first error if any */
int64_t yona_rt_group_await_all(yona_task_group_t* g);  /* forward decl — needs async_await */

void yona_rt_group_end(yona_task_group_t* g) {
    if (!g) return;
    pthread_mutex_destroy(&g->mutex);
    pthread_cond_destroy(&g->done_cond);
    free(g->children);
    free(g->io_children);
    free(g);
}

/* ===== Task Queue ===== */

typedef struct yona_task {
    yona_async_fn_t fn;     /* single-arg function (legacy) */
    yona_thunk_fn_t thunk;  /* zero-arg thunk (multi-arg via closure) */
    int64_t arg;
    yona_promise_t* promise;
    yona_task_group_t* group; /* owning group (NULL if ungrouped) */
    struct yona_task* next;
} yona_task_t;

/* Thread pool state */
static pthread_t yona_pool_threads[YONA_POOL_SIZE];
static yona_task_t* yona_task_head = NULL;
static yona_task_t* yona_task_tail = NULL;
static pthread_mutex_t yona_pool_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t yona_pool_cond = PTHREAD_COND_INITIALIZER;
static _Atomic int yona_pool_initialized = 0;

static void fulfill_promise(yona_task_t* task, int64_t result, int is_error) {
    pthread_mutex_lock(&task->promise->mutex);
    task->promise->result = result;
    task->promise->error = is_error;
    task->promise->completed = 1;
    pthread_cond_signal(&task->promise->cond);
    pthread_mutex_unlock(&task->promise->mutex);

    if (task->group) {
        if (__atomic_fetch_sub(&task->group->pending_count, 1, __ATOMIC_SEQ_CST) == 1)
            pthread_cond_signal(&task->group->done_cond);
    }
}

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

        /* Check cancellation before executing */
        if (task->group && __atomic_load_n(&task->group->cancelled, __ATOMIC_SEQ_CST)) {
            fulfill_promise(task, 0, 1);
            free(task);
            continue;
        }

        /* Execute with error capture via setjmp */
        void* jmp = yona_rt_try_push();
        if (setjmp(*(jmp_buf*)jmp) == 0) {
            int64_t result = task->thunk ? task->thunk() : task->fn(task->arg);
            yona_rt_try_end();
            fulfill_promise(task, result, 0);
        } else {
            /* Task raised an exception — capture in group */
            if (task->group) {
                pthread_mutex_lock(&task->group->mutex);
                if (!task->group->has_error) {
                    task->group->first_error_symbol = yona_rt_get_exception_symbol();
                    task->group->first_error_msg = yona_rt_get_exception_message();
                    task->group->has_error = 1;
                }
                pthread_mutex_unlock(&task->group->mutex);
                /* Cancel siblings */
                yona_rt_group_cancel(task->group);
            }
            fulfill_promise(task, 0, 1);
        }

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

static yona_promise_t* make_promise(void) {
    yona_promise_t* p = (yona_promise_t*)malloc(sizeof(yona_promise_t));
    p->result = 0;
    p->completed = 0;
    p->error = 0;
    pthread_mutex_init(&p->mutex, NULL);
    pthread_cond_init(&p->cond, NULL);
    return p;
}

static void enqueue_task(yona_task_t* task) {
    pthread_mutex_lock(&yona_pool_mutex);
    if (yona_task_tail) {
        yona_task_tail->next = task;
    } else {
        yona_task_head = task;
    }
    yona_task_tail = task;
    pthread_cond_signal(&yona_pool_cond);
    pthread_mutex_unlock(&yona_pool_mutex);
}

static yona_promise_t* submit_task(yona_async_fn_t fn, yona_thunk_fn_t thunk,
                                    int64_t arg, yona_task_group_t* group) {
    yona_pool_init();
    yona_promise_t* promise = make_promise();

    yona_task_t* task = (yona_task_t*)calloc(1, sizeof(yona_task_t));
    task->fn = fn;
    task->thunk = thunk;
    task->arg = arg;
    task->promise = promise;
    task->group = group;

    if (group) yona_rt_group_register(group, promise);
    enqueue_task(task);
    return promise;
}

yona_promise_t* yona_rt_async_call_thunk(yona_thunk_t thunk) {
    return submit_task(NULL, thunk, 0, NULL);
}

/* Submit async work to thread pool. Returns promise immediately (non-blocking). */
yona_promise_t* yona_rt_async_call(yona_async_fn_t fn, int64_t arg) {
    return submit_task(fn, NULL, arg, NULL);
}

/* Grouped variants for structured concurrency */
yona_promise_t* yona_rt_async_call_thunk_grouped(yona_thunk_t thunk, yona_task_group_t* group) {
    return submit_task(NULL, thunk, 0, group);
}

yona_promise_t* yona_rt_async_call_grouped(yona_async_fn_t fn, int64_t arg, yona_task_group_t* group) {
    return submit_task(fn, NULL, arg, group);
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

/* ===== Task Group: Cancel & Await All ===== */

/* Cancel: set flag. io_uring cancellation is done externally by the codegen
 * or by calling ring_cancel() for each io child (see uring.h). */
void yona_rt_group_cancel(yona_task_group_t* g) {
    if (!g) return;
    __atomic_store_n(&g->cancelled, 1, __ATOMIC_SEQ_CST);
    /* io_uring cancellation: handled by platform layer if uring.h is included.
     * The compiled_runtime.c includes this file after uring.h, so ring_cancel
     * is available there. We provide a hook for the platform layer. */
}

/* Await all children in the group, then re-raise first error if any.
 * Does NOT free promises — auto_await handles that when values are used.
 * Only waits for completion to ensure no tasks outlive the scope. */
int64_t yona_rt_group_await_all(yona_task_group_t* g) {
    if (!g) return 0;

    /* Wait for all thread-pool children to complete */
    for (int i = 0; i < g->child_count; i++) {
        yona_promise_t* p = g->children[i];
        pthread_mutex_lock(&p->mutex);
        while (!p->completed)
            pthread_cond_wait(&p->cond, &p->mutex);
        pthread_mutex_unlock(&p->mutex);
    }

    /* Re-raise first error on the caller's thread */
    if (g->has_error) {
        int64_t sym = g->first_error_symbol;
        const char* msg = g->first_error_msg;
        /* Don't end group here — let the codegen do it after cleanup */
        yona_rt_raise(sym, msg);
    }

    return 0;
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
