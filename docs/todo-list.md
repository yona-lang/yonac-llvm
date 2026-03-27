# TODO List - Yona-LLVM

## Summary
- **Compiler**: Yona → LLVM IR → native executable via `yonac`
- **REPL**: `yona` — compile-and-run interactive mode
- **Tests**: 216 codegen assertions
- **Stdlib**: Option, Result, List, Tuple, Range, Test (45 exported functions)

## Roadmap

### Phase 1: Make the Language Usable
- [ ] Pipe operators (`|>`, `<|`) — parsed but not compiled, desugar to function application
- [ ] String stdlib (split, join, trim, replace, indexOf, substring, startsWith, endsWith, contains, charAt)
- [ ] File IO stdlib (readFile, writeFile, appendFile, readLine, print, println, fileExists, deleteFile)
- [ ] Async IO — wrap blocking IO with `extern async` for transparent async via thread pool
- [ ] Inlining + SROA optimization passes — inline small functions, decompose ADT structs to registers
- [ ] Generators/comprehensions — parsed but not compiled (`[x * 2 for x in list]`)
- [ ] Reference counting GC — every heap allocation currently leaks (15 mallocs, 0 frees)
- [ ] Math stdlib expansion (pow, floor, ceil, round, log, log10, exp, tan, atan, pi, e)
- [ ] Types/conversion stdlib (intToString, floatToString, boolToString)

### Phase 2: Language Completeness
- [ ] Case guard expressions (`case x of n | n > 0 -> "positive" end`)
- [ ] `with` expression (resource management)
- [ ] Module system improvements (wildcard import, re-exports, hierarchical search)
- [ ] Error propagation operator (`?`) for Result types
- [ ] Lazy sequences / iterators
- [ ] Multi-arg async support (struct-packing for thread pool calls)

### Phase 3: Codegen Optimizations
- [ ] Inlining pass (AlwaysInline + function inlining for small functions)
- [ ] SROA (Scalar Replacement of Aggregates — decompose ADT structs to registers)
- [ ] Loop optimizations (LICM, unrolling, vectorization for generators)
- [ ] Dead argument elimination (unused wildcard params)
- [ ] LTO (link-time optimization across modules)
- [ ] Mutual tail call optimization (f→g→f recursion)
- [ ] Escape analysis (stack-allocate ADTs that don't escape)

### Phase 4: Tooling & Developer Experience
- [ ] LLVM debug info (DWARF) — source-level debugging with gdb/lldb
- [ ] Richer error messages — show source line, underline error span, suggest fixes
- [ ] Package manager / build system (yona.toml, dependency resolution)
- [ ] LSP server (editor integration, go-to-definition, hover types)

### Phase 5: Advanced Type System
- [ ] Type annotations (optional type signatures on functions)
- [ ] Traits / type classes (ad-hoc polymorphism via dictionary passing)
- [ ] Cross-module generics (ship AST in .yonai for importer monomorphization)

## Completed
- Lexer, Parser, AST (newline-aware, juxtaposition, string interpolation)
- LLVM codegen with TypedValue (type-directed, CType tags, monomorphization)
- Lambda lifting, higher-order functions, partial application
- Case expressions (integer, symbol, wildcard, head-tail, tuple, constructor, or-pattern)
- Symbol interning (i64 IDs, icmp eq comparison)
- Dict/Set construction with typed elements
- ADTs: non-recursive (flat struct) and recursive (heap nodes)
- ADT named fields (construction, dot access, functional update, named patterns)
- ADT constructors as first-class functions
- Exhaustiveness checking for ADT case expressions
- Interface files (.yonai) for cross-module type-safe linking
- Module compilation with C-ABI exports
- Module type metadata (pattern + body-based inference)
- Exception handling (raise/try/catch via setjmp/longjmp, ADT exceptions)
- Stack traces on unhandled exceptions (Linux/macOS/Windows)
- Async codegen (CType::PROMISE, thread pool, auto-await, parallel let)
- Optimization passes (TCE, constant folding, GVN, DCE)
- `extern` / `extern async` for C FFI
- Compiler error messages with source locations and type validation
- REPL (compile-and-run mode)
- All 6 stdlib modules compile: Option (7), Result (8), List (17), Tuple (4), Range (6), Test (3)
