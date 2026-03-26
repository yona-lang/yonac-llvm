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

### Records
- [ ] Named LLVM structs for record types
- [ ] Field access via extractvalue
- [ ] Functional update (copy struct, replace field)
- [ ] Record patterns in case expressions

### Type Checking
- [ ] Compile-time type error reporting (validate during codegen)
- [ ] Homogeneous collection enforcement
- [ ] Clear error messages with source locations

### Tooling
- [ ] Compiler error messages (clear, actionable)
- [ ] REPL (compile-and-run mode)

### Codegen Bugs
- [ ] Multi-line module function bodies: LLVM "referring to basic block in another function" error with multi-line case expressions in modules. Single-line functions work. Likely caused by function recreation (erased function's basic blocks referenced by new function).
- [ ] Cross-function monomorphization: when a module function calls another with different argument types than inferred defaults (e.g. `reverse` calling `fold` with SEQ accumulator instead of INT).

### Stdlib
- [ ] Compile full stdlib with yonac (List, Tuple, Range, Test) — blocked by multi-function module bug
- [ ] Exception utilities, stack traces
