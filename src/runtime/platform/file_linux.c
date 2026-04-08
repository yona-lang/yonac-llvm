/*
 * Linux file I/O — submit-and-return via io_uring.
 *
 * Each async function submits to io_uring and returns the user_data ID
 * immediately. Completion is handled by yona_rt_io_await() which does
 * type-specific post-processing (close fd, null-terminate buffer, etc).
 */

#include "../platform.h"
#include "uring.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>

extern void* yona_rt_rc_alloc_string(size_t bytes);
extern void yona_rt_rc_inc(void* ptr);
extern void yona_rt_rc_dec(void* ptr);
extern int64_t* yona_rt_seq_alloc(int64_t count);

/* ===== Generic io_uring completer ===== */

/* Sentinel: when io_uring is unavailable, IO functions store the direct
 * result in the io_ctx table with a special type. io_await returns it. */
#define IO_OP_DIRECT_RESULT 99
static _Atomic uint64_t direct_result_id = 0x80000000ULL; /* high offset avoids uring ID collision */

/* Register a direct (blocking fallback) result for io_await. */
static int64_t io_register_direct_result(void* result) {
    uint64_t id = atomic_fetch_add(&direct_result_id, 1);
    io_context_t* ctx = (io_context_t*)malloc(sizeof(io_context_t));
    ctx->type = IO_OP_DIRECT_RESULT;
    ctx->fd = -1;
    ctx->buf = (char*)result;
    ctx->buf_size = 0;
    ctx->close_fd = 0;
    io_ctx_put(id, ctx);
    return (int64_t)id;
}

/* Exported for compiled_runtime.c wrapper functions */
int64_t yona_io_register_direct_result(void* result) {
    return io_register_direct_result(result);
}

int64_t yona_rt_io_await(int64_t uring_id) {
    if (uring_id <= 0) return 0;
    io_context_t* ctx = io_ctx_take((uint64_t)uring_id);
    if (ctx && ctx->type == IO_OP_DIRECT_RESULT) {
        /* Blocking fallback: result is stored in buf pointer */
        int64_t result = (int64_t)(intptr_t)ctx->buf;
        free(ctx);
        return result;
    }
    if (ctx) io_ctx_put((uint64_t)uring_id, ctx); /* put it back for real await */

    int32_t res = ring_await((uint64_t)uring_id);

    /* Handle cancellation: clean up context and raise */
    if (res == -125 /* ECANCELED */) {
        ctx = io_ctx_take((uint64_t)uring_id);
        if (ctx) {
            if (ctx->buf) free(ctx->buf);
            if (ctx->close_fd && ctx->fd >= 0) close(ctx->fd);
            free(ctx);
        }
        return 0; /* Cancelled — caller checks group error */
    }

    ctx = io_ctx_take((uint64_t)uring_id);
    if (!ctx) return (int64_t)res;

    int64_t result;
    switch (ctx->type) {
        case IO_OP_READ_FILE:
            if (res >= 0) ctx->buf[res] = '\0';
            else ctx->buf[0] = '\0';
            if (ctx->close_fd) close(ctx->fd);
            result = (int64_t)(intptr_t)ctx->buf;
            break;
        case IO_OP_WRITE_FILE:
            if (ctx->close_fd) close(ctx->fd);
            /* Unpin the content buffer (was rc_inc'd at submit) */
            if (ctx->buf) yona_rt_rc_dec(ctx->buf);
            result = (res == (int32_t)ctx->buf_size) ? 1 : 0;
            break;
        case IO_OP_ACCEPT:
            free(ctx->buf);
            result = (res >= 0) ? (int64_t)res : -1;
            break;
        case IO_OP_CONNECT:
            free(ctx->buf);
            result = (res >= 0) ? (int64_t)ctx->fd : -1;
            if (res < 0) close(ctx->fd);
            break;
        case IO_OP_SEND:
            /* Unpin the send buffer (was rc_inc'd at submit) */
            if (ctx->buf) yona_rt_rc_dec(ctx->buf);
            result = (int64_t)res;
            break;
        case IO_OP_RECV:
            if (res > 0) ctx->buf[res] = '\0';
            else ctx->buf[0] = '\0';
            result = (int64_t)(intptr_t)ctx->buf;
            break;
        case IO_OP_RECV_BYTES: {
            /* Bytes: set length field, return the Bytes buffer */
            int64_t* bytes_buf = (int64_t*)(intptr_t)ctx->buf;
            bytes_buf[0] = (res > 0) ? (int64_t)res : 0;
            result = (int64_t)(intptr_t)bytes_buf;
            break;
        }
        case IO_OP_READ_FILE_BYTES: {
            int64_t* bytes_buf = (int64_t*)(intptr_t)ctx->buf;
            bytes_buf[0] = (res > 0) ? (int64_t)res : 0;
            if (ctx->close_fd) close(ctx->fd);
            result = (int64_t)(intptr_t)bytes_buf;
            break;
        }
        default:
            result = (int64_t)res;
            break;
    }
    free(ctx);
    return result;
}

