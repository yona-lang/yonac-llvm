# Platform architecture (runtime)

## Goals

- **Linux**: io_uring–backed async file and network I/O (`file_linux.c`, `net_linux.c`), POSIX process APIs (`os_linux.c`).
- **Windows**: Native Win32 and UCRT (`file_windows.c`, `os_windows.c`, `net_windows.c`). Socket and file submit paths integrate with `yona_rt_io_await` via IOCP-backed completion where appropriate, with direct-result IDs retained for ordering-sensitive operations.
- **No pthread on Windows**: Async and channels live under `src/runtime/platform/` (`async_win32.c` / `channel_win32.c`) and are pulled in via `#include` from `compiled_runtime.c`.

## Public runtime headers

- `include/yona/runtime/platform.h` — portable `yona_platform_*` / process ABI.
- `include/yona/runtime/uring.h` — Linux-only io_uring helpers + shared `io_context_t` layout for Linux platform `.c` files.

## Frozen platform ABI (v1)

`platform.h` is now treated as a versioned runtime ABI contract
(`YONA_PLATFORM_ABI_VERSION=1`). New platforms should implement every symbol in
this grouped inventory:

- **Async submit/await**: `yona_rt_io_await`, `yona_platform_*_submit`,
  fd submit helpers, fd string writes, fd line reads.
- **Filesystem (path-based)**: `read/write/append`, existence, remove, size,
  directory listing.
- **Filesystem (handle-based)**: open/close/seek/tell/advance/flush/truncate.
- **Process and environment**: `getenv/getcwd/exec/exec_status/setenv/hostname`,
  `exit_process`, and process handle lifecycle (`spawn/readLine/readAll/wait/kill/writeStdin/closeStdin/pid/destroy`).
- **Console and platform constants**: console line read and constant providers
  (page size, cpu count, endianness, os name, arch).

Policy:

- ABI-breaking changes must increment `YONA_PLATFORM_ABI_VERSION`.
- Non-breaking additive changes require implementations in all active platform
  backends before merging.

## CMake selection

- All `src/runtime/platform/*.c` are **removed** from the globbed library sources, then the correct trio is **appended** per OS (`WIN32` vs non-Windows).
- Under `src/runtime/platform/`, the files `async_posix.c`, `async_win32.c`, `channel_posix.c`, and `channel_win32.c` are excluded from the library glob with the other platform `.c` sources because they are **included** from `compiled_runtime.c`, not compiled as separate TUs.
- Windows links **`ws2_32`** for Winsock.

## `compiled_runtime.c` boundary

- Shared Yona runtime (RC, stdlib shims, HTTP helpers, etc.) lives here.
- Platform entry points (`yona_platform_*`, `yona_Std_Net__*`, process spawn, `yona_rt_io_await`) live under `src/runtime/platform/` and are compiled as their own `.c` files.
- Async/channel implementations are **included** at the end of `compiled_runtime.c` so promise and channel types stay in one link unit: `_WIN32` → win32 sources; else → POSIX sources.
- Handle-oriented file operations used by `Std\File` in `compiled_runtime.c`
  now route through platform wrappers (open/close/seek/tell/flush/truncate),
  removing remaining per-OS `#if` branches from this path.

## Windows compile flags

- **`NOMINMAX`** and **`WIN32_LEAN_AND_MEAN`** are set in CMake for `WIN32` so Windows headers do not break C++ standard library min/max.

## `yona_io_register_direct_result`

- Shared helper used by `file_windows.c` and `net_windows.c`: registers an opaque pointer (or integer cast through `intptr_t`) for a high direct-result ID range; `yona_rt_io_await` in `file_windows.c` completes those immediately.
