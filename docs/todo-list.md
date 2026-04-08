# Yona-LLVM — Status & Roadmap

## Summary

- **Compiler**: Yona → LLVM IR → native executable via `yonac`
- **REPL**: `yona` — compile-and-run interactive mode
- **Tests**: 1070 assertions across 197 test cases (all passing)
- **Stdlib**: 27 modules, ~290 exported functions (12 pure Yona + 15 C runtime)
- **Features**: Algebraic effects, transparent async, persistent data structures, traits
- **Packaging**: Docker, Homebrew, RPM, DEB, GitHub Releases
- **Benchmarks**: 18/18 passing, 3 faster than C, 8 within 2x C

## Benchmark Results

| Benchmark | Yona | C | Time | Yona MB | C MB | Mem |
|-----------|------|---|------|---------|------|-----|
| list_map_filter | 0.85ms | 0.81ms | **1.0x** | 2.6 | 2.9 | 0.9x |
| tak | 66ms | 77ms | **0.8x** | 2.1 | 2.0 | 1.1x |
| sum_squares | 0.60ms | 0.52ms | 1.2x | 2.2 | 2.0 | 1.1x |
| sieve | 0.76ms | 0.56ms | 1.4x | 2.9 | 2.0 | 1.5x |
| list_reverse | 0.96ms | 0.68ms | 1.4x | 2.9 | 2.4 | 1.2x |
| list_sum | 1.0ms | 0.66ms | 1.6x | 2.7 | 2.4 | 1.1x |
| dict_build | 1.4ms | 0.70ms | 2.0x | 3.1 | 2.2 | 1.4x |
| set_build | 1.5ms | 0.72ms | 2.1x | 3.2 | 2.2 | 1.5x |
| fibonacci | 16ms | 8.0ms | 2.0x | 2.1 | 2.0 | 1.1x |
| ackermann | 166ms | 65ms | 2.6x | 2.2 | 2.3 | 1.0x |
| queens | 14ms | 1.3ms | 10.8x | 42.7 | 2.0 | 21x |

## Remaining Work

### Performance
- [ ] **Profile-guided optimization** — runtime profiling for LLVM.
  Low priority: static branch hints already capture most benefit.

### Language — Type System & Effects
- [x] **Algebraic Effect System** — `perform Effect.op args` + `handle...with...end`.
  Static handler dispatch via CPS, handlers compiled as direct LLVM functions.
  Resume via raw function pointer indirect call. Zero overhead when not used.
  See [docs/effects.md](effects.md).
- [x] **Anonymous/Inline Sum Types** — `Int | String` as type annotations,
  function return types, and ADT field types. Parser handles `T1 | T2` via
  `parse_sum_type()` → `SumType`. Codegen: tagged 2-tuple `(type_tag, value)`
  via `box_as_sum()`. Type-based pattern matching: `case x of (n : Int) -> ...`.
  `TypedPattern` AST node, `CType::SUM`, auto-boxing on typed pattern match.
- [x] **Refinement Types** — compile-time invariant verification.
  `type NonEmpty a = { xs : [a] | length xs > 0 }`, `type Port = { n : Int | n > 0 && n < 65536 }`.
  RefinementChecker with FactEnv (integer intervals, non-emptiness, excluded values).
  Built-in checks: head/tail require non-empty, division requires non-zero.
  Pattern-match narrowing: `[h|t]` proves non-empty, integer literals set exact bounds,
  wildcard `_` excludes all prior matched values. Guard narrowing: `n | n > 0`.
  If-condition narrowing for all 6 comparisons (`>`, `<`, `>=`, `<=`, `==`, `!=`)
  with `&&` compound support. Arithmetic interval propagation through `+` and `-`.
  Variable aliasing. Parsed in type position: `{ var : Type | pred }`.
  Refinements erased at codegen (zero runtime cost). See `docs/refinement-types.md`.
- [x] **Row Polymorphism for Records** — anonymous record literals `{ name = "Alice", age = 30 }`,
  compiled as tuples with compile-time field maps. Field access `r.name` is direct indexed
  GEP (zero overhead). Row types in type checker: `MonoType::MRecord` with field list + optional
  row variable. Row unification matches common fields, propagates extras via row vars.
  Field access `r.name` constrains `r` to `{ name : t | r' }`. See `docs/row-polymorphism.md`.