/* ===== Submit-and-return functions ===== */

/* Blocking fallback: read entire file synchronously.
 * Used when io_uring is unavailable (ENOMEM, old kernel, container). */
static int64_t read_file_blocking(const char* path) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        char* empty = (char*)yona_rt_rc_alloc_string(1);
        empty[0] = '\0';
        return (int64_t)(intptr_t)empty;
    }
    struct stat st;
    if (fstat(fd, &st) < 0) { close(fd); char* e = (char*)yona_rt_rc_alloc_string(1); e[0]='\0'; return (int64_t)(intptr_t)e; }
    size_t size = (size_t)st.st_size;
    extern void* yona_rt_rc_alloc_string_len(size_t bytes, size_t str_len);
    char* buf = (char*)yona_rt_rc_alloc_string_len(size + 1, size);
    ssize_t n = read(fd, buf, size);
    if (n >= 0) buf[n] = '\0'; else buf[0] = '\0';
    close(fd);
    return (int64_t)(intptr_t)buf;
}

/* readFile: open, allocate buffer, submit uring read, return ID.
 * Uses rc_alloc_string_len to store the file size in the RC header,
 * enabling O(1) string length instead of O(n) strlen. */
int64_t yona_platform_read_file_submit(const char* path) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) return 0;
    struct stat st;
    if (fstat(fd, &st) < 0) { close(fd); return 0; }
    size_t size = (size_t)st.st_size;
    extern void* yona_rt_rc_alloc_string_len(size_t bytes, size_t str_len);
    char* buf = (char*)yona_rt_rc_alloc_string_len(size + 1, size);

    io_context_t* ctx = (io_context_t*)malloc(sizeof(io_context_t));
    ctx->type = IO_OP_READ_FILE;
    ctx->fd = fd;
    ctx->buf = buf;
    ctx->buf_size = size;
    ctx->close_fd = 1;

    struct io_uring_sqe sqe;
    memset(&sqe, 0, sizeof(sqe));
    sqe.opcode = IORING_OP_READ;
    sqe.fd = fd;
    sqe.addr = (unsigned long)buf;
    sqe.len = (unsigned)size;
    sqe.off = 0;

    uint64_t id = ring_submit_sqe(&sqe);
    if (id == 0) {
        /* io_uring unavailable — fall back to blocking read */
        close(fd);
        free(ctx);
        return 0;
    }
    io_ctx_put(id, ctx);
    return (int64_t)id;
}

/* writeFile: open, submit uring write, return ID */
int64_t yona_platform_write_file_submit(const char* path, const char* content) {
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return 0;
    size_t len = strlen(content);

    /* Copy content to RC-managed buffer so it survives until I/O completes.
     * Cannot rc_inc the original — it may be a string constant without RC header. */
    char* pinned = (char*)yona_rt_rc_alloc_string(len + 1);
    memcpy(pinned, content, len + 1);

    io_context_t* ctx = (io_context_t*)malloc(sizeof(io_context_t));
    ctx->type = IO_OP_WRITE_FILE;
    ctx->fd = fd;
    ctx->buf = pinned; /* rc_dec'd in completer */
    ctx->buf_size = len;
    ctx->close_fd = 1;

    struct io_uring_sqe sqe;
    memset(&sqe, 0, sizeof(sqe));
    sqe.opcode = IORING_OP_WRITE;
    sqe.fd = fd;
    sqe.addr = (unsigned long)pinned; /* use RC-managed copy */
    sqe.len = (unsigned)len;
    sqe.off = 0;

    uint64_t id = ring_submit_sqe(&sqe);
    if (id == 0) { close(fd); yona_rt_rc_dec(pinned); free(ctx); return 0; }
    io_ctx_put(id, ctx);
    return (int64_t)id;
}

