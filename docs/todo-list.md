# Yona-LLVM — Status & Roadmap

## Summary

- **Compiler**: Yona → LLVM IR → native executable via `yonac`
- **REPL**: `yona` — compile-and-run interactive mode
- **Tests**: 1211 assertions across 204 test cases (all passing)
- **Benchmarks**: 34/34 passing (rerun 2026-04-14, LLVM 22, 10 iters at -O2)
- **Stdlib**: ~37 modules (non-blocking Std\IO, Std\Stream, Std\Constants\{Num,Math,Platform}, Std\Channel, Std\Task)
- **Features**: Algebraic effects, transparent async, persistent data structures, traits
- **Packaging**: Docker, Homebrew, RPM, DEB, GitHub Releases

## Benchmark Results

Rerun on LLVM 22, 2026-04-14, 10 iterations. Sorted by Yona/C ratio.

> **Head-tail drop fix (2026-04-14)**: `case seq of [h|t] -> …` arms now
> rc_inc the scrutinee + force `seq_tail` onto the copy path, and rc_dec
> both at arm exit. This plugs a seq leak that pushed `queens` to 43 MB /
> 10.9x C; post-fix queens is 2.4 MB (matching C) but jumped in wall
> time from 14ms to 36ms because every tail now copies. Several list_*
> benchmarks also regressed on time and memory — tracked as separate
> perf/correctness followups in the open list.
>
> **RBT chunk leak fix (2026-04-14)**: the RBT seq's cons-promote and
> tail-chain-pop paths were over-rc_inc'ing head_next — leaking one ref
> on nearly every chunk for large (>32-element) seqs. `list_sum` showed
> 312 chunk allocs and only 1 free. Fixed by making ownership transfer
> cleanly in the unique paths: when `r->head_next` is rewritten to point
> at a different chunk, the rc is transferred, not duplicated. 312/312
> freed after the fix.
>
> **Head-tail consume path (2026-04-14)**: added `yona_rt_seq_tail_consume`
> for ownership-transfer pattern matching. `case scrut of [h|t] -> …`
> now picks one of two paths at compile time: when `scrut` is a local
> SSA value (not a function param, not re-referenced in the arm body)
> we hand it to consume and drop just `t` at arm exit — preserving the
> in-place fast path when the scrut is unique. When `scrut` is a
> function Argument or the body re-references it, we stick with the
> rc_inc + borrow-path that was added for the queens fix. The list_*
> benchmarks sit in the second bucket (scrut is foldl's seq param), so
> they still pay the copy cost; a further improvement would require
> either changing the callee-borrows convention for seq params or
> removing the defensive rc_inc on let-bound seqs.

| Benchmark | Yona | C | Ratio | Yona MB | C MB |
|-----------|------|---|-------|---------|------|
| par_map | 0.54ms | 0.64ms | **0.8x** | 2.4 | 2.4 |
| tak | 66ms | 64ms | **1.0x** | 2.4 | 2.2 |
| parallel_async | 102ms | 101ms | **1.0x** | 2.8 | 2.5 |
| sequential_async | 402ms | 401ms | **1.0x** | 2.8 | 2.2 |
| file_read | 0.78ms | 0.70ms | 1.1x | 3.6 | 3.3 |
| seq_map | 0.55ms | 0.51ms | 1.1x | 2.4 | 2.2 |
| process_exec | 1.2ms | 1.0ms | 1.1x | 3.9 | 3.9 |
| sum_squares | 0.55ms | 0.50ms | 1.1x | 2.4 | 2.2 |
| int_array_fill_sum | 0.63ms | 0.53ms | 1.2x | 2.6 | 2.3 |
| binary_write_read | 3.5ms | 2.9ms | 1.2x | 12.5 | 7.3 |
| binary_read_chunks | 0.83ms | 0.64ms | 1.3x | 2.5 | 2.2 |
| channel_pipeline | 1.3ms | 0.96ms | 1.4x | 3.2 | 2.3 |
| channel_fanin | 1.5ms | 1.1ms | 1.4x | 3.3 | 2.3 |
| channel_throughput | 1.8ms | 1.2ms | 1.5x | 3.5 | 2.2 |
| file_parallel_read | 1.3ms | 0.79ms | 1.6x | 5.8 | 5.4 |
| file_write_read | 1.4ms | 0.85ms | 1.7x | 4.8 | 3.2 |
| sieve | 0.91ms | 0.50ms | 1.8x | 3.2 | 2.1 |
| list_map_filter | 1.4ms | 0.73ms | 2.0x | 5.8 | 3.1 |
| set_build | 1.3ms | 0.59ms | 2.2x | 3.4 | 2.3 |
| sort | 1.1ms | 0.51ms | 2.2x | 2.9 | 2.2 |
| dict_build | 1.4ms | 0.63ms | 2.2x | 3.4 | 2.4 |
| fibonacci | 15ms | 6.2ms | 2.4x | 2.4 | 2.2 |
| ackermann | 165ms | 62ms | 2.7x | 2.6 | 2.4 |
| file_readlines_large | 44ms | 14ms | 3.1x | 2.6 | 2.2 |
| file_write_read_large | 46ms | 14ms | 3.3x | 107 | 2.3 |
| int_array_sum | 1.8ms | 0.52ms | 3.5x | 2.6 | 2.3 |
| int_array_map | 1.8ms | 0.52ms | 3.5x | 2.7 | 2.4 |
| list_sum | 2.1ms | 0.58ms | 3.6x | 9.1 | 2.6 |
| file_read_large | 13ms | 3.0ms | 4.2x | 55 | 2.3 |
| list_reverse | 2.7ms | 0.61ms | 4.4x | 9.1 | 2.5 |
| file_parallel_read_large | 8.6ms | 1.3ms | 6.7x | 37 | 2.4 |
| queens | 36ms | 2.3ms | 15.2x | 2.4 | 2.2 |

Erlang reference impls (bench/reference/*.erl) exist for all 17 Yona benchmarks
but are not yet wired into the runner's comparison output — that's a runner
enhancement, not a benchmark change.

## Remaining Work

### Performance
- [ ] **Seq calling convention: callee-borrows → callee-owns (Perceus-linear)** —
  highest-value open perf item. The current callee-borrows convention for
  seq parameters forces `seq_tail` onto the copy path at every function
  boundary, because in-place mutation under a borrowed seq isn't safe (the
  caller might look at it again). That's why list_sum/list_reverse/
  list_map_filter pay 2-4x vs C and queens pays 15x.

  Switching seq params to callee-owns (the Perceus linear model, already
  used for non-seq types in Yona) means: caller transfers ownership on
  call; callee rc_decs at function exit unless it forwarded the seq. The
  in-place `seq_tail` fast path then fires in foldl/map/filter/zip, which
  is the hot pattern for nearly every list-heavy program.

  Staged:
  (1) last-use dataflow analysis (~200 lines) + codegen_apply transfer
      emission for a whitelisted set of fold-style functions; verify
      list_* drops to ~1.5x.
  (2) generalize to all seq params; remove the defensive rc_incs in
      codegen_let_aliases and codegen_pattern_headtail; update the ~25
      extern C stdlib wrappers to rc_dec seq args on exit.
  (3) raise/longjmp cleanup so an uncaught raise doesn't leak owned seq
      params — needs either thread-local pending-drops lists or a
      codegen-installed setjmp unwind handler. This is the nastiest piece
      and can ship with a documented caveat initially.

  Scope: ~700 lines across codegen, runtime helpers, and stdlib C
  wrappers. Requires .yonai ABI bump (rebuild stdlib). Cross-module
  GENFN monomorphization must stay consistent on both sides.

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
