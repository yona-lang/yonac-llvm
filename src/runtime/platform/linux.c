/*
 * Linux platform implementation for Yona runtime.
 *
 * Uses io_uring for async I/O (Linux 5.6+), raw syscalls (no liburing dependency).
 * Falls back to synchronous POSIX calls for platforms without io_uring.
 */

#include "../platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdatomic.h>
#include <sys/syscall.h>
#include <sys/mman.h>
#include <linux/io_uring.h>

/* Forward declaration: rc_alloc from compiled_runtime.c */
#ifndef YONA_PLATFORM_INCLUDED
extern void* yona_rt_rc_alloc_string(size_t bytes);
extern int64_t* yona_rt_seq_alloc(int64_t count);
#endif

/* ===== io_uring raw syscall interface ===== */

static int io_uring_setup(unsigned entries, struct io_uring_params *p) {
    return (int)syscall(__NR_io_uring_setup, entries, p);
}

static int io_uring_enter(int ring_fd, unsigned to_submit, unsigned min_complete,
                          unsigned flags, void* sig) {
    return (int)syscall(__NR_io_uring_enter, ring_fd, to_submit, min_complete, flags, sig, 0);
}

/* Ring state */
typedef struct {
    int ring_fd;
    /* Submission queue */
    unsigned *sq_head, *sq_tail, *sq_ring_mask, *sq_ring_entries;
    unsigned *sq_array;
    struct io_uring_sqe *sqes;
    void *sq_ring_ptr;
    size_t sq_ring_sz;
    /* Completion queue */
    unsigned *cq_head, *cq_tail, *cq_ring_mask, *cq_ring_entries;
    struct io_uring_cqe *cqes;
    void *cq_ring_ptr;
    size_t cq_ring_sz;
    /* Tracking */
    atomic_uint_fast64_t next_id;
    int initialized;
} yona_ring_t;

static yona_ring_t yona_ring = {0};
static pthread_mutex_t yona_ring_mutex = PTHREAD_MUTEX_INITIALIZER;

static int yona_ring_init(void) {
    if (yona_ring.initialized) return 0;

    struct io_uring_params params;
    memset(&params, 0, sizeof(params));

    int fd = io_uring_setup(256, &params);
    if (fd < 0) return -1;

    yona_ring.ring_fd = fd;

    /* Map submission queue ring buffer */
    size_t sq_ring_sz = params.sq_off.array + params.sq_entries * sizeof(unsigned);
    void *sq_ptr = mmap(0, sq_ring_sz, PROT_READ | PROT_WRITE,
                        MAP_SHARED | MAP_POPULATE, fd, IORING_OFF_SQ_RING);
    if (sq_ptr == MAP_FAILED) { close(fd); return -1; }

    yona_ring.sq_ring_ptr = sq_ptr;
    yona_ring.sq_ring_sz = sq_ring_sz;
    yona_ring.sq_head = sq_ptr + params.sq_off.head;
    yona_ring.sq_tail = sq_ptr + params.sq_off.tail;
    yona_ring.sq_ring_mask = sq_ptr + params.sq_off.ring_mask;
    yona_ring.sq_ring_entries = sq_ptr + params.sq_off.ring_entries;
    yona_ring.sq_array = sq_ptr + params.sq_off.array;

    /* Map SQEs */
    size_t sqes_sz = params.sq_entries * sizeof(struct io_uring_sqe);
    yona_ring.sqes = mmap(0, sqes_sz, PROT_READ | PROT_WRITE,
                          MAP_SHARED | MAP_POPULATE, fd, IORING_OFF_SQES);
    if (yona_ring.sqes == MAP_FAILED) {
        munmap(sq_ptr, sq_ring_sz);
        close(fd);
        return -1;
    }

    /* Map completion queue ring buffer */
    size_t cq_ring_sz = params.cq_off.cqes + params.cq_entries * sizeof(struct io_uring_cqe);
    void *cq_ptr;
    if (params.features & IORING_FEAT_SINGLE_MMAP) {
        cq_ptr = sq_ptr; /* CQ shares the same mmap */
        cq_ring_sz = sq_ring_sz;
    } else {
        cq_ptr = mmap(0, cq_ring_sz, PROT_READ | PROT_WRITE,
                       MAP_SHARED | MAP_POPULATE, fd, IORING_OFF_CQ_RING);
        if (cq_ptr == MAP_FAILED) {
            munmap(yona_ring.sqes, sqes_sz);
            munmap(sq_ptr, sq_ring_sz);
            close(fd);
            return -1;
        }
    }

    yona_ring.cq_ring_ptr = cq_ptr;
    yona_ring.cq_ring_sz = cq_ring_sz;
    yona_ring.cq_head = cq_ptr + params.cq_off.head;
    yona_ring.cq_tail = cq_ptr + params.cq_off.tail;
    yona_ring.cq_ring_mask = cq_ptr + params.cq_off.ring_mask;
    yona_ring.cq_ring_entries = cq_ptr + params.cq_off.ring_entries;
    yona_ring.cqes = cq_ptr + params.cq_off.cqes;

    atomic_init(&yona_ring.next_id, 1);
    yona_ring.initialized = 1;
    return 0;
}

