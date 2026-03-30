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
