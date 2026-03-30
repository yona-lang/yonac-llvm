# TODO List - Yona-LLVM

## Summary
- **Compiler**: Yona → LLVM IR → native executable via `yonac`
- **REPL**: `yona` — compile-and-run interactive mode
- **Tests**: 650 assertions across 73 test cases
- **Stdlib**: 22 modules, ~220 exported functions (11 pure Yona + 11 C runtime)

## Remaining Work

### Language Features
- [ ] `with` expression (resource management / RAII-style cleanup)
- [ ] STM (Software Transactional Memory) — for shared mutable state

### Codegen Optimizations
- [x] Optimization levels (-O0 to -O3) via LLVM new PassManager ✅
- [x] Inlining (cost-based at O2+, AlwaysInliner at O0) ✅
- [x] SROA (scalar replacement of aggregates — decomposes structs) ✅
- [x] Loop optimizations (LICM, unrolling at O2+, vectorization at O3) ✅
- [x] Dead argument elimination ✅
- [x] Tail call marking (self-recursive calls marked `tail`) ✅
- [ ] LTO (link-time optimization across translation units)
- [ ] Mutual tail call optimization (between different functions)

### Tooling
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

### Platform Support
- [ ] macOS platform layer (kqueue-based I/O)
- [ ] Windows platform layer (IOCP-based I/O)

## Completed

### Compiler
- Lexer, Parser, AST (newline-aware, juxtaposition, string interpolation)
- LLVM codegen with TypedValue (type-directed, CType tags, monomorphization)
- Lambda lifting, higher-order functions, partial application, currying
- Pipe operators (`|>`, `<|`)
- Case expressions (integer, symbol, wildcard, head-tail, tuple, constructor, or-pattern, guards)
- Symbol interning (i64 IDs, icmp eq comparison)
- Dict/Set construction with typed elements
- Generators/comprehensions (loop-based codegen for seq/set/dict with guard support)
- ADTs: non-recursive (flat struct) and recursive (heap nodes), named fields, constructors as first-class functions, exhaustiveness checking
- Proper closures ({fn_ptr, env_ptr} pairs, uniform HOF calling convention)
- Lazy sequences / iterators (thunk-based lazy cons via closures in ADTs)
- Interface files (.yonai) for cross-module type-safe linking (FN, AFN, IO keywords)
- Module compilation with C-ABI exports, re-exports
- Cross-module generics (GENFN source in .yonai, on-demand monomorphization)
- Exception handling (raise/try/catch via setjmp/longjmp, ADT exceptions, stack traces)
- Async codegen (CType::PROMISE, thread pool for CPU, io_uring for I/O, auto-await, parallel let)

### Type System
- Function type signatures in ADT declarations
- HOF return type inference / currying (over-application)
- Type annotations (`add : Int -> Int -> Int`)
- Traits: declarations, concrete instances, constrained instances, multi-method, default methods, superclass constraints
- Cross-module trait dispatch (ExternalLinkage trait methods, ADT struct types in externs)

### Memory Management
- Reference counting infrastructure (RC header, rc_inc/rc_dec, type-tagged allocation)
- Automatic RC at let-binding scope boundaries
- Function parameter ownership (callee-borrows convention)
- Escape analysis (AST-level per-let-scope)
- Arena allocator (bump-allocated arenas for non-escaping values)

### I/O Architecture
- io_uring backend (Linux): raw syscalls, no liburing dependency
- Submit-and-return pattern: I/O functions submit to ring, return uring ID as promise
- Generic yona_rt_io_await() completer with io_context registry
- Platform abstraction: uring.h, file_linux.c, net_linux.c, os_linux.c

### Tooling
- LLVM debug info (DWARF) — `-g` flag, source lines, variable inspection
- Rich error messages (source line display, caret, color, "did you mean?" suggestions)
- Warning system (`--Wall`, `--Wextra`, `--Werror`, `-w`)
- DiagnosticEngine with GCC-compatible flags
- Documentation system: `##` doc comments, scripts/gendocs.py extractor

### Standard Library (22 modules, ~220 functions)

Pure Yona modules:
- Std\Option (10) — Some/None ADT, flatMap, filter, orElse, fold, zip, toResult
- Std\Result (11) — Ok/Err ADT, flatMap, flatten, toOption, andThen, orElse, fold
- Std\List (28) — map, filter, fold, zip, sortBy, groupBy, partition, intersperse, scanl, flatMap, find
- Std\Tuple (9) — fst, snd, swap, mapBoth, mapFst, mapSnd, curry, uncurry
- Std\Range (11) — lazy ranges with step, map, filter, fold, forEach
- Std\Math (11) — abs, max, min, clamp, gcd, pow, factorial
- Std\Pair (10) — ADT pair with named fields
- Std\Bool (7) — not, xor, implies, when, unless
- Std\Test (6) — assertEqual, assertNotEqual, assertTrue, assertGreater
- Std\Collection (9) — iterate, unfold, replicate, tabulate, window, chunks, dedup, frequencies
- Std\Function (8) — identity, const, compose, flip, on, apply, pipe, fix

C runtime modules:
- Std\String (27) — split, join, trim, replace, repeat, take, drop, count, lines, chars
- Std\Encoding (7) — base64, hex, URL encode/decode, HTML escape
- Std\Types (5) — intToString, floatToString, toInt, toFloat, boolToString
- Std\IO (7) — print, println, eprint, eprintln, readLine (IO), printInt, printFloat
- Std\File (7) — readFile (IO), writeFile (IO), appendFile, exists, remove, size, listDir
- Std\Process (3) — getenv, getcwd, exit
- Std\Random (4) — int, float, choice, shuffle
- Std\Json (7) — stringify (int/string/bool/float/null), parseInt, parseFloat
- Std\Crypto (4) — sha256, randomBytes, randomHex, uuid4
- Std\Log (6) — debug/info/warn/error with timestamps, setLevel/getLevel
- Std\Net (10) — TCP/UDP via io_uring: tcpConnect, tcpListen, tcpAccept, send, recv, close
- Std\Http (6) — buildRequest, parseStatus, parseBody, getHeader, parseUrl, httpGet (io_uring)