/* Submit a single SQE and return its user_data id */
static uint64_t ring_submit_sqe(struct io_uring_sqe *sqe_template) {
    pthread_mutex_lock(&yona_ring_mutex);

    if (!yona_ring.initialized && yona_ring_init() != 0) {
        pthread_mutex_unlock(&yona_ring_mutex);
        return 0;
    }

    unsigned tail = *yona_ring.sq_tail;
    unsigned mask = *yona_ring.sq_ring_mask;
    unsigned idx = tail & mask;

    uint64_t id = atomic_fetch_add(&yona_ring.next_id, 1);
    struct io_uring_sqe *sqe = &yona_ring.sqes[idx];
    *sqe = *sqe_template;
    sqe->user_data = id;

    yona_ring.sq_array[idx] = idx;
    __atomic_store_n(yona_ring.sq_tail, tail + 1, __ATOMIC_RELEASE);

    io_uring_enter(yona_ring.ring_fd, 1, 0, 0, NULL);

    pthread_mutex_unlock(&yona_ring_mutex);
    return id;
}

/* Wait for a specific completion by user_data id */
static int32_t ring_await(uint64_t id) {
    for (;;) {
        pthread_mutex_lock(&yona_ring_mutex);

        unsigned head = __atomic_load_n(yona_ring.cq_head, __ATOMIC_ACQUIRE);
        unsigned tail = __atomic_load_n(yona_ring.cq_tail, __ATOMIC_ACQUIRE);

        while (head != tail) {
            unsigned mask = *yona_ring.cq_ring_mask;
            struct io_uring_cqe *cqe = &yona_ring.cqes[head & mask];
            if (cqe->user_data == id) {
                int32_t res = cqe->res;
                __atomic_store_n(yona_ring.cq_head, head + 1, __ATOMIC_RELEASE);
                pthread_mutex_unlock(&yona_ring_mutex);
                return res;
            }
            head++;
        }
        /* Not found yet — wait for more completions */
        pthread_mutex_unlock(&yona_ring_mutex);
        io_uring_enter(yona_ring.ring_fd, 0, 1, IORING_ENTER_GETEVENTS, NULL);
    }
}

/* ===== io_uring-based file operations ===== */

int64_t yona_platform_read_file_uring(const char* path, char** out_buf) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;

    struct stat st;
    if (fstat(fd, &st) < 0) { close(fd); return -1; }
    int64_t size = (int64_t)st.st_size;

    char* buf = (char*)yona_rt_rc_alloc_string((size_t)size + 1);

    struct io_uring_sqe sqe;
    memset(&sqe, 0, sizeof(sqe));
    sqe.opcode = IORING_OP_READ;
    sqe.fd = fd;
    sqe.addr = (unsigned long)buf;
    sqe.len = (unsigned)size;
    sqe.off = 0;

    uint64_t id = ring_submit_sqe(&sqe);
    int32_t res = ring_await(id);
    close(fd);

    if (res < 0) { buf[0] = '\0'; *out_buf = buf; return -1; }
    buf[res] = '\0';
    *out_buf = buf;
    return res;
}

int64_t yona_platform_write_file_uring(const char* path, const char* content) {
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return -1;

    size_t len = strlen(content);
    struct io_uring_sqe sqe;
    memset(&sqe, 0, sizeof(sqe));
    sqe.opcode = IORING_OP_WRITE;
    sqe.fd = fd;
    sqe.addr = (unsigned long)content;
    sqe.len = (unsigned)len;
    sqe.off = 0;

    uint64_t id = ring_submit_sqe(&sqe);
    int32_t res = ring_await(id);
    close(fd);

    return (res == (int32_t)len) ? 0 : -1;
}

