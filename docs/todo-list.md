# TODO List - Yona-LLVM

## Summary
- **Compiler**: Yona → LLVM IR → native executable via `yonac`
- **Tests**: 35 tests, 441 assertions (codegen E2E + ADT tests)
- **Type System**: ADTs, symbol interning, Dict/Set, monomorphization, .yonai interface files

## Completed

- Lexer, Parser, AST (newline-aware, juxtaposition, string interpolation)
- LLVM codegen with TypedValue (type-directed, CType tags)
- Deferred function compilation (monomorphization at call sites)
- Lambda lifting, higher-order functions, partial application
- Case expressions (integer, symbol, wildcard, head-tail, tuple, constructor, or-pattern)
- Symbol interning (i64 IDs, icmp eq)
- Dict/Set construction with typed elements
- ADTs: non-recursive (flat struct) and recursive (heap nodes)
- ADT constructors as first-class functions
- Interface files (.yonai) for cross-module type-safe linking
- Stdlib: Option and Result using ADTs
- Module compilation with C-ABI exports
- Module type metadata (pattern + body-based inference)
- Async codegen (CType::PROMISE, thread pool, auto-await)
- Optimization passes (TCE, constant folding, GVN, DCE)
- `extern` / `extern async` for C FFI

## Remaining Work

### Exhaustiveness Checking
- [x] Warn when case expression doesn't cover all ADT constructors ✅

### Named ADT Fields
- [x] Named field syntax: `type Person = Person { name : String, age : Int }` ✅
- [x] Dot syntax field access: `p.name` ✅
- [x] Named construction: `Person { name = 42, age = 30 }` ✅
- [x] Functional update: `p { age = 31 }` ✅
- [x] Named field patterns: `case p of Person { age = a } -> a end` ✅

### Type Checking & Error Messages
- [x] Error reporting with source locations (file:line:col) ✅
- [x] Homogeneous collection enforcement ✅
- [x] Arithmetic type validation ✅
- [x] Undefined variable/function errors ✅

### Tooling
- [x] REPL: `yona` executable — compile-and-run mode ✅

### Stdlib
- [x] All 6 stdlib modules compile: Option (7), Result (8), List (17), Tuple (4), Range (6), Test (3) ✅
- [x] Exception handling: raise/try/catch via setjmp/longjmp ✅
- [x] Stack traces: backtrace on unhandled exceptions (Linux/macOS/Windows) ✅
