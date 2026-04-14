# Yona-LLVM — Status & Roadmap

## Summary

- **Compiler**: Yona → LLVM IR → native executable via `yonac`
- **REPL**: `yona` — compile-and-run interactive mode
- **Tests**: 1207 assertions across 204 test cases (all passing)
- **Benchmarks**: 34/34 passing (rerun 2026-04-14, LLVM 22, 10 iters at -O2)
- **Stdlib**: 33 modules (Std\IO rewritten non-blocking, Std\Stream v1, Std\Constants\{Num,Math,Platform})
- **Features**: Algebraic effects, transparent async, persistent data structures, traits
- **Packaging**: Docker, Homebrew, RPM, DEB, GitHub Releases

## Benchmark Results

Rerun on LLVM 22, 2026-04-14, 10 iterations, `b51e035`. Sorted by Yona/C ratio.

| Benchmark | Yona | C | Ratio | Yona MB | C MB |
|-----------|------|---|-------|---------|------|
| par_map | 0.60ms | 0.63ms | **1.0x** | 2.4 | 2.4 |
| list_map_filter | 0.73ms | 0.70ms | **1.0x** | 2.9 | 3.0 |
| parallel_async | 101ms | 101ms | **1.0x** | 2.7 | 2.5 |
| sequential_async | 402ms | 401ms | **1.0x** | 2.8 | 2.2 |
| process_spawn | 1.2ms | 1.1ms | **1.1x** | 3.8 | 3.8 |
| tak | 66ms | 62ms | **1.1x** | 2.4 | 2.2 |
| file_read | 0.75ms | 0.69ms | **1.1x** | 3.5 | 3.3 |
| binary_read_chunks | 0.80ms | 0.70ms | **1.1x** | 2.5 | 2.3 |
| process_exec | 1.2ms | 1.0ms | 1.2x | 3.9 | 3.9 |
| int_array_fill_sum | 0.59ms | 0.49ms | 1.2x | 2.6 | 2.3 |
| binary_write_read | 3.6ms | 2.9ms | 1.2x | 12.5 | 7.2 |
| channel_pipeline | 1.2ms | 0.94ms | 1.3x | 3.2 | 2.3 |
| channel_fanin | 1.5ms | 1.1ms | 1.3x | 3.3 | 2.4 |
| seq_map | 0.61ms | 0.49ms | 1.3x | 2.4 | 2.2 |
| sum_squares | 0.59ms | 0.46ms | 1.3x | 2.4 | 2.2 |
| channel_throughput | 1.7ms | 1.2ms | 1.4x | 3.5 | 2.3 |
| sieve | 0.77ms | 0.54ms | 1.4x | 3.1 | 2.2 |
| list_reverse | 0.89ms | 0.63ms | 1.4x | 3.2 | 2.6 |
| list_sum | 0.87ms | 0.59ms | 1.5x | 3.1 | 2.6 |
| file_parallel_read | 1.3ms | 0.81ms | 1.6x | 6.0 | 5.4 |
| file_write_read | 1.5ms | 0.86ms | 1.7x | 4.8 | 3.2 |
| set_build | 1.4ms | 0.67ms | 2.1x | 3.4 | 2.3 |
| dict_build | 1.4ms | 0.63ms | 2.2x | 3.4 | 2.5 |
| file_readlines_large | 35ms | 14ms | 2.4x | 2.5 | 2.1 |
| fibonacci | 15ms | 6.3ms | 2.4x | 2.3 | 2.2 |
| ackermann | 162ms | 63ms | 2.6x | 2.5 | 2.3 |
| sort | 1.5ms | 0.52ms | 2.8x | 8.1 | 2.2 |
| file_readlines | 2.2ms | 0.71ms | 3.1x | 7.3 | 2.2 |
| file_write_read_large | 44ms | 13ms | 3.3x | 107 | 2.3 |
| int_array_map | 1.9ms | 0.55ms | 3.5x | 2.8 | 2.4 |
| int_array_sum | 1.8ms | 0.47ms | 3.9x | 2.7 | 2.3 |
| file_read_large | 12ms | 2.8ms | 4.4x | 55 | 2.3 |
| file_parallel_read_large | 8.2ms | 1.3ms | 6.4x | 37 | 2.4 |
| queens | 13ms | 1.2ms | 10.6x | 43 | 2.2 |