/* ===== POSIX fallback implementations ===== */

char* yona_platform_read_file(const char* path) {
    char* buf = NULL;
    int64_t res = yona_platform_read_file_uring(path, &buf);
    if (res >= 0 && buf) return buf;
    /* Fallback to stdio if io_uring fails */
    FILE* f = fopen(path, "rb");
    if (!f) {
        if (!buf) buf = (char*)yona_rt_rc_alloc_string(1);
        buf[0] = '\0';
        return buf;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size < 0) { fclose(f); buf = (char*)yona_rt_rc_alloc_string(1); buf[0] = '\0'; return buf; }
    if (!buf) buf = (char*)yona_rt_rc_alloc_string((size_t)size + 1);
    size_t read = fread(buf, 1, (size_t)size, f);
    buf[read] = '\0';
    fclose(f);
    return buf;
}

int yona_platform_write_file(const char* path, const char* content) {
    int64_t res = yona_platform_write_file_uring(path, content);
    if (res == 0) return 0;
    /* Fallback */
    FILE* f = fopen(path, "wb");
    if (!f) return -1;
    size_t len = strlen(content);
    size_t written = fwrite(content, 1, len, f);
    fclose(f);
    return (written == len) ? 0 : -1;
}

int yona_platform_append_file(const char* path, const char* content) {
    FILE* f = fopen(path, "ab");
    if (!f) return -1;
    size_t len = strlen(content);
    size_t written = fwrite(content, 1, len, f);
    fclose(f);
    return (written == len) ? 0 : -1;
}

int yona_platform_file_exists(const char* path) {
    struct stat st;
    return (stat(path, &st) == 0) ? 1 : 0;
}

int yona_platform_remove_file(const char* path) {
    return (remove(path) == 0) ? 0 : -1;
}

int64_t yona_platform_file_size(const char* path) {
    struct stat st;
    if (stat(path, &st) != 0) return -1;
    return (int64_t)st.st_size;
}

int64_t* yona_platform_list_dir(const char* path) {
    DIR* dir = opendir(path);
    if (!dir) return yona_rt_seq_alloc(0);

    int64_t count = 0;
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.' &&
            (entry->d_name[1] == '\0' ||
             (entry->d_name[1] == '.' && entry->d_name[2] == '\0')))
            continue;
        count++;
    }

    rewinddir(dir);
    int64_t* seq = yona_rt_seq_alloc(count);
    int64_t i = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.' &&
            (entry->d_name[1] == '\0' ||
             (entry->d_name[1] == '.' && entry->d_name[2] == '\0')))
            continue;
        size_t len = strlen(entry->d_name);
        char* name = (char*)yona_rt_rc_alloc_string(len + 1);
        memcpy(name, entry->d_name, len + 1);
        seq[1 + i] = (int64_t)(intptr_t)name;
        i++;
    }

    closedir(dir);
    return seq;
}

/* ===== Console I/O ===== */

char* yona_platform_read_line(void) {
    char buf[4096];
    if (!fgets(buf, sizeof(buf), stdin)) {
        char* r = (char*)yona_rt_rc_alloc_string(1);
        r[0] = '\0';
        return r;
    }
    size_t len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n') buf[--len] = '\0';
    if (len > 0 && buf[len - 1] == '\r') buf[--len] = '\0';

    char* r = (char*)yona_rt_rc_alloc_string(len + 1);
    memcpy(r, buf, len + 1);
    return r;
}

/* ===== Process ===== */

char* yona_platform_getenv(const char* name) {
    const char* val = getenv(name);
    if (!val) val = "";
    size_t len = strlen(val);
    char* r = (char*)yona_rt_rc_alloc_string(len + 1);
    memcpy(r, val, len + 1);
    return r;
}

char* yona_platform_getcwd(void) {
    char buf[4096];
    if (!getcwd(buf, sizeof(buf))) buf[0] = '\0';
    size_t len = strlen(buf);
    char* r = (char*)yona_rt_rc_alloc_string(len + 1);
    memcpy(r, buf, len + 1);
    return r;
}
