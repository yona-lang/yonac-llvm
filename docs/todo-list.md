# Yona-LLVM — Status & Roadmap

## Summary

- **Compiler**: Yona → LLVM IR → native executable via `yonac`
- **REPL**: `yona` — compile-and-run interactive mode
- **Tests**: 1260 assertions across 209 test cases (all passing)
- **Benchmarks**: 34/34 passing (rerun 2026-04-16, LLVM 22, 10 iters at -O2)
- **Stdlib**: ~37 modules (non-blocking Std\IO, Std\Stream, Std\Constants\{Num,Math,Platform}, Std\Channel, Std\Task)
- **Features**: Algebraic effects, transparent async, persistent data structures, traits
- **Packaging**: Docker, Homebrew, RPM, DEB, GitHub Releases

## Benchmark Results

Rerun on LLVM 22, 2026-04-16, 10 iterations, post-borrow-inference.
Startup-adjusted times; sorted by Yona/C ratio.

| Benchmark | Yona | C | Ratio | Yona MB | C MB |
|-----------|------|---|-------|---------|------|
| int_array_fill_sum | 0.01ms | 0.05ms | **0.2x** | 2.6 | 2.3 |
| sum_squares | 0.01ms | 0.01ms | **0.8x** | 2.4 | 2.2 |
| file_read | 0.23ms | 0.27ms | **0.9x** | 3.5 | 3.3 |
| par_map | 0.01ms | 0.23ms | **~0x** | 2.4 | 2.4 |
| seq_map | 0.01ms | 0.01ms | **1.0x** | 2.4 | 2.2 |
| parallel_async | 101ms | 101ms | **1.0x** | 2.8 | 2.3 |
| sequential_async | 401ms | 401ms | **1.0x** | 2.8 | 2.2 |
| tak | 73ms | 66ms | 1.1x | 2.4 | 2.2 |
| process_exec | 0.69ms | 0.58ms | 1.2x | 3.9 | 4.0 |
| list_map_filter | 0.30ms | 0.25ms | 1.2x | 3.3 | 3.1 |
| binary_read_chunks | 0.28ms | 0.22ms | 1.3x | 2.5 | 2.3 |
| binary_write_read | 3.1ms | 2.5ms | 1.2x | 12.5 | 7.3 |
| channel_throughput | 1.2ms | 0.86ms | 1.4x | 3.5 | 2.2 |
| channel_pipeline | 0.76ms | 0.50ms | 1.5x | 3.2 | 2.3 |
| channel_fanin | 1.1ms | 0.67ms | 1.6x | 3.3 | 2.4 |
| file_parallel_read | 0.59ms | 0.34ms | 1.7x | 6.0 | 5.5 |
| file_write_read | 0.84ms | 0.39ms | 2.2x | 4.8 | 3.2 |
| list_sum | 0.34ms | 0.14ms | 2.4x | 3.1 | 2.6 |
| fibonacci | 16ms | 6.3ms | 2.5x | 2.4 | 2.2 |
| ackermann | 178ms | 66ms | 2.7x | 2.5 | 2.4 |
| list_reverse | 0.48ms | 0.17ms | 2.9x | 3.1 | 2.6 |
| file_write_read_large | 49ms | 15ms | 3.4x | 107 | 2.3 |
| file_readlines_large | 53ms | 15ms | 3.5x | 2.6 | 2.1 |
| file_read_large | 13ms | 2.4ms | 5.1x | 55 | 2.3 |
| sieve | 0.39ms | 0.07ms | 6.1x | 3.1 | 2.1 |
| dict_build | 0.98ms | 0.16ms | 6.3x | 3.6 | 2.4 |
| set_build | 1.1ms | 0.15ms | 7.2x | 3.6 | 2.3 |
| file_parallel_read_large | 8.0ms | 1.0ms | 7.8x | 37 | 2.4 |
| queens | 18ms | 2.1ms | 8.5x | 2.4 | 2.2 |
| sort | 1.0ms | 0.05ms | 18.8x | 7.9 | 2.2 |
| int_array_map | 1.5ms | 0.06ms | 27.6x | 3.3 | 2.4 |
| int_array_sum | 1.6ms | 0.04ms | 36.3x | 3.3 | 2.3 |

Full multi-language matrix in [benchmark-results.md](./benchmark-results.md).
Reference impls in C, Erlang, Haskell, Java, Node.js, Python under
[`bench/reference/`](../bench/reference/).

## Remaining Work