- [x] **Linear/Affine Types for Resources** — `type Linear a = Linear a` ADT,
  compiler enforces single-consume via LinearityChecker. Pattern match
  `case x of Linear fd -> ...` is the consumption point. Use-after-consume
  errors [E0600], branch inconsistency [E0601], resource leak warnings [E0602].
  Stdlib producers registered: tcpConnect, tcpListen, tcpAccept, udpBind, spawn.
  See `docs/linear-types.md`.
- [ ] **Gradual Typing with Contracts** — optional `@contract` annotations
  that generate runtime checks in debug, erased in release.

### Language — Concurrency
- [x] **Structured Concurrency** — automatic scoped concurrency with cancellation.
  Let blocks auto-group async bindings. If one fails, siblings are cancelled and
  error is propagated. Task groups in runtime with IORING_OP_ASYNC_CANCEL for
  io_uring ops. Cancel effect (`perform Cancel.check ()`) for cooperative
  cancellation. Parallel comprehensions `[| expr for x = source ]`.
  Std\Parallel module with pmap, pfor. See `docs/structured-concurrency.md`.
- [ ] **Channels (CSP-style)** — typed channels for goroutine-style
  communication: `let ch = channel 10 in send ch msg; recv ch`.
- [ ] **STM** (Software Transactional Memory) — shared mutable state

### Language — Metaprogramming & Introspection
- [ ] **Multi-Stage Programming** — compile-time computation.
  `static regex_compile pattern = ...` compiles regex at build time.
  Hygienic macros via staging.
- [ ] **Runtime Type Introspection** — `typeOf x` returns type tag at
  runtime. Pattern matching on types: `case typeOf x of :int -> ...; :string -> ... end`.
  Foundation for generic serialization, debugging, dynamic dispatch.
- [ ] **Compile-Time Reflection** — access ADT constructor names, field
  names/types, trait instances at compile time. Enables: auto-derive
  (derive Show, Eq), schema generation, serialization codegen.
- [ ] **Quasiquotes / Template Expressions** — `quote { expr }` captures
  AST for manipulation. `splice expr` inserts computed AST into code.
  Enables: DSLs, custom syntax extensions, code generation.

### Diagnostics
- [x] **Rich error explanations** — `yonac --explain E0100` shows detailed
  explanation with examples. Error codes (E01xx type, E02xx effect, E03xx parse,
  E04xx codegen) on all errors. Unified DiagnosticEngine with colored output,
  source context display, caret underlines, and "did you mean?" suggestions.
  Parser errors routed through DiagnosticEngine in CLI.

### Tooling
- [ ] Package manager / build system
- [ ] LSP server

### Distribution
- [ ] Windows installer (NSIS/WiX)

### Platform Support
- [ ] macOS platform layer (kqueue-based async I/O)
- [ ] Windows platform layer (IOCP-based async I/O)
- [ ] Multi-arch (aarch64/arm64) build presets

---

## What's Done

### Compiler
- Lexer, parser, AST (newline-aware, juxtaposition, string interpolation)
- LLVM codegen with TypedValue (type-directed, CType tags, monomorphization)
- Lambda lifting, HOFs, partial application, currying, pipe operators
- Case expressions (integer, symbol, wildcard, head-tail, tuple, constructor, or-pattern, guards)
- Symbol interning (i64 IDs, icmp eq comparison)
- Generators/comprehensions (seq/set/dict) with stream fusion
- ADTs: non-recursive (flat struct), recursive (heap-allocated)
- Closures: env-passing convention, closure devirtualization
- Interface files (.yonai), modules, re-exports, extern declarations
- Cross-module generics (GENFN monomorphization)
- Cross-module return type propagation (boxed wrapper detection)
- Exception handling (raise/try/catch via setjmp/longjmp)
- `with` expression, `do` blocks, async codegen (PROMISE, thread pool, io_uring)
- Algebraic effects (perform/handle with CPS, static handler dispatch)
- Optimization levels O0-O3 via LLVM new PassManager
- fastcc calling convention, tail call marking, function inlining hints
- Codegen refactored: codegen_apply (11 helpers), codegen_let (4 helpers),
  codegen_case (5 pattern helpers), RuntimeDecls struct, TypeRegistry/
  ImportState/SymbolTable/DebugState grouping

