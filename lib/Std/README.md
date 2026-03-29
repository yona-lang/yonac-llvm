# Yona Standard Library

## Module Overview

| Module | Type | Functions | Description |
|--------|------|-----------|-------------|
| `Std\Option` | Pure Yona | 10 | Optional values (`Some a \| None`) |
| `Std\Result` | Pure Yona | 11 | Error handling (`Ok a \| Err e`) |
| `Std\List` | Pure Yona | 28 | Sequence operations |
| `Std\Tuple` | Pure Yona | 9 | 2-tuple operations |
| `Std\Range` | Pure Yona | 11 | Lazy integer ranges |
| `Std\Math` | Pure Yona | 11 | Integer arithmetic |
| `Std\Pair` | Pure Yona | 10 | ADT-based pairs with named fields |
| `Std\Bool` | Pure Yona | 7 | Boolean combinators |
| `Std\Test` | Pure Yona | 6 | Test assertions |
| `Std\String` | C runtime | 18 | String operations (via compiled_runtime.c) |
| `Std\Types` | C runtime | 5 | Type conversions (via compiled_runtime.c) |

## Documentation

Doc comments use `##` prefix in source files. Run `python3 scripts/gendocs.py`
to generate markdown API reference in `docs/api/`.

## Architecture

### Pure Yona modules (`.yona`)
Written entirely in Yona. Compiled to `.o` + `.yonai` like user modules.
No platform-specific code.

### C runtime modules (`.yonai` only)
Interface files point to C functions in `src/compiled_runtime.c`.
These handle string operations, type conversions, and will handle I/O.

### Platform-specific code (future)
I/O modules (`Std\IO`, `Std\File`, `Std\Net`) will need platform-specific
implementations. These should be separated into:

```
src/runtime/
  compiled_runtime.c      # Platform-independent runtime
  platform/
    linux.c               # Linux: epoll, io_uring
    macos.c               # macOS: kqueue
    windows.c             # Windows: IOCP
  io_common.c             # Shared I/O abstractions
```

The build system selects the appropriate platform file. The `.yonai` interface
is identical across platforms — only the C implementation differs.
