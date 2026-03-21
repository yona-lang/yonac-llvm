# Yona-LLVM Compiler

A compiler and interpreter for the [Yona programming language](https://yona-lang.org/), featuring transparent async, automatic parallelization, and a comprehensive standard library.

## Project Status

- ✅ **Interpreter**: Feature-complete with transparent async
- ✅ **Type System**: Hindley-Milner with Promise\<T\> coercion
- ✅ **Parser**: Newline-aware with juxtaposition function application
- ✅ **Async**: Promise-aware type system, parallel let bindings, type-directed auto-await
- ✅ **Standard Library**: 19 native modules, 150+ functions
- ✅ **C Embedding API**: Stable C interface for FFI integration
- ✅ **Test Coverage**: 336 tests, 1407 assertions, 100% passing
- 📋 **Planned**: Sandboxing, LLVM backend

## Features

- **Transparent Async** — no async/await keywords; Promise\<T\> auto-coerces to T at use sites
- **Auto-Parallelization** — independent let bindings execute in parallel via thread pool
- **Newline-Aware Parsing** — newlines delimit expressions; semicolons for one-liners; brackets suppress newlines
- **Juxtaposition Application** — `f x y` function calls (Haskell-style), alongside `f(x, y)`
- **Pattern Matching** — wildcards, tuples, lists, head-tail, dicts, records, or-patterns, non-linear patterns, guards
- **Type Inference** — Hindley-Milner with polymorphism and Promise coercion
- **String Interpolation** — `"hello {name}"` with auto-conversion
- **Module System** — import/export with FQN, aliases, native + file-based modules
- **Currying** — automatic partial application
- **C Embedding API** — embed Yona in any language via FFI

## Setup

### Red Hat / Fedora

```bash
sudo dnf install llvm llvm-devel llvm-libs clang20 lld20 cmake ninja-build \
  libcxx-devel libcxxabi-devel
```

### macOS

```bash
brew install llvm cmake ninja
```

### Windows

Ensure Visual Studio 2022 or later is installed with C++ development tools.

## Building

```bash
# Configure (Linux)
cmake --preset x64-debug-linux

# Build
cmake --build --preset build-debug-linux

# Run tests
ctest --preset unit-tests-linux
```

Replace `linux` with `macos` or omit the suffix for Windows. Replace `debug` with `release` for release builds.

```bash
# Run specific test
YONA_PATH=test/code ./out/build/x64-debug-linux/tests -tc="TestName"

# Format code
./scripts/format.sh
```

## Language Examples

### Newlines and Semicolons

```yona
# Newlines delimit case arms and do-block expressions
case x of
  :ok -> handle_ok x
  :error -> handle_error x
  _ -> default_handler x
end

# Semicolons for one-liners
case x of :ok -> 1; :error -> 2; _ -> 0 end

# Newlines suppressed inside brackets
let list = [
  1, 2, 3,
  4, 5, 6
] in list
```

### Function Application

```yona
# Juxtaposition (space-separated)
println "Hello, World!"
map (\x -> x * 2) [1, 2, 3]
add 3 4

# Parenthesized
add(3, 4)
println("Hello")

# Partial application
let add5 = add 5 in add5 10  # Returns 15
```

### String Interpolation

```yona
let name = "World" in "Hello {name}!"
let x = 6 in "result is {(x * 7)}"
```

### Pattern Matching

```yona
case value of
  0 -> "zero"
  n | n > 0 -> "positive"
  _ -> "negative"
end

# OR patterns
case status of
  :ok | :success -> "good"
  :error | :failure -> "bad"
end

# Non-linear patterns
case (x, x) of
  (a, a) -> "equal"
  (a, b) -> "different"
end
```

### Transparent Async

```yona
# Independent bindings auto-parallelize
let
  a = read_file "foo.txt",
  b = read_file "bar.txt"
in
  a ++ b  # Both reads happen in parallel, auto-awaited here
```

### Modules

```yona
# Import specific functions
import map, filter from Std\List in
  filter (\x -> x > 0) (map (\x -> x - 1) [1, 2, 3])

# Import with alias
import Std\Math as M in
  M.abs(-42)
```

## Standard Library

| Module | Functions |
|--------|-----------|
| `Std\IO` | print, println, readFile, writeFile, appendFile, fileExists, deleteFile, readLine, readChar |
| `Std\Math` | sin, cos, tan, asin, acos, atan, atan2, exp, log, log10, pow, sqrt, ceil, floor, round, abs, pi, e, max, min, factorial |
| `Std\List` | map, filter, fold, foldl, foldr, length, head, tail, reverse, take, drop, flatten, zip, lookup, splitAt, any, all, contains, isEmpty |
| `Std\String` | length, toUpperCase, toLowerCase, trim, split, join, substring, indexOf, contains, replace, startsWith, endsWith, toString, chars |
| `Std\Dict` | get, put, remove, containsKey, keys, values, size, toList, fromList, merge, fold, map, filter, isEmpty, lookup |
| `Std\Set` | contains, add, remove, size, toList, fromList, union, intersection, difference, fold, map, filter, isEmpty |
| `Std\Option` | some, none, isSome, isNone, unwrapOr, map |
| `Std\Result` | ok, err, isOk, isErr, unwrapOr, map, mapErr |
| `Std\Tuple` | fst, snd, swap, mapBoth, zip, unzip |
| `Std\Range` | range, toList, contains, length, take, drop |
| `Std\Json` | parse, stringify |
| `Std\Regexp` | match, matchAll, replace, split, test |
| `Std\File` | exists, isDir, isFile, listDir, mkdir, remove, basename, dirname, extension, join, absolute |
| `Std\Random` | int, float, choice, shuffle |
| `Std\Time` | now, nowMillis, format, parse |
| `Std\Timer` | sleep (async), millis, nanos, measure |
| `Std\Types` | typeOf, isInt, isFloat, isString, isBool, isSeq, isSet, isDict, isTuple, isFunction, toInt, toFloat, toStr |
| `Std\Http` | get, post (async) |
| `Std\System` | env, exit |

## C Embedding API

Embed Yona in any application via the C API (`yona_api.h`):

```c
#include "yona_api.h"

// Create interpreter
yona_interp_t interp = yona_create();

// Evaluate Yona code
yona_value_t result;
yona_eval(interp, "1 + 2", &result);
printf("result: %d\n", yona_as_int(result));  // 3
yona_value_free(result);

// Register host-provided functions
yona_register_function(interp, "App\\Handlers", "notify", 2, my_notify_fn, context);

// Call from Yona code
yona_eval(interp, "import notify from App\\Handlers in notify \"team\" \"alert\"", &result);

yona_value_free(result);
yona_destroy(interp);
```

## Architecture

- **Lexer & Parser**: Recursive descent with Pratt parsing, newline-aware tokenization
- **AST**: Visitor pattern with comprehensive node types
- **Interpreter**: Tree-walking with frame-based execution, transparent async
- **Type Checker**: Hindley-Milner with Promise\<T\> coercion
- **Async Runtime**: Thread pool, dependency analyzer, auto-parallelization
- **C API**: Stable `extern "C"` interface with opaque handles

## Documentation

- [Language Syntax Reference](docs/language-syntax.md)
- [C Embedding API](docs/c-embedding-api.md)
- [LLVM Backend Plan](docs/llvm-backend-plan.md)
- [Module System](docs/module-system.md)
- [Type System](docs/type-system-implementation.md)
- [Async Implementation](docs/async-implementation-plan.md)
- [TODO List](docs/todo-list.md)
- [Project Roadmap](docs/project-roadmap.md)

## Development

See [CLAUDE.md](CLAUDE.md) for build details and architecture guide.
