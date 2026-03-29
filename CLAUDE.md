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

# Directly
./out/build/x64-debug-linux/tests

# Run a specific test (doctest subcase filter)
./out/build/x64-debug-linux/tests -tc="TestName"
```

### Format code
```bash
./scripts/format.sh  # runs clang-format on include/, src/, test/, cli/
```

## Architecture Overview

Yona language compiler using LLVM. Pipeline: Lexer → Parser → AST → Codegen (LLVM IR) → Native executable.

### Key Design Patterns

**Newline-aware lexer**: Newlines are significant tokens (`YNEWLINE`) that delimit expressions in case arms, do-blocks, and module function bodies. Semicolons are equivalent to newlines. Inside brackets (`()`, `[]`, `{}`), newlines are suppressed (treated as whitespace). After binary operators and continuation tokens (`->`, `=`, `,`), the following newline is suppressed to allow natural line continuation. This enables juxtaposition-based function application (`f x y` instead of requiring `f(x, y)`) without ambiguity at expression boundaries.

**Visitor pattern with generic return types**: `AstVisitor<ResultType>` is a templated base class. AST nodes implement `accept()` that dispatches to the correct `visit()` overload.

**LLVM Codegen** (`src/Codegen.cpp`): Type-directed code generation using `TypedValue = {Value*, CType}`. Every codegen method returns both an LLVM value and its type tag. Functions are compiled with deferred compilation — stored as AST at definition, compiled at call site where argument types are known (monomorphization). Closures use env-passing convention: `{fn_ptr, ret_tag, arity, cap0, ...}` heap arrays, functions take `(ptr env, args...)`. The `yonac` CLI compiles Yona source to native executables via LLVM.

**Algebraic Data Types**: `type Option a = Some a | None` — named constructors with typed fields. ADT fields support function type signatures: `type Stream a = Next a (() -> Stream a) | End`. Constructors are first-class functions. Non-recursive ADTs use flat structs `{i64 tag, payload}`; recursive ADTs and ADTs with function fields are heap-allocated.

**Module System**: Modules are top-level declarations (`ModuleDecl`), not expressions. No `as`/`end` — modules end at EOF. `export` statements: `export func`, `export type Name`, `export f from Mod`. Compile to native object files with C-ABI exports. Interface files (`.yonai`) provide cross-module type metadata. Exported functions include source text in `.yonai` (`GENFN_BEGIN`/`GENFN_END`) for cross-module monomorphization — when call-site types differ from the pre-compiled signature, the source is re-parsed and compiled locally with actual types.

**Memory Management**: Reference counting with arena optimization. All heap objects (`SEQ`, `SET`, `DICT`, `ADT`, `FUNCTION`, `STRING`) use `rc_alloc` which prepends a `[refcount, type_tag]` header. Let bindings `rc_inc` the result and `rc_dec` bindings at scope exit. Closures `rc_inc` captured values. Function parameters use callee-borrows convention (no RC on params). Escape analysis (`include/EscapeAnalysis.h`) determines which let-bound values don't escape their scope; non-escaping values are bump-allocated from a per-scope arena (`yona_rt_arena_alloc`) with sentinel refcount (`INT64_MAX`) so `rc_dec` safely skips them, and the entire arena is freed in bulk at scope exit.

**Symbol interning**: Symbols (`:ok`, `:none`) are interned to `i64` IDs at compile time. Comparison is `icmp eq i64` (1 cycle). Pattern matching on symbols generates integer switch/select.

### Core Components

- **AST** (`include/ast.h`): Node hierarchy rooted at `AstNode`. Major branches: `ExprNode` (expressions), `PatternNode` (pattern matching), `ScopedNode` (scope-creating). Each node tracks `SourceContext` for error reporting.
- **Codegen** (`src/Codegen.cpp`): LLVM IR generation with `TypedValue` system. Supports literals, arithmetic, functions, closures, recursion, case expressions, tuples, sequences, symbols, sets, dicts, ADTs, or-patterns, higher-order functions, partial application, generators/comprehensions. Compiled runtime in `src/compiled_runtime.c`.
- **Type System** (`include/types.h`): Variant-based types including builtins, function types, product/sum types, record types, ADTs, and named types. The codegen uses `CType` + `TypedValue` with compile-time type inference and monomorphization.
- **Pattern Matching**: `CaseExpr` contains a target expression and vector of `CaseClause(pattern, body)`. Pattern types include value, tuple, seq, head-tail (`[h|t]`), dict, record, constructor (`Some x`), `as` binding (`@`), and or-patterns.
- **Module System**: FQN-based. Modules (`ModuleDecl` in AST) are top-level declarations, not expressions. They compile to object files with C-ABI exports via name mangling (`yona_Pkg_Mod__func`). Interface files (`.yonai`) for cross-module type-safe linking. Three import styles: selective, wildcard, FQN calls (`Mod::func`). Cross-module generics via GENFN source in `.yonai` — on-demand re-parse and monomorphization when call-site types differ. Trait instance methods have `ExternalLinkage` for cross-module trait dispatch.

### Build Targets

- `yona_lib` (shared) / `yona_lib_static`: Core library (lexer, parser, AST, codegen)
- `yonac`: Compiler executable (links `yona_lib` + CLI11)
- `yona`: REPL executable — compile-and-run mode (links `yona_lib`)
- `tests`: Test executable (links `yona_lib_static` + doctest)

### Dependencies

- **LLVM 16+**: Code generation backend
- **CLI11**: Command-line argument parsing (fetched via FetchContent)
- **doctest**: Testing framework (fetched via FetchContent)

### Development Workflow

- New language features require changes across: Lexer → Parser → AST → Codegen
- AST modifications must update the header (`ast.h`), visitor (`ast_visitor.h`), and codegen
- Tests are in `test/` — codegen E2E fixtures in `test/codegen/*.yona` + `*.expected`
- ADT tests in `test/adt_test.cpp`, trait/cross-module tests in `test/trait_test.cpp`