### Bugs
- [ ] **Sort benchmark RBT leak** (8669 RBT leaked). Root cause:
  closure calling convention erases all arg types to `i64`. When
  `insert` is called through `\acc x -> insert x acc`, the `sorted`
  param compiles as i64 instead of ptr/SEQ, bypassing ALL Perceus
  tracking (empty-arm dec, transfer_scope, function-exit dec, borrow
  inference). CType upgrade (INT→SEQ) was attempted twice — first
  at case codegen level, then at compile_function level — both
  caused heap corruption: the arm_drop dec + transfer_scope
  compensating dec freed sorted while foldl still held a ref (double
  free via RBT structural sharing cascading through shared subtrees).
  Correct fix: **preserve types through the closure convention**.
  When a closure calls a known function, compile that function with
  ptr params for heap-typed args (not i64). emit_direct_call already
  handles i64→ptr coercion. This requires plumbing actual CTypes
  from the enclosing scope (foldl's acc = SEQ) through the lambda's
  params into the deferred function's compile_function call.
  Scope: ~100 lines in closure codegen + emit_direct_call.
  Repro: `bench/core/sort.yona` with `YONA_ALLOC_STATS=1`.

### Code Quality — deferred from 2026-04-15 audit
- [ ] **O(1) transfer_scope BB detection** — a true O(1) replacement
  for the `pre_blocks` walk requires hooking every BB creation (LLVM
  doesn't expose BB ordinal positions); lazy compute at scope_exit
  breaks correctness for nested constructs. Current O(n) eager
  capture is correct and not a measured hotspot. Revisit only if
  compile time becomes an issue.
- [ ] **Unify transferred_seqs_ / transferred_maps_** — the two sets
  differ *behaviorally*: seqs participate in per-branch transfer-
  scope compensating drops, maps only suppress the function-exit
  drop. Unifying storage while preserving the split would require
  type-tagging every transfer-scope operation — more code than the
  duplication saves. Revisit if a third transfer class appears.
- [ ] **Relax stream fusion gate** — fusion requires
  `count_identifier_refs(let_body, name) == 1`. Relaxing to "single
  use on each path" needs AST duplication into each branch (no
  support today in `codegen_fused_seq_generator`). Revisit with a
  concrete benchmark showing measurable loss.


### Performance
- [ ] **LLVM EH migration** — replace setjmp/longjmp with
  invoke/landingpad. Each heap-owning function gets a cleanup
  landingpad that runs rc_dec and resumes unwinding. Would eliminate
  the phase-3 TLS depth check entirely (true zero-cost happy path).
  Bigger codegen change: every call that can raise becomes an
  invoke, and the landingpad must handle all owned drops. ~500 lines.
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

### Language — Safety & Ownership
- [ ] **Use-site exhaustiveness for Result/Option** (Gleam-inspired).
  If a function returns `Result a e` or `Option a`, the call site
  must either pattern-match both arms or explicitly propagate.
  Enforced at compile time by the refinement checker — no new syntax
  needed. The prelude already defines `Result` and `Option`; this
  adds a rule: "unmatched Result/Option at a call site is a compile
  error". Prevents silently ignoring errors. ~200 lines in
  `RefinementChecker`.
- [ ] **Borrow annotations for read-only parameters** (Rust-inspired).
  The biggest remaining perf gap vs C: every multi-use heap param
  pays `rc_inc` at the call site and `rc_dec` at function exit. If
  the type system could distinguish "reading, not storing" (a
  borrow), those inc/dec pairs vanish. Start as opt-in annotation
  (`@borrow` or `&`), enforced by the linearity checker. A borrowed
  param cannot be stored into a heap structure, returned, or
  captured — only read. This subsumes the "skip rc_inc for
  unused/always-transferred params" item from the code-quality
  audit. Scope: ~500 lines (type annotation + linearity check +
  codegen skip).

### Language — Architecture & Infrastructure
- [ ] **Per-task-group arenas** — each task group allocates from a
  bump arena that's freed wholesale on group completion. Leverages
  existing arena + escape analysis (`include/EscapeAnalysis.h`) +
  the structured concurrency plan. Kills the "raise leaks heap
  values" problem entirely for task groups (no per-object rc_dec
  needed on unwind — just free the arena). ~300 lines.
- [ ] **Supervisors as effect handlers** — model Erlang-style
  supervision trees via the existing algebraic effect system. A
  supervisor is a `handle ... with` that catches child-task failures
  and decides restart/escalate/ignore. No compiled functional
  language has this. Depends on structured concurrency landing first.
- [ ] **Content-addressed code** (Unison-inspired). Functions
  identified by hash of their AST, not by name. Enables: perfect
  caching (same function = same hash = skip recompile), zero-conflict
  merges, refactoring without breakage. Yona's `.yonai` interface
  files with GENFN source are already halfway there — they embed
  source for cross-module monomorphization. Content-addressing would
  make this principled. Research-phase; significant tooling impact
  (package manager, LSP, VCS integration).

