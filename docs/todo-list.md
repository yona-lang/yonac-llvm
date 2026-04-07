# Yona-LLVM — Status & Roadmap

## Summary

- **Compiler**: Yona → LLVM IR → native executable via `yonac`
- **REPL**: `yona` — compile-and-run interactive mode
- **Tests**: 763 assertions across 75 test cases (all passing)
- **Stdlib**: 27 modules, ~290 exported functions (12 pure Yona + 15 C runtime)
- **Packaging**: Docker, Homebrew, RPM, DEB, GitHub Releases
- **Benchmarks**: 11/11 passing, 2 faster than C, 6 within 2x C

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
- [x] **Offset-based flat seq tail** — flat seq tail now bumps an offset
  instead of O(n) memmove. Queens: 10.3x→~5x C. Cons into offset space
  is also O(1). Offset encoded in upper 32 bits of flags word.
- [ ] **Profile-guided optimization** — runtime profiling for LLVM.
  Low priority: static branch hints already capture most benefit.

### Compiler Bugs
- [x] **Recursive seq-parameter type inference** — fixed by adding inttoptr
  coercion in case seq patterns when scrutinee is i64-typed (boxed pointer).
- [x] **Head-tail pattern mutation** — `case s of [h|t] -> x :: s` corrupted
  `s` because seq_tail modified it in place. Fixed by targeted rc_inc when
  the scrutinee variable appears in the case body.
- [x] **File I/O crash when io_uring unavailable** — `io_uring_setup` returns
  ENOMEM in constrained environments (containers, low-memory). Fixed with
  blocking fallback: direct result registration via `IO_OP_DIRECT_RESULT`
  sentinel in io_ctx table, transparent to `io_await` callers.

### Language — Type System & Effects
- [ ] **Algebraic Effect System** — first-class effects without monads.
  `effect State s` with `get`, `put`; `handle action with ...` handlers
  that compose. Replaces try/catch, enables typed side effects, mockable
  testing. Positions Yona alongside Koka, Eff, OCaml 5 but compiled to
  native via LLVM. Would naturally extend error handling, state, logging.
- [ ] **Refinement Types** — compile-time invariant verification.
  `type NonEmpty a = { seq : [a] | length seq > 0 }`, `type Port = { n : Int | 0 < n && n < 65536 }`.
  Total functions like `head : NonEmpty a -> a` with no runtime check.
- [ ] **Row Polymorphism for Records** — polymorphic record access.
  `greet : { name : String | r } -> String` works on any record with
  a `name` field. Like Elm/PureScript/OCaml row types.
- [ ] **Linear/Affine Types for Resources** — `type Linear File`,
  compiler enforces exactly-once use (must close). Composable with
  effects for safe resource management.
- [ ] **Gradual Typing with Contracts** — optional `@contract` annotations
  that generate runtime checks in debug, erased in release.

### Language — Concurrency
- [ ] **Structural Concurrency** — scoped concurrency with cancellation.
  `with scope` blocks ensure children can't outlive parent. Extends
  existing transparent async (io_uring, thread pool, auto-await).
- [ ] **Channels (CSP-style)** — typed channels for goroutine-style
  communication: `let ch = channel 10 in send ch msg; recv ch`.
- [ ] **STM** (Software Transactional Memory) — shared mutable state

### Language — Metaprogramming
- [ ] **Multi-Stage Programming** — compile-time computation.
  `static regex_compile pattern = ...` compiles regex at build time.
  Hygienic macros via staging.

### Codegen Refactoring (prerequisite for effect system)
- [x] **Phase 1: Break codegen_apply** (761 lines → 48-line dispatcher + 11 helpers)
  resolve_function, prepare_arguments, handle_partial_application,
  handle_higher_order_call, emit_direct_call
- [x] **Phase 1: Break codegen_let** (252 lines → 25-line dispatcher + 4 helpers)
- [x] **Phase 1: Break codegen_case** (461 lines → 5 pattern helpers)
- [ ] **Phase 2: Extract RuntimeDecls** — move 76 rt_* pointers into a struct
- [ ] **Phase 2: Deduplicate generators** — seq/set/dict share ~60% code
- [ ] **Phase 2: Define constants** — replace 787 magic numbers
- [ ] **Phase 3: Split Codegen class** — CodegenContext + RuntimeManager +
  TypeCache + Codegen (<50 members each)
- [ ] **Phase 3: Split compiled_runtime.c** — rc.c, adt.c, exceptions.c,
  async.c, closures.c, printing.c (seq.c, hamt.c, regex.c already split)

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
- Optimization levels O0-O3 via LLVM new PassManager
- fastcc calling convention, tail call marking, function inlining hints

### Performance Optimizations
- Stream fusion (chained comprehensions → single loop)
- LTO (cross-module inlining via clang bitcode + llvm::Linker)
- Closure devirtualization (direct calls for known lambdas)
- Inline seq checks (is_empty as count==0 GEP+load)
- Branch prediction hints (LIKELY/UNLIKELY on hot paths)
- HAMT transient inserts (unique-owner in-place mutation)

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
- DWARF debug info, rich error messages, DiagnosticEngine
- Documentation system (`##` doc comments → `scripts/gendocs.py`)
- Benchmark suite (11 benchmarks with C references)
- CI/CD: GitHub Actions (Linux, macOS, Windows)
- Docker image (multi-stage Fedora build)
- Homebrew formula, RPM spec, DEB packaging
- GitHub Releases workflow (Linux + macOS binaries on tag push)
- INSTALL.md, CONTRIBUTING.md, CHANGELOG.md
