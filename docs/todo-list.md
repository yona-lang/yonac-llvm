# TODO List - Yona-LLVM

## Summary
- **Compiler**: Yona → LLVM IR → native executable via `yonac`
- **REPL**: `yona` — compile-and-run interactive mode
- **Tests**: 222 codegen assertions
- **Stdlib**: Option, Result, List, Tuple, Range, Test (45 exported functions)

## Roadmap

### Standard Library (see docs/stdlib-plan.md for full design)

All I/O is non-blocking via `extern async`. Pure functions are sync.
15 modules, ~200 functions across 6 implementation phases.

Phase 1: Core types (Result, Option, List enhancements)
- [ ] Enhanced Result: flatMap, flatten, toOption, collect, sequence
- [ ] Enhanced Option: flatMap, filter, orElse, zip
- [ ] Enhanced List: sort, groupBy, partition, intersperse, scanl

Phase 2: String and Encoding
- [ ] Std\String — 20+ functions (split, join, trim, replace, pad, format, etc.)
- [ ] Std\Encoding — base64, hex, URL encoding, HTML escaping

Phase 3: I/O Foundation (all async)
- [ ] Std\IO — console (print, readLine, eprint)
- [ ] Std\File — filesystem (readFile, writeFile, listDir, watch, temp, paths)
- [ ] Std\Process — spawn, pipe, wait, kill, signals

Phase 4: Networking (all async)
- [ ] Std\Net — TCP/UDP sockets, listeners, connections
- [ ] Std\Http — full HTTP client (Request/Response ADTs, headers, streaming, timeouts) + server

Phase 5: Data Formats
- [ ] Std\Json — parse, stringify, query, pretty print
- [ ] Std\Crypto — SHA-256, HMAC, random bytes, UUID
- [ ] Std\Random — int, float, choice, shuffle

Phase 6: Concurrency
- [ ] Std\Concurrent — channels, semaphores, atomics, STM
- [ ] Std\Log — structured logging with levels and formatters
- [ ] Std\Collection — sorted map/set, priority queue, iterators

### Language Features
- [ ] Generators/comprehensions — parsed but not compiled (needs HOF return type inference for transducers)
- [x] Case guard expressions (`if` keyword) ✅
- [ ] `with` expression (resource management)
- [x] FQN calls without import (`Std\List::map fn seq`) ✅
- [x] Wildcard import (`import Std\List in expr`) ✅
- [x] ADT field names in .yonai (cross-module named field access) ✅
- [ ] Re-exports (module re-exporting from another module)
- [ ] Lazy sequences / iterators

### Codegen Optimizations
- [ ] Inlining pass (AlwaysInline + function inlining)
- [ ] SROA (decompose ADT structs to registers)
- [ ] Loop optimizations (LICM, unrolling, vectorization)
- [ ] Dead argument elimination
- [ ] LTO (link-time optimization)
- [ ] Mutual tail call optimization
- [ ] Escape analysis

### Memory Management
- [ ] Reference counting GC
- [ ] Arena allocator

### Tooling
- [ ] LLVM debug info (DWARF)
- [ ] Richer error messages (source line display)
- [ ] Package manager / build system
- [ ] LSP server

### Distribution & Packaging
- [ ] RPM package (Fedora/RHEL/CentOS)
- [ ] DEB package (Debian/Ubuntu)
- [ ] Homebrew formula (macOS)
- [ ] Windows installer (MSI or NSIS)
- [ ] Static binary releases (Linux/macOS/Windows) via GitHub Releases
- [ ] Docker image
- [ ] CI/CD pipeline for automated releases

### Type System & Inference
- [ ] HOF return type inference (a function returning a function — needed for transducers)
- [ ] Type annotations (optional signatures)
- [ ] Traits / type classes
- [ ] Cross-module generics

## Completed
- Lexer, Parser, AST (newline-aware, juxtaposition, string interpolation)
- LLVM codegen with TypedValue (type-directed, CType tags, monomorphization)
- Lambda lifting, higher-order functions, partial application
- Pipe operators (`|>`, `<|`)
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
- Multi-arg async support (thunk-based thread pool calls)
- Optimization passes (TCE, constant folding, GVN, DCE)
- `extern` / `extern async` for C FFI
- Compiler error messages with source locations and type validation
- REPL (compile-and-run mode)
- Stdlib: Option (7), Result (8), List (17), Tuple (4), Range (6), Test (3)
