# Yona-LLVM — Status & Roadmap

## Summary

- **Compiler**: Yona → LLVM IR → native executable via `yonac`
- **REPL**: `yona` — compile-and-run interactive mode
- **Tests**: 1179 assertions across 204 test cases (all passing)
- **Benchmarks**: 31/31 passing (8 CPU, 5 collections, 10 I/O, 4 concurrency)
- **Stdlib**: 31 modules, ~330 exported functions (Channel, Task added)
- **Features**: Algebraic effects, transparent async, persistent data structures, traits
- **Packaging**: Docker, Homebrew, RPM, DEB, GitHub Releases
- **Benchmarks**: 31/31 passing, 7 at/below C parity, 21 within 2x C

## Benchmark Results

| Benchmark | Yona | C | Time | Yona MB | C MB | Mem |
|-----------|------|---|------|---------|------|-----|
| tak | 66ms | 77ms | **0.9x** | 2.3 | 2.0 | 1.2x |
| par_map | 0.65ms | 0.71ms | **0.9x** | 2.3 | 2.4 | 1.0x |
| int_array_fill_sum | 0.61ms | 0.70ms | **0.9x** | 2.5 | 2.2 | 1.1x |
| list_map_filter | 0.83ms | 0.83ms | **1.0x** | 2.9 | 3.0 | 1.0x |
| file_read | 0.88ms | 0.86ms | **1.0x** | 3.5 | 3.2 | 1.1x |
| parallel_async | 102ms | 102ms | **1.0x** | 2.8 | 2.3 | 1.2x |
| sequential_async | 402ms | 401ms | **1.0x** | 2.7 | 2.1 | 1.3x |
| binary_write_read | 3.5ms | 3.3ms | 1.1x | 12.4 | 7.2 | 1.7x |
| process_spawn | 1.3ms | 1.3ms | 1.1x | 3.9 | 3.9 | 1.0x |
| process_exec | 1.2ms | 1.1ms | 1.1x | 3.9 | 3.9 | 1.0x |
| seq_map | 0.61ms | 0.54ms | 1.1x | 2.4 | 2.1 | 1.1x |
| sum_squares | 0.62ms | 0.53ms | 1.2x | 2.4 | 2.0 | 1.2x |
| binary_read_chunks | 0.98ms | 0.76ms | 1.3x | 2.4 | 2.1 | 1.1x |
| sieve | 0.75ms | 0.53ms | 1.4x | 3.0 | 2.1 | 1.4x |
| list_sum | 0.83ms | 0.65ms | 1.3x | 3.1 | 2.5 | 1.2x |
| list_reverse | 0.89ms | 0.66ms | 1.4x | 3.2 | 2.4 | 1.3x |
| file_write_read | 1.6ms | 1.0ms | 1.6x | 4.7 | 3.1 | 1.5x |
| file_parallel_read | 1.4ms | 0.89ms | 1.6x | 5.8 | 5.2 | 1.1x |
| dict_build | 1.4ms | 0.69ms | 2.0x | 3.4 | 2.3 | 1.5x |
| fibonacci | 16ms | 7.8ms | 2.0x | 2.4 | 2.0 | 1.2x |
| set_build | 1.4ms | 0.64ms | 2.2x | 3.3 | 2.2 | 1.5x |
| ackermann | 166ms | 67ms | 2.5x | 2.2 | 2.4 | 0.9x |
| file_readlines | 2.3ms | 0.85ms | 2.7x | 7.2 | 2.1 | 3.4x |
| file_readlines_large | 39ms | 15ms | 2.6x | 2.4 | 2.0 | 1.2x |
| sort | 1.6ms | 0.57ms | 2.9x | 8.0 | 2.1 | 3.8x |
| int_array_map | 2.0ms | 0.67ms | 3.0x | 2.7 | 2.4 | 1.1x |
| int_array_sum | 1.8ms | 0.55ms | 3.3x | 2.6 | 2.2 | 1.2x |
| file_write_read_large | 51ms | 16ms | 3.2x | 107 | 2.1 | 51x |
| file_read_large | 14ms | 3.2ms | 4.5x | 55 | 2.3 | 24x |
| file_parallel_read_large | 9.4ms | 1.5ms | 6.2x | 37 | 2.3 | 16x |
| queens | 14ms | 1.3ms | 10.6x | 43 | 2.1 | 20x |

