/* ===== Exception handling (setjmp/longjmp) ===== */

#define YONA_MAX_TRY_DEPTH 256

typedef struct {
    int64_t symbol;
    const char* message;
} yona_exception_t;

/* ===== Perceus phase 3: frame-scoped heap cleanup on unwind =====
 *
 * Each user-defined function with heap params allocates a
 * `yona_frame_t` on its own stack (via codegen alloca) and chains it
 * into the thread-local `yona_current_frame`. On normal exit the frame
 * is popped and cleanup happens via the codegen's function-exit rc_dec
 * loop. On raise, yona_rt_raise walks frames from the current one back
 * to the frame that was active when the surrounding try block ran
 * setjmp, rc_dec'ing any drops that haven't been transferred.
 *
 * Codegen emits:
 *   - alloca yona_frame_t
 *   - fills f->drops[0..N-1] with heap-typed param pointers, sets
 *     f->drop_count = N
 *   - calls yona_rt_frame_push(f) after entry-time rc setup
 *   - before returning normally, calls yona_rt_frame_pop(f)
 *   - at each transfer site (single-use SEQ/SET/DICT arg handed to
 *     a callee), calls yona_rt_frame_transfer(ptr) to NULL that slot
 *     so unwind won't double-dec something the callee now owns.
 *
 * The type tag we feed to the destructor lives in the RC header at
 * `ptr - 16`; `yona_rt_rc_dec` reads it, so the frame only needs to
 * hold raw pointers.
 */

#define YONA_MAX_FRAME_DROPS 16

typedef struct yona_frame {
    struct yona_frame* prev;
    int drop_count;
    int _pad;
    void* drops[YONA_MAX_FRAME_DROPS];
} yona_frame_t;

_Thread_local yona_frame_t* yona_current_frame = NULL;

typedef struct {
    jmp_buf buf[YONA_MAX_TRY_DEPTH];
    int depth;
    yona_exception_t current;
    yona_frame_t* saved_frame[YONA_MAX_TRY_DEPTH];
} yona_exc_state_t;

static _Thread_local yona_exc_state_t yona_exc = { .depth = 0 };

/* Exposed as a TLS global for zero-overhead depth checks from codegen.
 * The codegen declares this as a thread_local i32 and loads it inline
 * (no function call) to branch around frame setup. */
_Thread_local int yona_try_depth = 0;

int yona_rt_try_depth(void) { return yona_try_depth; }

extern void yona_rt_rc_dec(void* ptr);

void yona_rt_frame_push(yona_frame_t* f) {
    /* The codegen's inline depth check (yona_rt_try_depth) already
     * branches around the stores + push call when depth == 0. But
     * as a safety net, also check here. */
    if (yona_exc.depth == 0) {
        f->drop_count = -1;
        return;
    }
    f->prev = yona_current_frame;
    yona_current_frame = f;
}

void yona_rt_frame_pop(yona_frame_t* f) {
    if (!f) return;  /* hot path: frame was never allocated (no try active) */
    yona_current_frame = f->prev;
}

void yona_rt_frame_transfer(void* ptr) {
    yona_frame_t* f = yona_current_frame;
    if (!f || !ptr) return;
    for (int i = 0; i < f->drop_count; i++) {
        if (f->drops[i] == ptr) { f->drops[i] = NULL; return; }
    }
}

static void yona_rt_unwind_frames_to(yona_frame_t* stop) {
    while (yona_current_frame && yona_current_frame != stop) {
        yona_frame_t* f = yona_current_frame;
        for (int i = 0; i < f->drop_count; i++) {
            void* p = f->drops[i];
            if (p) {
                f->drops[i] = NULL;
                yona_rt_rc_dec(p);
            }
        }
        yona_current_frame = f->prev;
    }
}

// yona_rt_try_push: push a jmp_buf slot, return pointer to it.
// The caller must call setjmp on the returned pointer directly
// (setjmp must execute in the caller's stack frame).
void* yona_rt_try_push(void) {
    if (yona_exc.depth >= YONA_MAX_TRY_DEPTH) {
        fprintf(stderr, "Fatal: try/catch nesting depth exceeded\n");
        abort();
    }
    yona_exc.saved_frame[yona_exc.depth] = yona_current_frame;
    yona_try_depth = yona_exc.depth + 1;
    return &yona_exc.buf[yona_exc.depth++];
}

void yona_rt_try_end(void) {
    if (yona_exc.depth > 0) yona_exc.depth--;
    yona_try_depth = yona_exc.depth;
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
    /* Phase 3: unwind owned-heap frames before longjmp blows past
     * their function-exit cleanups. */
    yona_rt_unwind_frames_to(yona_exc.saved_frame[yona_exc.depth]);
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

