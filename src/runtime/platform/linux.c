/*
 * Linux platform implementation for Yona runtime.
 *
 * Uses standard POSIX calls (also compatible with macOS).
 * Linux-specific extensions (io_uring, epoll) will go here when needed.
 */

#include "../platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <stdint.h>

/* When compiled standalone, these are extern. When #included from
 * compiled_runtime.c, they're already defined above. */
#ifndef YONA_PLATFORM_INCLUDED
extern void* yona_rt_rc_alloc_string(size_t bytes);
extern int64_t* yona_rt_seq_alloc(int64_t count);
#endif

/* ===== Filesystem ===== */

char* yona_platform_read_file(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size < 0) { fclose(f); return NULL; }

    char* buf = (char*)yona_rt_rc_alloc_string((size_t)size + 1);
    size_t read = fread(buf, 1, (size_t)size, f);
    buf[read] = '\0';
    fclose(f);
    return buf;
}

int yona_platform_write_file(const char* path, const char* content) {
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

    /* First pass: count entries (skip . and ..) */
    int64_t count = 0;
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.' &&
            (entry->d_name[1] == '\0' ||
             (entry->d_name[1] == '.' && entry->d_name[2] == '\0')))
            continue;
        count++;
    }

    /* Second pass: collect names */
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
        /* EOF or error — return empty string */
        char* r = (char*)yona_rt_rc_alloc_string(1);
        r[0] = '\0';
        return r;
    }
    /* Strip trailing newline */
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