## Remaining Work

### Performance
- [x] **Unboxed Arrays (IntArray, FloatArray)** — contiguous typed arrays
  without per-element RC. `IntArray`: flat `int64_t[]`, `FloatArray`: flat
  `double[]`. O(1) random access, SIMD auto-vectorization by LLVM, cache-
  friendly bulk ops (map, foldl, filter, slice, join, cons, head, tail).
  Persistent semantics (copy-on-write set). `fromSeq`/`toSeq` conversion.
  `Std\IntArray` (15 functions) and `Std\FloatArray` (11 functions).
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
- [x] **Multi-Parameter Traits** — traits support multiple type params:
  `trait Iterable a b`. TraitDeclNode, TraitInfo, TraitInstanceInfo all have
  `type_params`/`type_names` vectors. Instance key is `"Trait:Type1:Type2"`.
  Parser, codegen, .yonai format all updated. Backward compatible with
  single-param traits.
- [x] **Iterator & Iterable** — `type Iterator a = Iterator (() -> Option a)`
  ADT in Prelude. Generator codegen detects Iterator sources and emits
  `next()` call loop. File line iterator with 64KB buffered reads in runtime
  (`yona_rt_file_line_iterator`). Foundation for streaming I/O — processes
  files line-by-line with O(64KB) memory instead of O(file_size).
  Iterable trait instances and full streaming benchmarks pending.
- [ ] **Gradual Typing with Contracts** — optional `@contract` annotations
  that generate runtime checks in debug, erased in release.

### Language — Concurrency
- [x] **Structured Concurrency** — automatic scoped concurrency with cancellation.
  Let blocks auto-group async bindings. If one fails, siblings are cancelled and
  error is propagated. Task groups in runtime with IORING_OP_ASYNC_CANCEL for
  io_uring ops. Cancel effect (`perform Cancel.check ()`) for cooperative
  cancellation. Parallel comprehensions `[| expr for x = source ]`.
  Std\Parallel module with pmap, pfor. See `docs/structured-concurrency.md`.
- [ ] **Stream Sugar** (depends on Channels) — `Stream` effect-style API that
  desugars to spawning the producer in a task and using a bounded channel.
  `perform Stream.yield x` becomes `send chan x`; the consumer reads from
  the channel via `foldl` or iteration. Provides backpressure (bounded channel),
  pipeline parallelism (producer/consumer on different threads), and composable
  pipelines. Small scope (~200 lines) once Channels exist. Solves the streaming
  use cases (chunked file processing, lazy reads, pipeline parallelism) without
  rewriting the effect system.
- [ ] **Multi-Shot Effects with Stackful Coroutines** (research, deferred) —
  extend the effect system to support multi-shot resume and true suspension
  via heap-allocated continuations or per-effect stacks. Enables: backtracking
  effects, generators with replay, async/await desugaring, full algebraic
  effects à la Koka/Effekt. Requires: function coloring or stackful coroutines,
  heap continuation frames, GC interaction. Scope: 2000+ lines. Not needed
  for streaming use cases (Stream Sugar + Channels solve those). Defer until
  there's a concrete use case beyond streaming.
- [x] **Dict/Set Iterator Instances** — streaming iterators for Dict (entries,
  keysIter, values) and Set (iterator). Stack-based HAMT trie traversal
  (14-level stack, no recursion). forEach for both. O(1) memory per element.
  File (listDir) pending. String chars/split/lines already done.
- [x] **Built-in Fold** — C loop-based `foldl`/`foldr` in runtime + Prelude.
  Handles 50K+ elements without stack overflow. General TCO blocked by
  RC cleanup after recursive calls preventing LLVM TailCallElimination.
- [x] **General TCO** — tail-call optimization for self-recursive functions.
  Pre-tail-call RC cleanup: Perceus DROP moved before the call for non-pass-through
  args. LLVM TCE converts `tail call` to loop. User-defined foldl handles 100K+ elements.