/* readFileBytes: like readFile but returns Bytes instead of String */
int64_t yona_platform_read_file_bytes_submit(const char* path) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) return 0;
    struct stat st;
    if (fstat(fd, &st) < 0) { close(fd); return 0; }
    size_t size = (size_t)st.st_size;

    extern void* rc_alloc(int64_t type_tag, size_t payload_bytes);
    int64_t* buf = (int64_t*)rc_alloc(8 /* RC_TYPE_BYTES */, sizeof(int64_t) + size);
    buf[0] = 0; /* length set by completer */

    io_context_t* ctx = (io_context_t*)malloc(sizeof(io_context_t));
    ctx->type = IO_OP_READ_FILE_BYTES;
    ctx->fd = fd;
    ctx->buf = (char*)buf;
    ctx->buf_size = size;
    ctx->close_fd = 1;

    struct io_uring_sqe sqe;
    memset(&sqe, 0, sizeof(sqe));
    sqe.opcode = IORING_OP_READ;
    sqe.fd = fd;
    sqe.addr = (unsigned long)(uint8_t*)(buf + 1);
    sqe.len = (unsigned)size;
    sqe.off = 0;

    uint64_t id = ring_submit_sqe(&sqe);
    if (id == 0) { close(fd); free(ctx); return 0; }
    io_ctx_put(id, ctx);
    return (int64_t)id;
}

/* ===== Synchronous fallbacks ===== */

char* yona_platform_read_file(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) { char* r = (char*)yona_rt_rc_alloc_string(1); r[0] = '\0'; return r; }
    fseek(f, 0, SEEK_END); long size = ftell(f); fseek(f, 0, SEEK_SET);
    if (size < 0) size = 0;
    char* buf = (char*)yona_rt_rc_alloc_string((size_t)size + 1);
    size_t rd = fread(buf, 1, (size_t)size, f);
    buf[rd] = '\0'; fclose(f); return buf;
}

int yona_platform_write_file(const char* path, const char* content) {
    FILE* f = fopen(path, "wb"); if (!f) return -1;
    size_t len = strlen(content);
    size_t w = fwrite(content, 1, len, f); fclose(f);
    return (w == len) ? 0 : -1;
}

int yona_platform_append_file(const char* path, const char* content) {
    FILE* f = fopen(path, "ab"); if (!f) return -1;
    size_t len = strlen(content);
    size_t w = fwrite(content, 1, len, f); fclose(f);
    return (w == len) ? 0 : -1;
}

int yona_platform_file_exists(const char* path) {
    struct stat st; return (stat(path, &st) == 0) ? 1 : 0;
}

int yona_platform_remove_file(const char* path) { return (remove(path) == 0) ? 0 : -1; }

int64_t yona_platform_file_size(const char* path) {
    struct stat st; if (stat(path, &st) != 0) return -1; return (int64_t)st.st_size;
}

int64_t* yona_platform_list_dir(const char* path) {
    DIR* dir = opendir(path);
    if (!dir) return yona_rt_seq_alloc(0);
    int64_t count = 0;
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.' && (entry->d_name[1] == '\0' || (entry->d_name[1] == '.' && entry->d_name[2] == '\0'))) continue;
        count++;
    }
    rewinddir(dir);
    int64_t* seq = yona_rt_seq_alloc(count);
    int64_t i = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.' && (entry->d_name[1] == '\0' || (entry->d_name[1] == '.' && entry->d_name[2] == '\0'))) continue;
        size_t len = strlen(entry->d_name);
        char* name = (char*)yona_rt_rc_alloc_string(len + 1);
        memcpy(name, entry->d_name, len + 1);
        seq[1 + i] = (int64_t)(intptr_t)name;
        i++;
    }
    closedir(dir); return seq;
}

/* ===== Streaming File Line Iterator ===== */
/* Reads a file line-by-line with 64KB buffered I/O.
 * Returns an Iterator (closure that yields Option String).
 * Memory: O(64KB buffer + one line) instead of O(file_size). */

#define LINE_ITER_BUF_SIZE 65536

/* Forward declarations for runtime functions used by the iterator */
extern void* rc_alloc(int64_t type_tag, size_t payload_bytes);

typedef struct {
    int fd;
    char* buf;
    size_t buf_pos;
    size_t buf_len;
    int eof;
} line_iter_state_t;

/* Read the next line from the buffered file iterator.
 * Returns an Option: Some(line_string) or None (as heap ADT).
 * Option layout: [rc, tag_encoded, tag_i64, value_i64] where tag=0 is Some, tag=1 is None. */
