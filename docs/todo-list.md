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
> Results on `queens`: 36ms → 17ms (2× faster), 43 MB → **2.4 MB**
> after generalizing per-branch `transferred_seqs_` tracking to both
> if-expressions and case arms (TransferScope helper). Fixed a
> dominance bug where the pre_blocks snapshot included the newly
> created then_bb — the block was then (incorrectly) considered
> "droppable across branches" and values defined inside it got
> rc_dec'd in sibling branches. Moved `transfer_scope_enter()` ahead
> of branch-block creation so pre_blocks only captures truly
> pre-existing blocks.
>
> **Perceus callee-owns for dicts and sets (2026-04-15)**: extended
> the seq ABI to Set/Dict HAMT operations. Runtime: `yona_rt_set_insert`
> / `yona_rt_dict_put` consume their input on path-copy (rc_dec old
> when returning a new pointer); `set_ensure_hamt` consumes the flat
> set when converting. Codegen: new `transferred_maps_` set (separate
> from `transferred_seqs_` so the per-branch transfer-scope logic in
> if/case doesn't misdrop Set/Dict values as SEQs); populated by
> `codegen_extern_call` for Set/Dict ops where arg0 and return are
> map-typed, and by `emit_direct_call` for single-use Set/Dict args
> to user-defined functions; consumed by `cleanup_let_scope` and the
> function-exit DROP check. Also fixed a HAMT size-tracking bug —
> same-key updates through a child sub-node were incrementing `size`
> regardless of whether the recursion inserted or replaced (broke
> `Set.union` correctness). Results: **0 DICT leaks** on 10k-insert
> `dict_build`/`set_build` with `let` binding (was 316/1000 before,
> ~24% leak). Root-caused an earlier blocker: two sequential
> `build N {}` calls and `Set.union` both crashed with pool UAF via
> double-dec when extern_call aliased %s and a deeper recursion
> freed the shared object.

| Benchmark | Yona | C | Ratio | Yona MB | C MB |
|-----------|------|---|-------|---------|------|
| par_map | 0.60ms | 0.80ms | **0.7x** | 2.4 | 2.4 |
| parallel_async | 101ms | 101ms | **1.0x** | 2.7 | 2.3 |
| sequential_async | 402ms | 401ms | **1.0x** | 2.8 | 2.1 |
| tak | 65ms | 64ms | **1.0x** | 2.4 | 2.2 |
| file_read | 0.82ms | 0.81ms | **1.0x** | 3.6 | 3.3 |
| int_array_fill_sum | 0.58ms | 0.52ms | 1.1x | 2.4 | 2.2 |
| list_map_filter | 0.90ms | 0.84ms | 1.1x | 2.9 | 3.0 |
| seq_map | 0.62ms | 0.55ms | 1.1x | 2.4 | 2.2 |
| binary_read_chunks | 1.0ms | 0.77ms | 1.3x | 2.4 | 2.2 |
| binary_write_read | 4.0ms | 3.2ms | 1.2x | 12.4 | 7.2 |
| process_exec | 1.4ms | 1.0ms | 1.3x | 3.8 | 3.8 |
| channel_pipeline | 1.5ms | 1.1ms | 1.3x | 3.3 | 2.2 |
| file_parallel_read | 1.2ms | 0.96ms | 1.3x | 5.8 | 5.2 |
| sum_squares | 0.81ms | 0.57ms | 1.4x | 2.4 | 2.2 |
| channel_fanin | 1.7ms | 1.2ms | 1.5x | 3.3 | 2.3 |
| list_reverse | 1.0ms | 0.66ms | 1.5x | 3.1 | 2.5 |
| channel_throughput | 2.1ms | 1.3ms | 1.6x | 3.4 | 2.2 |
| list_sum | 1.1ms | 0.66ms | 1.7x | 3.0 | 2.5 |
| file_write_read | 1.7ms | 0.92ms | 1.8x | 4.6 | 3.2 |
| sieve | 1.2ms | 0.59ms | 2.0x | 3.0 | 2.0 |
| set_build | 1.5ms | 0.63ms | 2.4x | 3.1 | 2.2 |
| dict_build | 1.4ms | 0.68ms | 2.1x | 3.2 | 2.4 |
| fibonacci | 15ms | 6.5ms | 2.4x | 2.3 | 2.1 |
| ackermann | 161ms | 61ms | 2.6x | 2.7 | 1.9 |
| int_array_sum | 1.9ms | 0.59ms | 3.3x | 2.9 | 2.2 |
| sort | 1.7ms | 0.58ms | 2.9x | 7.9 | 2.1 |
| file_readlines_large | 48ms | 15ms | 3.3x | 2.6 | 2.2 |
| int_array_map | 1.9ms | 0.62ms | 3.2x | 2.9 | 2.4 |
| file_write_read_large | 50ms | 15ms | 3.4x | 107 | 2.2 |
| file_read_large | 13ms | 3.1ms | 4.3x | 55 | 2.3 |
| file_parallel_read_large | 9.6ms | 1.5ms | 6.2x | 37 | 2.4 |
| queens | 15ms | 2.4ms | **6.3x** | 2.3 | 2.2 |

Erlang reference impls (bench/reference/*.erl) exist for all 17 Yona benchmarks
but are not yet wired into the runner's comparison output — that's a runner
enhancement, not a benchmark change.

## Remaining Work

### Bugs
None currently tracked. The "HAMT pool UAF on two sequential Set
builds" and the `Set.union` crash were both root-caused and fixed as
part of the Perceus-for-dicts-and-sets work — see the Performance
section entry below for details.

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
- [x] **Exception-safe Perceus cleanup** (phase 3, 2026-04-16).
  Implemented approach A (frame-scoped drop list) with a TLS depth
  guard so the hot path (no try active) pays only one TLS load +
  predicted-taken branch per non-TCO function with heap params.
  Known limitations: TCO functions skip frames (LLVM TCE moves init
  into the loop header); multi-use params leak the extra rc_inc'd ref
  on raise. Both are documented as follow-up items. Regression:
  ~20-35% on micro-benchmarks (TLS load overhead). Next step for
  zero-overhead: migrate to LLVM EH (invoke/landingpad) which would
  eliminate the check entirely on the happy path.
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