- [x] **Iterator RC Cleanup** — fixed Option ADT layout mismatch (was `[tag, value]`,
  now `[tag, num_fields, heap_mask, field]`). foldl_iterator rc_dec's each wrapper.
  Memory: 116MB → 2.4MB for 500K lines. Time: 53.9ms → 39.8ms.
- [x] **Blocking Type Checker** — Hindley-Milner inference handles all AST node
  types. Blocking enabled: type errors stop compilation in both CLI and test
  harness. Polymorphic arithmetic (`a → a → a`), generators with bound iteration
  variables, mutual recursion across nested lets (pre-scan + pre-bind), pipe
  operators, imports, pattern destructuring. E2E rejection test fixtures for
  invalid programs (6 cases). 199 tests, 1100 assertions all passing.
- [x] **Binary File I/O** — `FileHandle` ADT in Prelude wrapping fd.
  `openFile` returns `Linear FileHandle`, `Closeable FileHandle` instance for
  `with` expression RAII. Handle-based: `readBytes`, `writeBytes` (io_uring pread/pwrite
  with userspace position tracking), `seek`, `tell`, `flush`, `truncate`.
  `readChunks` returns streaming `Iterator Bytes` for chunked binary reads.
  All I/O uses explicit offsets (pread/pwrite) — safe for concurrent access.
- [ ] **Distributed Yona** — network/interprocess communication between Yona
  systems. Actor model, message passing, distributed effects, serialization.
  Erlang-style nodes, effect-based RPC, distributed task groups.
- [x] **Channels (CSP-style)** — typed bounded channels for inter-task
  communication. New `CType::CHANNEL` + `RC_TYPE_CHANNEL`. API: `channel`,
  `send` (blocks if full), `recv` (returns `Some v` / `None`), `tryRecv`,
  `close`, `isClosed`, `length`, `capacity`. Bounded ring buffer + mutex +
  two condvars. Timed wait + heuristic deadlock detection. 6 E2E test
  fixtures. See `docs/channels.md`, `docs/api/Channel.md`.
- [x] **Std\Task.spawn primitive** — `spawn : (() -> a) -> a` declared
  as `IO` so it returns a PROMISE and participates in let-binding
  auto-grouping. Wraps `yona_rt_async_spawn_closure` which uses the
  existing thread pool infrastructure.
- [ ] **Channel deadlock detection** (part of Channels) — two-tier safety:
  (1) Linear sender/receiver split: `channel : Int -> (Linear Sender a,
  Linear Receiver a)` so the LinearityChecker enforces that the sender is
  passed to a spawned producer task (or closed) at compile time. Forgetting
  `spawn` produces a linearity error. (2) Runtime deadlock detection in
  `recv`: before `cond_wait`, check if all tasks in the group are blocked
  and no I/O is in flight; if so, raise `:Deadlock`. Catches transitive
  deadlocks (cycles, crashed producers) that linear types can't see.
  Combined approach: linearity for compile-time obvious cases, runtime
  detection as safety net.
- [x] **Channel benchmarks with multi-language references** (part of Channels) —
  3 benchmarks landed: `channel_throughput` (10000 sends), `channel_pipeline`
  (5000 doubled sends), `channel_fanin` (two producers + coordinator + one
  consumer), each with C (pthreads), Python (`queue.Queue`), and Go
  (channels) reference implementations. Additional shapes (pingpong,
  fanout, actor) deferred until the parser supports `case` inside lambda
  bodies — a recurring blocker that affected nested closure consumers.
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
- [x] **Runtime Type Introspection** — `typeOf x` is a compile-time intrinsic
  returning a `Type` ADT value. Zero runtime cost — emits a constant ADT
  literal. Variants: `TInt`, `TFloat`, `TBool`, `TString`, `TSymbol`, `TUnit`,
  `TSeq`, `TSet`, `TDict`, `TTuple`, `TFunction`, `TPromise`, `TByteArray`,
  `TIntArray`, `TFloatArray`, `TAdt String`, `TSum`, `TRecord`. Type ADT
  in Prelude. Also added `FileMode` (Read/Write/ReadWrite/Append) and
  `Whence` (SeekSet/SeekCur/SeekEnd) ADTs replacing string-based dispatch
  in file I/O. Stdlib now uses ADTs over symbols/strings for type-safe APIs.
  See `docs/type-introspection.md`.
