# TODO List - Yona-LLVM

## Summary
- **Compiler**: Yona → LLVM IR → native executable via `yonac`
- **REPL**: `yona` — compile-and-run interactive mode
- **Tests**: 644 assertions across 73 test cases
- **Stdlib**: 20 modules, ~200 exported functions (11 pure Yona + 9 C runtime)

## Roadmap

### Standard Library (see docs/stdlib-plan.md for full design)

All I/O is non-blocking via `extern async`. Pure functions are sync.
15 modules, ~200 functions across 6 implementation phases.

Phase 1: Core types (Result, Option, List enhancements)
- [x] Enhanced Result: flatMap, flatten, toOption, andThen, orElse, fold ✅
- [x] Enhanced Option: flatMap, filter, orElse, zip, toResult, fold ✅
- [x] Enhanced List: sortBy, groupBy, partition, intersperse, scanl, zip, zipWith, enumerate, flatMap, find, nth, sum, product ✅
- [x] New modules: Math (11 fns), Pair (10 fns), Bool (7 fns) ✅

Phase 2: String and Encoding
- [x] Std\String — 27 functions (split, join, trim, replace, pad, repeat, take, drop, count, lines, chars, etc.) ✅
- [x] Std\Encoding — base64, hex, URL encode/decode, HTML escape ✅

Phase 3: I/O Foundation
- [x] Std\IO — print, println, eprint, eprintln, readLine, printInt, printFloat ✅
- [x] Std\File — readFile, writeFile, appendFile, exists, remove, size, listDir ✅
- [x] Std\Process — getenv, getcwd, exit ✅
- Platform layer: src/runtime/platform/linux.c (POSIX), prepared for macOS/Windows

Phase 4: Networking (all async)
- [ ] Std\Net — TCP/UDP sockets, listeners, connections
- [ ] Std\Http — full HTTP client (Request/Response ADTs, headers, streaming, timeouts) + server

Phase 5: Data Formats
- [x] Std\Json — stringify (int, string, bool, float, null), parseInt, parseFloat ✅
- [x] Std\Crypto — sha256, randomBytes, randomHex, uuid4 ✅
- [x] Std\Random — int, float, choice, shuffle ✅

Phase 6: Utilities
- [x] Std\Log — debug, info, warn, error, setLevel, getLevel (timestamped stderr) ✅
- [x] Std\Collection — iterate, unfold, replicate, tabulate, window, chunks, pairwise, dedup, frequencies ✅
- [x] Std\Function — identity, const, compose, flip, on, apply, pipe, fix ✅

Future:
- [ ] STM (Software Transactional Memory) — when shared mutable state is needed
- [ ] Std\Net — TCP/UDP sockets via io_uring (Linux), kqueue (macOS)
- [ ] Std\Http — HTTP client/server built on Std\Net

### Language Features
- [x] Generators/comprehensions — loop-based codegen for seq/set/dict generators with guard support ✅
- [x] Case guard expressions (`if` keyword) ✅
- [ ] `with` expression (resource management)
- [x] FQN calls without import (`Std\List::map fn seq`) ✅
- [x] Wildcard import (`import Std\List in expr`) ✅
- [x] ADT field names in .yonai (cross-module named field access) ✅
- [x] Re-exports (`exports add, mul from Std\Arith, localFunc as`) ✅
- [x] Proper closures ({fn_ptr, env_ptr} pairs — needed for functions in data structures) ✅
- [x] Lazy sequences / iterators (thunk-based lazy cons via closures in ADTs) ✅
- [x] Closures in HOF parameters (uniform closure calling convention) ✅

### Codegen Optimizations
- [ ] Inlining pass (AlwaysInline + function inlining)
- [ ] SROA (decompose ADT structs to registers)
- [ ] Loop optimizations (LICM, unrolling, vectorization)
- [ ] Dead argument elimination
- [ ] LTO (link-time optimization)
- [ ] Mutual tail call optimization

### Memory Management
- [x] Reference counting infrastructure (RC header, rc_inc/rc_dec, type-tagged allocation) ✅
- [x] Automatic RC at let-binding scope boundaries (inc result, dec bindings) ✅
- [x] Function parameter ownership: callee-borrows convention (no RC on params, documented) ✅
- [x] Escape analysis: AST-level per-let-scope analysis of non-escaping bindings ✅
- [x] Arena allocator: bump-allocated arenas for non-escaping values, bulk deallocation ✅

### Tooling
- [x] LLVM debug info (DWARF) — `-g` flag, source lines, variable inspection, type info ✅
- [x] Rich error messages (source line display, caret, color, "did you mean?" suggestions) ✅
- [x] Warning system (`--Wall`, `--Wextra`, `--Werror`, `-w`) ✅
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
- [x] Function type signatures in ADT declarations (`(Int -> Int)`, `(() -> Stream a)`) ✅
- [x] HOF return type inference / currying (over-application, functions returning functions) ✅
- [x] Type annotations (`add : Int -> Int -> Int` before definition) ✅
- [x] Traits Phase 1 — declarations, concrete instances, static monomorphized dispatch ✅
- [x] Traits Phase 2 — constrained instances (`Show a => Show (Option a)`) ✅
- [x] Traits Phase 3 — multi-method traits, default methods, superclass constraints ✅
- [x] Cross-module generics (GENFN source in .yonai, on-demand monomorphization, trait method extern dispatch) ✅

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
