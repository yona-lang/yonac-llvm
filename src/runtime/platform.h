/*
 * Platform abstraction layer for Yona runtime.
 *
 * Each function has a single portable interface. Implementations live in
 * platform/<os>.c and are selected at build time. Standard POSIX calls
 * are used directly in this header when they're portable across all targets.
 *
 * Adding a new platform:
 *   1. Create src/runtime/platform/<os>.c
 *   2. Implement all yona_platform_* functions
 *   3. Add the file to CMakeLists.txt for the target OS
 */

#ifndef YONA_PLATFORM_H
#define YONA_PLATFORM_H

#include <stdint.h>
#include <stddef.h>

/* ===== io_uring I/O await ===== */

/* Generic io_uring completer — blocks until the specified operation completes,
 * then does type-specific post-processing (close fd, null-terminate, etc). */
int64_t yona_rt_io_await(int64_t uring_id);

/* ===== Async File I/O (submit-and-return) ===== */

/* Submit a file read to io_uring. Returns uring ID (0 on failure). */
int64_t yona_platform_read_file_submit(const char* path);

/* Submit a file write to io_uring. Returns uring ID (0 on failure). */
int64_t yona_platform_write_file_submit(const char* path, const char* content);

/* ===== Filesystem ===== */

/* Read entire file into a malloc'd string. Returns NULL on error. */
char* yona_platform_read_file(const char* path);

/* Write string to file (truncate). Returns 0 on success, -1 on error. */
int yona_platform_write_file(const char* path, const char* content);

/* Append string to file. Returns 0 on success, -1 on error. */
int yona_platform_append_file(const char* path, const char* content);

/* Returns 1 if path exists, 0 otherwise. */
int yona_platform_file_exists(const char* path);

/* Remove a file. Returns 0 on success, -1 on error. */
int yona_platform_remove_file(const char* path);

/* File size in bytes. Returns -1 on error. */
int64_t yona_platform_file_size(const char* path);

/* List directory entries. Returns a yona seq of strings (rc_alloc'd).
 * Caller owns the returned sequence. */
int64_t* yona_platform_list_dir(const char* path);

/* ===== Console I/O ===== */

/* Read a line from stdin. Returns rc_alloc'd string (without newline). */
char* yona_platform_read_line(void);

/* ===== Process ===== */

/* Get environment variable. Returns rc_alloc'd string or empty string. */
char* yona_platform_getenv(const char* name);

/* Get current working directory. Returns rc_alloc'd string. */
char* yona_platform_getcwd(void);

#endif /* YONA_PLATFORM_H */
