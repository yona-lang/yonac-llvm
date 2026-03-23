# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

### Prerequisites

- LLVM 16+ (Fedora: `sudo dnf install llvm llvm-devel llvm-libs cmake ninja-build`)
- CMake 3.10+, Ninja
- C++23 capable compiler

### Configure and build (Linux)
```bash
cmake --preset x64-debug-linux
cmake --build --preset build-debug-linux
```
Replace `linux` with `macos` or omit the suffix for Windows. Replace `debug` with `release` for release builds.

### Run tests
```bash
# Via CTest
ctest --preset unit-tests-linux

# Directly (with required env var)
YONA_PATH=test/code ./out/build/x64-debug-linux/tests

# Run a specific test (doctest subcase filter)
./out/build/x64-debug-linux/tests -tc="TestName"
```

### Format code
```bash
./scripts/format.sh  # runs clang-format on include/, src/, test/, cli/
```

## Architecture Overview

Yona language compiler/interpreter using LLVM as the backend. Traditional compiler pipeline: Lexer → Parser → AST → Interpreter (tree-walking) / Codegen (LLVM IR, WIP).

### Key Design Patterns

**Newline-aware lexer**: Newlines are significant tokens (`YNEWLINE`) that delimit expressions in case arms, do-blocks, and module function bodies. Semicolons are equivalent to newlines. Inside brackets (`()`, `[]`, `{}`), newlines are suppressed (treated as whitespace). After binary operators and continuation tokens (`->`, `=`, `,`), the following newline is suppressed to allow natural line continuation. This enables juxtaposition-based function application (`f x y` instead of requiring `f(x, y)`) without ambiguity at expression boundaries.

**Visitor pattern with generic return types**: `AstVisitor<ResultType>` is a templated base class. The Interpreter uses `AstVisitor<InterpreterResult>`, the TypeChecker uses its own result type, etc. AST nodes implement `accept()` that dispatches to the correct `visit()` overload.

**Variant-based runtime values**: `RuntimeObject` wraps a `RuntimeObjectData` variant holding primitives, symbols, collections (seq, set, dict, tuple, record), FQNs, modules, functions, promises, and applied values. All complex values use `shared_ptr`.

**Promise-aware type system**: `PromiseType` in `types.h` wraps an inner type. During unification, `Promise<T>` auto-coerces to `T` — the type checker knows where awaits are needed without user-visible `async`/`await` keywords. Native async functions are marked with `is_async` flag and return promises that are auto-awaited at binary operators, function arguments, conditions, and pattern match targets.

**Frame-based scoping**: `Frame<T>` (in `common.h`) is a generic scope with parent-pointer chaining, used for lexical scoping. New frames are pushed for function calls, let expressions, and pattern matching. Pattern matching creates a temporary frame for bindings, then merges to parent on success.

**LLVM Codegen** (`src/Codegen.cpp`): Type-directed code generation using `TypedValue = {Value*, CType}`. Every codegen method returns both an LLVM value and its type tag. Functions are compiled with deferred compilation — stored as AST at definition, compiled at call site where argument types are known. Closures use lambda lifting (free variables become extra parameters). The `yonac` CLI compiles Yona source to native executables via LLVM.

### Core Components

- **AST** (`include/ast.h`): Large node hierarchy rooted at `AstNode`. Major branches: `ExprNode` (expressions), `PatternNode` (pattern matching), `ScopedNode` (scope-creating). Each node tracks `SourceContext` for error reporting.
- **Codegen** (`src/Codegen.cpp`): LLVM IR generation with `TypedValue` system. Supports literals, arithmetic, functions, closures, recursion, case expressions, tuples, sequences, symbols, or-patterns, higher-order functions. Compiled runtime in `src/compiled_runtime.c`.
- **Interpreter** (`src/Interpreter.cpp`): Tree-walking interpreter with `InterpreterState` holding the frame stack, module cache, exception state, and generator context.
- **Type System** (`include/types.h`): Variant-based types including builtins, function types, product/sum types, record types, and named types. Supports Hindley-Milner inference via `TypeChecker`.
- **Pattern Matching**: `CaseExpr` contains a target expression and vector of `CaseClause(pattern, body)`. Pattern types include value, tuple, seq, head-tail (`[h|t]`), dict, record, `as` binding (`@`), and or-patterns.
- **Module System**: FQN-based with filesystem path resolution. `YONA_PATH` env var sets search paths. Modules are cached after first load. `ModuleValue` holds exports map, record type metadata, and keeps the AST alive.
- **Native Modules** (`src/stdlib/`, `include/stdlib/`): C++ implementations registered via `NativeModuleRegistry` singleton. 19 modules: Math, IO, System, List, Option, Result, Tuple, Range, String, Set, Dict, Timer, Http, Json, Regexp, File, Random, Time, Types.
- **Functions**: `FunctionValue` tracks FQN, arity, implementation lambda, partial args (for currying), `is_native` and `is_async` flags. Partial application is built-in. Zero-arity functions auto-evaluate (strict semantics); use `\-> expr` for thunks.
- **C Embedding API** (`include/yona_api.h`, `src/yona_api.cpp`): Stable C interface with `extern "C"` linkage. Opaque handles (`yona_interp_t`, `yona_value_t`), eval/call/inspect functions, native function registration with userdata for host-provided callbacks.

### Build Targets

- `yona_lib` (shared) / `yona_lib_static`: Core library (lexer, parser, AST, interpreter, codegen)
- `yona`: REPL executable (links `yona_lib` + CLI11)
- `yonac`: Compiler executable (links `yona_lib` + CLI11)
- `tests`: Test executable (links `yona_lib_static` + doctest)

### Dependencies

- **LLVM 16+**: Code generation backend
- **CLI11**: Command-line argument parsing (fetched via FetchContent)
- **doctest**: Testing framework (fetched via FetchContent)

### Development Workflow

- New language features require changes across: Lexer → Parser → AST → Interpreter (and eventually Codegen)
- AST modifications must update both the header (`ast.h`) and the visitor accept/visit methods
- Pattern matching requires careful frame management — bindings go in a child frame, merged on success
- Tests are in `test/` with test fixture Yona source files in `test/code/`
- The `YONA_PATH` environment variable must point to `test/code/` when running tests (set automatically by CTest)