### Language — GPU / Heterogeneous Compute
- [ ] **Std\GPU module** (pragmatic first step). Explicit GPU dispatch
  for FloatArray/IntArray operations:
  `reduceGPU (\a b -> a + b) 0.0 (mapGPU (\x -> x * x) arr)`.
  Implemented as Vulkan compute shaders compiled from SPIR-V at build
  time. ~500 lines runtime (Vulkan init, buffer management, dispatch)
  + ~200 lines codegen (emit SPIR-V for the lambda). No new syntax.
  Yona is uniquely suited: (1) no aliasing — persistent data structures
  guarantee no data races by construction, (2) effect tracking — pure
  functions are GPU-eligible without annotation, (3) unboxed arrays
  (IntArray/FloatArray) are contiguous with no per-element headers —
  zero-copy GPU upload, (4) LTO infrastructure extends naturally to
  GPU kernel extraction.
- [ ] **Transparent GPU lowering** (future). Compiler automatically
  lowers FloatArray.map/foldl to GPU when the array is large enough
  and the lambda is pure. Transparent, like Yona's transparent async.
  Inspired by: Halide (algorithm/schedule separation — effects as
  schedules), Julia (@cuda JIT specialization — maps to Yona's
  deferred compilation), ArrayFire/Accelerate (combinator fusion →
  GPU dispatch — maps to Yona's stream fusion).

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
- (all benchmarks now have reference impls in C + Erlang + Haskell +
  Java + Node.js + Python; see `bench/reference/` and
  [benchmark-results.md](./benchmark-results.md))

### Distribution
- [ ] Windows installer (NSIS/WiX)

### Platform Support
- [ ] macOS platform layer (kqueue-based async I/O)
- [ ] Windows platform layer (IOCP-based async I/O)
- [ ] Multi-arch (aarch64/arm64) build presets

### Explicitly not doing
- **Resource-limit / sandbox stdlib module** (was proposed as `Std\Cgroup`
  and then rescoped to `Std\Sandbox`). Dropped as not worth the budget:
  (1) tiny audience inside a young language, (2) duplicates what docker/
  systemd/podman already do at OS level, (3) macOS has no grouped-
  accounting primitive so the cross-platform story would be a limp, (4)
  a security-flavored API that isn't actually airtight is worse than
  nothing, (5) ~1000 lines of platform C + ongoing maintenance for a
  feature nobody has asked for. Users who need it can bind `cgroup` /
  `JobObject` / `jail` via `extern` in their own project. Revisit only
  if a real user shows up with a real use case.

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
- Persistent Dict (HAMT, splitmix64, O(1) amortized, transient inserts,
  size-delta tracking for same-key replaces via child-recurse)
- Persistent Set (HAMT-backed, union/intersection/difference)

### Memory Management
- Atomic RC (`RELAXED` inc, `ACQ_REL` dec)
- Recursive destructors for all container types (heap_mask bitmasks)
- Perceus-linear callee-owns ABI for seqs, sets, and dicts — single-use
  detection at call sites skips the rc_inc and transfers ownership to
  the callee; runtime consume paths rc_dec on path-copy so callers
  don't double-drop
- Per-branch `transferred_seqs_` / `transferred_maps_` scoping in
  if-expressions and case arms — compensating rc_decs emitted only
  where needed so one-arm-transfers don't leak on the other arm, and
  SSA dominance is preserved via the pre_blocks snapshot taken before
  branch-block creation
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

### Standard Library (~37 modules)

**Pure Yona:** Option, Result, List, Tuple, Range, Math, Pair, Bool,
Test, Collection, Function, Http, IO (non-blocking io_uring writes +
thread-pool readLine), Stream (lazy pull-based sequences with
`chunksOf`, `bracket`, pipeline parallelism via `async`/`buffered`),
Parallel, Regex, Channel, Constants\{Num,Math,Platform}

**C runtime:** String, Encoding, Types, File, Process, Random, Json,
Crypto, Log, Net, Bytes, Time, Path, Format, Dict, Set, IntArray,
FloatArray, ByteArray, Task

### Tooling & Distribution
- DWARF debug info, DiagnosticEngine with error codes and `--explain`
- Documentation system (`##` doc comments → `scripts/gendocs.py`)
- Benchmark suite (11 benchmarks with C references)
- CI/CD: GitHub Actions (Linux, macOS, Windows)
- Docker image (multi-stage Fedora build)
- Homebrew formula, RPM spec, DEB packaging
- GitHub Releases workflow (Linux + macOS binaries on tag push)
- INSTALL.md, CONTRIBUTING.md, CHANGELOG.md