- [ ] **Compile-Time Evaluator** — evaluate pure functions at compile time.
  Enables user-defined derive strategies, constant folding, static assertions.
  Requires: subset interpreter for pure Yona expressions (no I/O, no effects).
- [ ] **User-Defined Derives** — traits declare themselves derivable via
  `derive` block that templates over ADT structure. Requires compile-time
  evaluator or external codegen tool reading enriched `.yonai` metadata.
- [ ] **Quasiquotes / Template Expressions** — `quote { expr }` captures
  AST for manipulation. `splice expr` inserts computed AST into code.
  Enables: DSLs, custom syntax extensions, code generation.

### Diagnostics
- [x] **Rich error explanations** — `yonac --explain E0100` shows detailed
  explanation with examples. Error codes (E01xx type, E02xx effect, E03xx parse,
  E04xx codegen) on all errors. Unified DiagnosticEngine with colored output,
  source context display, caret underlines, and "did you mean?" suggestions.
  Parser errors routed through DiagnosticEngine in CLI.

### Module System
- [x] **Pure-Yona Stdlib Module Loading** — `load_module_interface` falls
  back to `.yona` source files when no `.yonai` exists. Parses the source,
  registers the module's ADTs/traits/instances in the current Codegen
  state, defers function compilation via `deferred_functions_`, and
  caches by canonical path so each module is parsed at most once.
  `register_yona_module_decls` is the slimmed counterpart to
  `compile_module` (no IR verify/optimize; non-exported functions stay
  deferred). Demonstrated by `stdlib_pair_basic` test using `Std/Pair.yona`.
- [x] **Extern symbol aliasing** — `extern NAME : TYPE = "C_SYMBOL"` binds
  a Yona-friendly local name to a mangled C ABI symbol. The local name
  goes into the codegen registry; the LLVM extern function is created
  under the C symbol. Used by `Std/Channel.yona` to expose the C
  channel runtime through clean `raw_new` / `raw_send` / etc. helpers
  instead of leaking `yona_Std_Channel__channel` into every wrapper.
- [x] **Linear Sender/Receiver split for Channels** — `Std/Channel.yona`
  defines `type Sender a = Sender Channel`, `type Receiver a = Receiver Channel`,
  and `channel n = let raw = raw_new n in (Linear (Sender raw), Linear (Receiver raw))`.
  Users pattern-match the `Linear` once per side; afterwards a `Sender`
  can only `send` and a `Receiver` can only `recv`/`tryRecv`. All 6
  channel test fixtures and 3 benchmarks updated to the new pattern.
  Required codegen plumbing landed alongside: (1) ADT type-name
  propagation from extern returns (`yona_type_adt_name`,
  `CompiledFunction.return_adt_name`, `ModuleFunctionMeta.return_adt_name`)
  so wrapper functions know the ADT type of their return value;
  (2) tuple subtype propagation through wrapper returns
  (`CompiledFunction.return_subtypes`); (3) auto-boxing of non-recursive
  ADT structs when stored into a tuple slot or another ADT field
  (`codegen_tuple` and `codegen_adt_construct.to_i64`); (4) ptr restoration
  when destructuring heap-typed tuple elements in let-PatternAlias and
  case TuplePattern; (5) field-type fallback to scrutinee subtypes when
  the constructor's `field_types` is empty (generic field like `Linear a`).
- [x] **RC corruption for Linear-extracted ADTs** — fixed. Root cause was
  ADT field storage: `codegen_adt_construct` (and `codegen_tuple` boxing
  + closure capture) wrote heap-typed values into ADT fields without
  rc_inc'ing them, and the resulting boxed ADTs had `heap_mask = 0`,
  so the field's lifetime was tied solely to the original let binding.
  When the wrapper `channel n = let raw = yona_Std_Channel__channel n
  in (Linear (Sender raw), ...)` returned, `raw`'s scope cleanup
  rc_dec'd the channel and freed it, while the boxed Sender still held
  a (now dangling) pointer. Fix: rc_inc heap-typed args during ADT
  construction and propagate the inner field heap_mask onto every
  boxed copy. Channel benchmarks now run with full N=5000–10000 spawn
  producers deterministically.

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