### Performance Optimizations
- Stream fusion (chained comprehensions → single loop)
- LTO (cross-module inlining via clang bitcode + llvm::Linker)
- Closure devirtualization (direct calls for known lambdas)
- Inline seq checks (is_empty as count==0 GEP+load)
- Branch prediction hints (LIKELY/UNLIKELY on hot paths)
- HAMT transient inserts (unique-owner in-place mutation)
- Offset-based flat seq tail (O(1) instead of O(n) memmove)

### Data Structures
- Persistent Seq RBT (flat <=32, head chain + back trie + tail buffer >32)
- Persistent Dict (HAMT, splitmix64, O(1) amortized, transient inserts)
- Persistent Set (HAMT-backed, union/intersection/difference)

### Memory Management
- Atomic RC (`RELAXED` inc, `ACQ_REL` dec)
- Recursive destructors for all container types (heap_mask bitmasks)
- Hybrid Perceus DUP/DROP (callee-owns for non-seq, callee-borrows for seq)
- Let-bound seq protection (rc_inc prevents in-place tail mutation)
- Slab-based pool allocator (5 size classes)
- Arena allocation for non-escaping let-bound values
- Unique-owner optimization (in-place cons/tail/insert when rc==1)
- io_uring buffer pinning (copy-on-submit for async write/send)
- Weak closure self-references (break RC cycles)
- See `docs/memory-management.md` for full details.

### Type System
- Hindley-Milner type inference with let-polymorphism (TypeChecker)
- Union-find with path compression, occurs check, level-based generalization
- ADT constructor type inference (polymorphic schemes)
- Trait constraints with deferred solving and instance resolution
- Effect type tracking: perform/handle inference, handler scope, unhandled warnings
- Anonymous sum types: `Int | String`, typed patterns `(n : Int)`, auto-boxing
- Refinement types: `{ n : Int | n > 0 }`, flow-sensitive fact tracking, zero-cost erasure,
  built-in checks (head/tail non-empty, division non-zero), wildcard complement narrowing,
  guard narrowing, arithmetic interval propagation, all 6 comparison operators in if-narrowing
- Linear types: `type Linear a = Linear a` ADT, LinearityChecker with use-after-consume
  detection, branch consistency, resource leak warnings, stdlib producer registry
- Prelude module: `lib/Prelude.yona` with Linear, Option, Result types + utility
  functions (identity, const, flip, compose). Auto-loaded without imports.
- Row polymorphism: anonymous records `{ field = val }` as tuples, `MonoType::MRecord`
  with row variables, row unification, field access type inference
- Structured concurrency: auto-grouping let blocks, task group runtime, io_uring
  cancellation, Cancel effect, parallel comprehensions, Std\Parallel module
- "Did you mean?" suggestions via edit distance on undefined variables
- Function type signatures, HOF return type inference, type annotations
- Traits: concrete/constrained instances, multi-method, default methods, superclass
- Num trait, cross-module trait dispatch, Bytes type

### I/O & Concurrency
- io_uring backend (Linux): raw syscalls, submit-and-return
- Thread pool with work-stealing for extern async functions
- Non-blocking Process: spawn, readLine, readAll, wait, kill, writeStdin, pid
- Async exec/execStatus via thread pool (AFN)

### Standard Library (27 modules)

**Pure Yona (12):** Option, Result, List, Tuple, Range, Math, Pair, Bool,
Test, Collection, Function, Http

**C runtime (15):** String, Encoding, Types, IO, File, Process, Random,
Json, Crypto, Log, Net, Bytes, Time, Path, Format, Dict, Set, Regex

### Tooling & Distribution
- DWARF debug info, DiagnosticEngine with error codes and `--explain`
- Documentation system (`##` doc comments → `scripts/gendocs.py`)
- Benchmark suite (11 benchmarks with C references)
- CI/CD: GitHub Actions (Linux, macOS, Windows)
- Docker image (multi-stage Fedora build)
- Homebrew formula, RPM spec, DEB packaging
- GitHub Releases workflow (Linux + macOS binaries on tag push)
- INSTALL.md, CONTRIBUTING.md, CHANGELOG.md
