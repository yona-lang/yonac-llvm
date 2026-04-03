/*
 * Linux OS operations — console I/O, process, environment.
 */

#include "../platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern void* yona_rt_rc_alloc_string(size_t bytes);

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

/* exec: run a shell command, capture stdout, return as string.
 * Returns a tuple-like struct: (exit_code, stdout_string) encoded as
 * two return values via the Yona convention (returns the string,
 * exit code available via a separate call). For simplicity, returns
 * just the stdout string. Exit code is the return value of pclose. */
char* yona_platform_exec(const char* cmd) {
    FILE* pipe = popen(cmd, "r");
    if (!pipe) {
        char* r = (char*)yona_rt_rc_alloc_string(1);
        r[0] = '\0';
        return r;
    }

    /* Read all output into a dynamically growing buffer */
    size_t capacity = 4096;
    size_t len = 0;
    char* buf = (char*)malloc(capacity);

    while (1) {
        size_t n = fread(buf + len, 1, capacity - len, pipe);
        if (n == 0) break;
        len += n;
        if (len >= capacity) {
            capacity *= 2;
            buf = (char*)realloc(buf, capacity);
        }
    }
    pclose(pipe);

    /* Strip trailing newline */
    if (len > 0 && buf[len - 1] == '\n') len--;
    if (len > 0 && buf[len - 1] == '\r') len--;

    char* r = (char*)yona_rt_rc_alloc_string(len + 1);
    memcpy(r, buf, len);
    r[len] = '\0';
    free(buf);
    return r;
}

/* execWithStatus: run command, return exit code as i64 */
int64_t yona_platform_exec_status(const char* cmd) {
    int status = system(cmd);
    if (status == -1) return -1;
    return WEXITSTATUS(status);
}

/* setenv */
int64_t yona_platform_setenv(const char* name, const char* value) {
    return setenv(name, value, 1) == 0 ? 1 : 0;
}

/* hostname */
char* yona_platform_hostname(void) {
    char buf[256];
    if (gethostname(buf, sizeof(buf)) != 0) buf[0] = '\0';
    size_t len = strlen(buf);
    char* r = (char*)yona_rt_rc_alloc_string(len + 1);
    memcpy(r, buf, len + 1);
    return r;
}
