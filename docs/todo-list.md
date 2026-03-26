# TODO List - Yona-LLVM

## Summary
- **Compiler**: Yona → LLVM IR → native executable via `yonac`
- **Test Coverage**: 35 tests, 441 assertions (codegen E2E + ADT tests)
- **Type System**: Symbol interning, Dict/Set, ADTs, monomorphization via deferred compilation

## Completed

- Lexer, Parser, AST (newline-aware, juxtaposition, string interpolation)
- LLVM codegen with TypedValue (type-directed, CType tags propagate)
- Deferred function compilation at call sites (monomorphization)
- Lambda lifting, higher-order functions, partial application
- Case expressions (integer, symbol, variable, wildcard, head-tail, tuple, or-pattern, constructor)
- Tuples (LLVM structs), sequences (runtime heap arrays)
- Symbol interning (i64 IDs, icmp eq comparison)
- Dict/Set construction with typed elements
- Algebraic data types (`type Option a = Some a | None`)
- Module compilation with C-ABI exports, cross-language linking
- Module type metadata (pattern + body-based type inference)
- Async codegen (CType::PROMISE, thread pool, auto-await, parallel let)
- Optimization passes (TCE, constant folding, GVN, DCE)
- `extern` / `extern async` for C FFI

## Remaining Work

### ADTs
- [x] Non-recursive ADTs: flat struct {i8 tag, payload} ✅
- [x] Recursive ADTs: heap-allocated nodes, runtime alloc/access ✅
- [x] Stdlib migration: Option, Result rewritten with ADTs ✅
- [x] Interface files (.yonai) for cross-module linking ✅
- [ ] Exhaustiveness checking (warnings)

### Cross-Module
- [x] Interface files (.yonai) for type-safe linking ✅

### Records
- [ ] Named LLVM structs for record types
- [ ] Field access, functional update, record patterns

### Type Checking
- [ ] Compile-time type error checking (validate during codegen, clear error messages)
- [ ] Homogeneous collection enforcement

### Tooling
- [ ] Compiler error messages (clear, actionable, with source locations)
- [ ] REPL (compile-and-run mode)

### Stdlib
- [ ] Yona-written stdlib modules compiled with yonac
- [ ] Exception utilities, stack traces
- [ ] HTTP client with TLS

### Future
- [ ] LLVM coroutine intrinsics (replace thread pool)
- [ ] Whole-program optimization across module boundaries
- [ ] Incremental compilation
- [ ] Type classes (dictionary passing)
