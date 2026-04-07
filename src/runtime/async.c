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