static int64_t line_iter_next(int64_t* closure_env) {
    /* closure_env layout: [fn_ptr, ret_tag, arity, num_caps, heap_mask, cap0]
     * cap0 = pointer to line_iter_state_t */
    line_iter_state_t* st = (line_iter_state_t*)(intptr_t)closure_env[5];
    extern void* yona_rt_rc_alloc_string_len(size_t bytes, size_t str_len);

    if (st->eof) {
        /* Return None — use the prelude None constructor.
         * None is tag=1, arity=0. Non-recursive ADT: struct {i64 tag}
         * But since all values are i64, return a tagged pointer. */
        /* Allocate a small ADT: [rc, type_tag, tag_value] */
        int64_t* adt = (int64_t*)rc_alloc(4 /* RC_TYPE_ADT */, 2 * sizeof(int64_t));
        adt[0] = 1;  /* tag = None */
        return (int64_t)(intptr_t)adt;
    }

    /* Scan buffer for newline, refill if needed */
    char line_buf[8192];
    size_t line_len = 0;

    while (1) {
        /* Refill buffer if empty */
        if (st->buf_pos >= st->buf_len) {
            ssize_t n = read(st->fd, st->buf, LINE_ITER_BUF_SIZE);
            if (n <= 0) {
                st->eof = 1;
                if (line_len == 0) {
                    /* EOF with no partial line — return None */
                    int64_t* adt = (int64_t*)rc_alloc(4, 2 * sizeof(int64_t));
                    adt[0] = 1;
                    return (int64_t)(intptr_t)adt;
                }
                break;  /* Return the partial line */
            }
            st->buf_len = (size_t)n;
            st->buf_pos = 0;
        }

        /* Scan for newline in current buffer */
        while (st->buf_pos < st->buf_len) {
            char c = st->buf[st->buf_pos++];
            if (c == '\n') goto line_complete;
            if (line_len < sizeof(line_buf) - 1)
                line_buf[line_len++] = c;
        }
    }

line_complete:
    line_buf[line_len] = '\0';

    /* Allocate RC string for the line */
    char* line_str = (char*)yona_rt_rc_alloc_string_len(line_len + 1, line_len);
    memcpy(line_str, line_buf, line_len + 1);

    /* Return Some(line_str) — ADT with tag=0, value=pointer */
    int64_t* adt = (int64_t*)rc_alloc(4 /* RC_TYPE_ADT */, 2 * sizeof(int64_t));
    adt[0] = 0;  /* tag = Some */
    adt[1] = (int64_t)(intptr_t)line_str;
    return (int64_t)(intptr_t)adt;
}

/* Create a streaming line iterator for a file.
 * Returns an Iterator ADT wrapping the next-line closure. */
int64_t yona_rt_file_line_iterator(const char* path) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) return 0;

    /* Allocate state */
    line_iter_state_t* st = (line_iter_state_t*)malloc(sizeof(line_iter_state_t));
    st->fd = fd;
    st->buf = (char*)malloc(LINE_ITER_BUF_SIZE);
    st->buf_pos = 0;
    st->buf_len = 0;
    st->eof = 0;

    /* Create a closure that captures the state.
     * Closure layout: [fn_ptr, ret_tag, arity, num_caps, heap_mask, cap0, ...] */
    extern int64_t* yona_rt_closure_create(int64_t fn_ptr, int64_t ret_tag,
                                            int64_t arity, int64_t num_caps);
    int64_t* closure = yona_rt_closure_create(
        (int64_t)(intptr_t)line_iter_next,
        0,   /* ret_tag: INT (actually Option, but i64 representation) */
        1,   /* arity: takes 1 arg (the closure env itself) */
        1    /* num_caps: 1 captured value (the state pointer) */
    );
    /* Set cap0 = state pointer */
    extern void yona_rt_closure_set_cap(int64_t* closure, int64_t index, int64_t value);
    yona_rt_closure_set_cap(closure, 0, (int64_t)(intptr_t)st);

    /* Wrap in Iterator ADT: non-recursive, {tag=0, closure_ptr} */
    int64_t* iter_adt = (int64_t*)rc_alloc(4 /* RC_TYPE_ADT */, 2 * sizeof(int64_t));
    iter_adt[0] = 0;  /* tag = Iterator (constructor 0) */
    iter_adt[1] = (int64_t)(intptr_t)closure;

    return (int64_t)(intptr_t)iter_adt;
}
