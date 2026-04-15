# Yona-LLVM — Status & Roadmap

## Summary

- **Compiler**: Yona → LLVM IR → native executable via `yonac`
- **REPL**: `yona` — compile-and-run interactive mode
- **Tests**: 1213 assertions across 204 test cases (all passing)
- **Benchmarks**: 34/34 passing (rerun 2026-04-14, LLVM 22, 10 iters at -O2)
- **Stdlib**: ~37 modules (non-blocking Std\IO, Std\Stream, Std\Constants\{Num,Math,Platform}, Std\Channel, Std\Task)
- **Features**: Algebraic effects, transparent async, persistent data structures, traits
- **Packaging**: Docker, Homebrew, RPM, DEB, GitHub Releases

## Benchmark Results

Rerun on LLVM 22, 2026-04-15, 10 iterations, post-Perceus. Sorted by Yona/C ratio.

> **Perceus callee-owns for seqs (2026-04-15)**: the big one. Seq
> function parameters now follow the Perceus-linear model — caller
> transfers ownership at the call site, callee consumes (via pattern
> match or transfer onward) or drops at exit. Key pieces:
> (a) last-use detection via single-use count over the enclosing
> function body (fixed a bug in `count_identifier_refs` that silently
> dropped arguments in curried apply chains — `safe col placed 1`
> wasn't counting `placed`);
> (b) transferred-seq tracking so let-scope and function-exit drops
> skip already-transferred bindings;
> (c) unconditional scrutinee drop on the empty-seq arm (no
> pattern-match consume there);
> (d) `codegen_pattern_headtail` uses `seq_tail_consume` whenever the
> scrut is single-use function-wide, falling back to rc_inc + copy for
> multi-use scrutinees.
>
> Results on list_* benchmarks (the target):
> - list_sum: 2.1ms 9.1MB → **0.93ms 3.1MB** (2.3× faster, 3× less mem)
> - list_reverse: 2.7ms 9.1MB → **1.0ms 3.0MB** (2.7× faster, 3× less mem)
> - list_map_filter: 1.6ms 5.8MB → **0.83ms 2.9MB** (2× faster, 2× less mem)
>
> Residual: `queens` is 2× faster (36ms → 17ms). Post-if-scoping fix
> (commit c2e2f39) memory is 21 MB, down from the 37 MB peak right
> after the initial Perceus switch. The same per-branch transfer
> tracking still needs to extend to case arms to close the rest of
> the queens leak — see task #117. (The current single-use heuristic
> misses the pattern where a capture
> is used in multiple call arms). Queued as followup task #117.

| Benchmark | Yona | C | Ratio | Yona MB | C MB |
|-----------|------|---|-------|---------|------|
| par_map | 0.56ms | 0.68ms | **0.8x** | 2.4 | 2.4 |
| parallel_async | 101ms | 102ms | **1.0x** | 2.8 | 2.4 |
| sequential_async | 402ms | 401ms | **1.0x** | 2.8 | 2.2 |
| tak | 66ms | 63ms | **1.1x** | 2.4 | 2.2 |
| seq_map | 0.55ms | 0.48ms | 1.1x | 2.4 | 2.2 |
| int_array_fill_sum | 0.56ms | 0.51ms | 1.1x | 2.5 | 2.3 |
| sum_squares | 0.54ms | 0.48ms | 1.1x | 2.4 | 2.1 |
| binary_read_chunks | 0.84ms | 0.74ms | 1.1x | 2.5 | 2.3 |
| list_map_filter | 0.83ms | 0.74ms | 1.1x | 2.9 | 3.0 |
| process_exec | 1.2ms | 1.0ms | 1.2x | 3.8 | 3.9 |
| file_read | 0.88ms | 0.71ms | 1.2x | 3.6 | 3.3 |
| binary_write_read | 3.6ms | 2.7ms | 1.3x | 12.5 | 7.2 |
| channel_pipeline | 1.3ms | 0.93ms | 1.4x | 3.3 | 2.2 |
| channel_fanin | 1.6ms | 1.1ms | 1.4x | 3.2 | 2.4 |
| channel_throughput | 1.8ms | 1.3ms | 1.4x | 3.5 | 2.3 |
| list_sum | 0.93ms | 0.58ms | **1.6x** | 3.1 | 2.6 |
| file_write_read | 1.4ms | 0.84ms | 1.7x | 4.8 | 3.2 |
| file_parallel_read | 1.4ms | 0.86ms | 1.7x | 6.0 | 5.5 |
| list_reverse | 1.0ms | 0.61ms | **1.7x** | 3.0 | 2.6 |
| set_build | 1.3ms | 0.68ms | 1.9x | 3.4 | 2.3 |
| dict_build | 1.4ms | 0.64ms | 2.1x | 3.4 | 2.4 |
| sieve | 1.1ms | 0.48ms | 2.3x | 3.0 | 2.2 |
| fibonacci | 15ms | 6.2ms | 2.5x | 2.4 | 2.2 |
| sort | 1.7ms | 0.68ms | 2.5x | 8.1 | 2.1 |
| ackermann | 168ms | 62ms | 2.7x | 2.5 | 2.4 |
| file_readlines_large | 41ms | 14ms | 2.8x | 2.6 | 2.3 |
| file_write_read_large | 46ms | 14ms | 3.4x | 107 | 2.2 |
| int_array_map | 2.0ms | 0.53ms | 3.7x | 3.1 | 2.4 |
| int_array_sum | 2.0ms | 0.49ms | 4.0x | 2.9 | 2.3 |
| file_read_large | 12ms | 2.8ms | 4.3x | 55 | 2.3 |
| file_parallel_read_large | 8.1ms | 1.3ms | 6.1x | 37 | 2.3 |
| queens | 17ms | 2.5ms | **6.8x** | 21 | 2.2 |

Erlang reference impls (bench/reference/*.erl) exist for all 17 Yona benchmarks
but are not yet wired into the runner's comparison output — that's a runner
enhancement, not a benchmark change.

## Remaining Work

### Performance
- [ ] **Per-case-arm flow-sensitive transfer tracking for seqs** — the
  Perceus callee-owns conversion landed (commit b3ddc46) and then was
  extended with per-branch `transferred_seqs_` scoping at if-expressions
  (c2e2f39). Queens memory dropped 43 MB → 21 MB along the way. What
  still leaks: pattern-match case arms where one arm transfers the
  tail but another doesn't (e.g. safe's `if queen == q then false
  else … safe queen rest (col+1)` — the recursive arm transfers
  `rest` while the sibling arms don't). Extending the if-expression
  scoping logic to case expressions should eliminate most of queens'
  remaining 170k seq leak. ~100 lines in codegen_case mirroring the
  per-branch snapshot/diff logic in codegen_if.
- [ ] **Raise/longjmp cleanup for owned seqs** — phase 3 of the
  Perceus work (not yet done). An uncaught raise that propagates out
  of a function with owned seq params skips the function-exit rc_dec,
  leaking the param. Fix: thread-local pending-drops list, pushed at
  function entry and flushed by setjmp unwind handlers. Needed before
  we can call this convention robust in exceptional paths.
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
