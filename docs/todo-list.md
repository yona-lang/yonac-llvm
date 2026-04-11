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
- [ ] **Stream (lazy pull-based sequence)** — `Std\Stream` module built
  on top of channels, not effects. Type:

      type Stream a = Stream (() -> Option (a, Stream a))

  Each step yields the next element + the rest of the stream — pure
  state-passing, no Cell primitive, no existentials. Operators (`map`,
  `filter`, `take`, `drop`, `zip`, `concat`, `flatMap`, `scan`,
  `chunksOf`, etc.) compose by closure, are lazy, and run sequentially
  in the consumer's task by default. Pipeline parallelism is opt-in:
  `Stream.async` spawns the upstream producer into a task and pipes
  through a bounded channel, with `Stream.buffered N` for an explicit
  buffer size. Producers: `range`, `naturals`, `repeat`, `iterate`,
  `unfold`, `fromSeq`, `fromIterator`. Terminators: `toSeq`, `foldl`,
  `forEach`, `count`, `sum`, `any`, `all`, `find`, `head`. Pure-Yona
  library (~250 lines), no compiler/runtime changes — exercises the
  channel + Linear sender/receiver + ADT-with-function-field +
  recursive let infrastructure. Deferred: resource cleanup
  (`Stream.bracket`), error forwarding across `async`, cross-operator
  fusion, multi-consumer broadcast.
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
- [x] **Parser: `case` inside lambda bodies** — fixed by adding a
  `block_depth_` counter on the lexer that tracks `case`/`do`/`with`/`handle`
  nesting and keeps newlines significant inside those blocks even when the
  surrounding parens would otherwise suppress them.
- [x] **Parser: integer literals overflow at INT32_MAX** — fixed.
  `IntegerExpr` was templated on `LiteralExpr<int>` (32-bit) and the
  parser was casting the lexer's int64 value to `int` before storing it.
  The lexer always parsed via `stoll` so the bug was purely in the AST
  node and the parser conversions. Now `IntegerExpr` uses int64_t and
  every parser callsite passes the value through unchanged. Test:
  `test/codegen/int_literal_int64.yona` exercises 1e12 + ~INT64_MAX.
- [x] **Codegen: ADT field types beyond head identifier** — fixed in
  three pieces. (1) `register_yona_module_decls` and `compile_module`
  now do a fixpoint pass over the module's ADTs marking any ADT that
  references another ADT-with-function-fields as recursive (heap
  allocated), so mutual recursion like
  `Stream a = Stream (() -> Step a)` /
  `Step a = Yield a (Stream a) | Done` correctly forces both to use
  the heap layout instead of the closure ABI's flat-struct return.
  (2) `AdtInfo` now carries `field_fn_return_types` /
  `field_fn_return_adt_names`, populated when a field is a function
  type, so the call site knows what the closure returns.
  (3) `codegen_pattern_constructor` propagates that return CType /
  ADT name into the bound `TypedValue.subtypes` of function-typed
  fields, so the higher-order call resolves the right LLVM signature.
  (4) `compile_function`'s recreate path was leaving a stale Function*
  in `compiled_functions_[name]`, so self-recursive calls inside a
  re-codegen'd body were jumping to a freed function — also fixed.
- [x] **Codegen: 0-arity exported value definitions** — fixed.
  `codegen_identifier` now auto-forces deferred functions with zero
  parameters: when an identifier resolves to a 0-param deferred def,
  compile it and call it with no args, returning the result. This
  matches Haskell-style CAFs: `naturals = range 0 N` at module
  top-level is auto-evaluated when referenced. Test:
  `test/codegen/stdlib_naturals_caf.yona`.
- [ ] **Pipe operator only accepts named functions, not partial applications** —
  `range 1 6 |> map (\x -> x * 2) |> sum` fails with "pipe: right side
  must be a function". The codegen for `|>` resolves the right operand
  by name lookup; partial applications like `map (\x -> ...)` and lambda
  literals aren't recognized as functions. Should evaluate the right
  operand as an arbitrary expression that produces a callable, then
  apply it to the left operand. Workaround: use prefix call form
  `sum (map (\x -> ...) (range 1 6))`.
- [ ] **Cons of tuple-typed element into Seq crashes** —
  `(x, y) :: rest` where rest is `Seq (Tuple)` segfaults inside `toSeq`
  on the stream returned by `zip`. Same heap-tracking story as the
  tuple-in-ADT-field bug below — Seq element storage doesn't dup the
  tuple element and the cons operation drops it. Needs the same family
  of fix in seq's cons codegen path.
- [ ] **Stream zip / tuple-in-ADT-field crashes at runtime** —
  `zip (range 1 4) (range 10 13)` from `Std\Stream` segfaults. Yield's
  first field is declared as `a` (a type variable) which the codegen
  registers as CType::INT; when zip stores a tuple `(x, y)` into that
  field the tuple pointer is stored as i64 with no heap tracking. Same
  root family as the bug-#2 fix — generic ADT fields default to INT and
  can't carry heap tracking. Workaround: avoid storing tuples in generic
  ADT fields. Proper fix: extend `codegen_adt_construct` so it
  rc_inc's heap-typed args (TUPLE/SEQ/STRING/etc.) when storing them
  into ADT fields, and propagates the heap_mask to the boxed ADT, the
  same way the post-bug-#2 path handles ADT-in-ADT.
- [ ] **Std\IO module** — standard input/output abstractions. Today the
  built-in `print` family handles most output but there's no real `IO`
  module. Should provide: `stdin`/`stdout`/`stderr` as `FileHandle`
  values, line-oriented read (`readLine`, `readLines : Iterator String`),
  formatted output (`printf`/`println` with format strings), buffered
  vs unbuffered modes, redirection helpers. Builds on the existing
  `Std\File` runtime and the `with`/`Linear` resource pattern.
- [ ] **Std\Constants module** — numeric and platform constants currently
  hard-coded throughout the stdlib. `intMax`/`intMin` (INT64_MAX/INT64_MIN),
  `floatMax`/`floatMin`/`floatEpsilon`, `pathSeparator`, `lineSeparator`,
  page size, host endianness, math constants (`pi`, `e`, `tau`),
  filesystem limits. Single source of truth so e.g. `Std\Stream.naturals`
  doesn't have to spell out `9223372036854775807` literally. Pure-Yona
  module, no C runtime needed.
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
