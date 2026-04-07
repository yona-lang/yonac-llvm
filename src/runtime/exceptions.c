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

