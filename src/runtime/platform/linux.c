/*
 * Linux platform — aggregates all platform-specific implementations.
 *
 * When #included from compiled_runtime.c, all platform code is compiled
 * as a single translation unit. For separate compilation, build each
 * file_linux.c, net_linux.c, os_linux.c individually.
 *
 * io_uring infrastructure is shared via uring.h (header-only).
 */

#include "file_linux.c"
#include "net_linux.c"
#include "os_linux.c"