Erlang reference impls (bench/reference/*.erl) exist for all 17 Yona benchmarks
but are not yet wired into the runner's comparison output — that's a runner
enhancement, not a benchmark change.

## Remaining Work

### Performance
- [ ] **Profile-guided optimization** — runtime profiling for LLVM.
  Low priority: static branch hints already capture most benefit.
- [ ] **Explore JIT compilation potential** — research task. Investigate
  whether Yona could benefit from a JIT (LLVM ORC, Cranelift, or custom)
  for: (1) REPL/interactive use (compile expressions on the fly without
  full AOT pipeline), (2) hot-path specialization (detect monomorphic call
  sites at runtime, generate specialized code), (3) inlining across module
  boundaries beyond LTO, (4) profile-guided optimization at runtime.
  Trade-offs: JIT adds startup latency, code cache memory, complexity.
  Yona's AOT pipeline already produces fast code; JIT only wins if there's
  a specific runtime adaptation benefit. Output: design doc with concrete
  proposal or recommendation to defer.

### Language — Type System & Effects
- [ ] **Gradual Typing with Contracts** — optional `@contract` annotations
  that generate runtime checks in debug, erased in release.

### Language — Concurrency
- [x] **Stream (lazy pull-based sequence)** — shipped as `lib/Std/Stream.yona`.
  Type: `Stream a = Yield a (() -> Stream a) | Nil`. Surface: producers
  (`empty`, `singleton`, `fromSeq`, `range`, `naturals`, `repeat`,
  `iterate`, `unfold`, `fromIterator`), lazy operators (`map`, `filter`,
  `take`, `drop`, `takeWhile`, `dropWhile`, `zip`, `zipWith`, `concat`,
  `flatMap`, `scan`), pipeline parallelism (`async`, `buffered`), and
  terminators (`toSeq`, `foldl`, `forEach`, `count`, `sum`, `anyMatch`,
  `allMatch`, `find`, `head`, `isEmpty`). Fixture tests: `stream_basic`,
  `stream_filter_take`, `stream_iterate`, `stream_pipeline`, `stream_zip`,
  `stream_async`. Deferred: `chunksOf`, resource cleanup (`Stream.bracket`),
  error forwarding across `async`, cross-operator fusion, multi-consumer
  broadcast.
- [ ] **Multi-Shot Effects with Stackful Coroutines** (research, deferred) —
  extend the effect system to support multi-shot resume and true suspension
  via heap-allocated continuations or per-effect stacks. Enables: backtracking
  effects, generators with replay, async/await desugaring, full algebraic
  effects à la Koka/Effekt. Requires: function coloring or stackful coroutines,
  heap continuation frames, GC interaction. Scope: 2000+ lines. Not needed
  for streaming use cases (Stream Sugar + Channels solve those). Defer until
  there's a concrete use case beyond streaming.
- [ ] **Distributed Yona** — network/interprocess communication between Yona
  systems. Actor model, message passing, distributed effects, serialization.
  Erlang-style nodes, effect-based RPC, distributed task groups.
- [ ] **Channel deadlock detection** — runtime deadlock detection in
  `recv`: before `cond_wait`, check if all tasks in the group are blocked
  and no I/O is in flight; if so, raise `:Deadlock`. Catches transitive
  deadlocks (cycles, crashed producers) that linear types can't see.
- [x] **Std\IO module** — non-blocking console and handle-based byte I/O.
  Shipped as pure-Yona module calling `yona_Std_IO__*` externs that
  submit via io_uring (writes) or thread pool (readLine). All write
  operations return Promises that auto-await at the call site.
- [x] **Std\Constants module** — split into `Std\Constants\Num`,
  `Std\Constants\Math`, `Std\Constants\Platform`. Platform queries use
  externs in `os_linux.c`.
- [ ] **LLVM coroutine lowering for async** — replace the thread-pool +
  promise machinery (`yona_pool_worker`, `yona_rt_async_call`, `async_await`)
  with LLVM coroutines as the suspend/resume substrate. io_uring stays as
  the kernel completion source: an await suspends the coroutine, submits
  an SQE with the coroutine handle in `user_data`, and a reactor thread
  reaps the CQ and calls `resume`. Wins: no parked worker per pending I/O,
  frame sized to actual capture set instead of full thread stack, suspend
  is a few loads + indirect jump instead of futex + scheduler.
  Catches: every async function gets coroutine-shaped (split into ramp +
  resume), so `extern io`/`extern async` lowering and the C ABI for
  promises both need rework. Frame allocation via `llvm.coro.alloc`
  should route through `rc_alloc` (or arena per task) so frames
  participate in the refcount system. Cancellation needs first-class
  plumbing — depends on the structured-concurrency work landing first
  so the cancellation model is settled before we bake it into coroutine
  shape. Scope: ~1500 lines codegen + runtime.
- [ ] **STM** (Software Transactional Memory) — shared mutable state
- [ ] **Serialization System** — structured binary/text serialization for Yona
  values. Encoders/decoders for ADTs, tuples, sequences, dicts, sets.
  Possible approaches: derive-based (requires compile-time reflection),
  trait-based (`trait Serialize a`, `trait Deserialize a`), or built-in
  codec for a wire format (MessagePack, CBOR, or custom). Foundation for
  distributed Yona (message passing), persistence, and interop.

### Language — Metaprogramming & Introspection
- [ ] **Multi-Stage Programming** — compile-time computation.
  `static regex_compile pattern = ...` compiles regex at build time.
  Hygienic macros via staging.
- [ ] **Compile-Time Evaluator** — evaluate pure functions at compile time.
  Enables user-defined derive strategies, constant folding, static assertions.
  Requires: subset interpreter for pure Yona expressions (no I/O, no effects).
- [ ] **User-Defined Derives** — traits declare themselves derivable via
  `derive` block that templates over ADT structure. Requires compile-time
  evaluator or external codegen tool reading enriched `.yonai` metadata.
- [ ] **Quasiquotes / Template Expressions** — `quote { expr }` captures
  AST for manipulation. `splice expr` inserts computed AST into code.
  Enables: DSLs, custom syntax extensions, code generation.

### Tooling
- [ ] Package manager / build system
- [ ] LSP server

### Benchmarks
- [ ] **Erlang/OTP reference implementations** — add Erlang reference impls
  for concurrency benchmarks (parallel_async, sequential_async, par_map,
  channel_throughput, channel_pingpong, channel_pipeline, channel_workers,
  channel_actor). Erlang is the gold standard for actor-model concurrency
  with cheap processes (~300B/process), built-in mailboxes, and preemptive
  scheduling — directly comparable to Yona's structured concurrency +
  channels. Should also add: fibonacci, ackermann, tak, sieve, sort,
  queens, list_sum (for non-concurrency comparison). Files: `bench/reference/*.erl`.

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
- Hindley-Milner type inference with let-polymorphism (TypeChecker), blocking mode enabled
- All AST node types handled: generators, imports, mutual recursion, effects, records
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
