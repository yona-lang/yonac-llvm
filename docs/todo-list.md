# TODO List - Yona-LLVM

## Summary
- **Compiler**: Yona → LLVM IR → native executable via `yonac`
- **REPL**: `yona` — compile-and-run interactive mode
- **Tests**: 717 assertions across 74 test cases (all passing)
- **Stdlib**: 26 modules, ~270 exported functions (12 pure Yona + 14 C runtime)
- **Benchmarks**: 11/11 passing, 6 within 1.5x C, list_map_filter at 1.0x C

## Benchmark Results

| Benchmark | Yona | Yona MB | C | C MB | Ratio | Notes |
|-----------|------|---------|---|------|-------|-------|
| list_map_filter | 0.76ms | 2.7 | 0.90ms | 2.9 | **0.8x** | Stream fusion |
| tak | 68ms | 2.1 | 77ms | 2.0 | **0.9x** | Faster than C |
| sum_squares | 0.59ms | 2.1 | 0.53ms | 2.0 | **1.1x** | |
| sieve | 0.76ms | 2.9 | 0.57ms | 2.0 | 1.3x | |
| list_reverse | 0.93ms | 2.9 | 0.67ms | 2.4 | 1.4x | |
| list_sum | 0.99ms | 2.7 | 0.62ms | 2.4 | 1.6x | |
| dict_build | 1.4ms | 3.2 | 0.68ms | 2.3 | **2.1x** | HAMT transient |
| set_build | 1.4ms | 3.2 | 0.76ms | 2.2 | **1.8x** | HAMT transient |
| fibonacci | 16ms | 2.1 | 7.9ms | 2.0 | 2.0x | |
| ackermann | 164ms | 2.3 | 66ms | 2.2 | 2.5x | Deep recursion |
| queens | 14ms | 42.7 | 1.4ms | 2.0 | 10.3x | Allocation-heavy (42MB vs 2MB) |

## Performance Optimization

### Completed
- [x] **Generator head/tail iteration** — O(1) per element vs O(n/32)
  for chunked seqs (cons-built). Guard comprehensions use single-pass
  indexed with over-allocation. list_map_filter: 2.7x → 1.5x C.
- [x] **Closure devirtualization** — direct calls for known lambdas
  at HOF call sites. list_sum: 1.4x→1.2x, list_reverse: 1.6x→1.5x.
- [x] **Inline seq checks** — is_empty inlined as count==0 GEP+load.
- [x] **Function inlining hints** — InlineHint on small functions.
- [x] **fastcc calling convention** — for internal non-HOF functions.
- [x] **Tail call marking** — self-recursive calls → LLVM TCE loops.
- [x] **Stream fusion** — deferred single-use generator let-bindings fused
  at codegen time. Eliminates intermediate seq allocations in chained
  comprehensions. list_map_filter: 1.5x → 1.0x C.
- [x] **LTO** — cross-module inlining via clang bitcode + llvm::Linker.
  Queens: 16x→10.8x C, list_map_filter: 1.4x→1.2x C.
- [x] **Hash-based Dict** — HAMT with splitmix64 hash, O(1) amortized.
  Transient inserts (unique-owner in-place mutation). dict_build: 1.4ms (2.1x C).
- [x] **Hash-based Set** — HAMT-backed persistent set, sharing Dict's trie.
  Transient inserts. set_build: 1.4ms (1.8x C).
  Std\Set module (insert/contains/size/elements/union/intersection/difference).
- [x] **Seq snoc** — O(1) amortized via RBT tail buffer.
- [x] **Mutual tail call optimization** — LLVM already handles this.
  Self-recursive and mutual recursive tail calls are converted to loops
  or constant-folded. Verified: collatz→loop, ping/pong→constant-fold.
- [x] **Persistent Seq RBT** — radix-balanced trie with head chain +
  tail buffer. O(1) amortized cons/head/tail via head chain, O(log32 n)
  indexed get for snoc-built elements, O(1) amortized snoc via tail buffer.
  Flat seqs (≤32) unchanged for O(1) everything.
- [x] **Branch prediction hints** — LIKELY/UNLIKELY on hot runtime paths
  (seq_cons, seq_tail, seq_head, rc_inc, rc_dec).
- [x] **Callee-borrows sharing fix** — rc_inc let-bound seqs to prevent
  unique-owner in-place tail mutation from corrupting shared bindings.

### Remaining
- [ ] **Arena allocation** — per-scope bump allocator for non-escaping
  let-bound values. Framework exists (escape analysis + arena runtime)
  but disabled: per-scope arena creation/destruction hurts recursive code
  (queens 14ms→39ms). Needs smarter heuristic: only enable for scopes
  with multiple heap-typed non-escaping bindings, not per-let-scope.
- [ ] **Profile-guided optimization** — runtime profiling for LLVM.
  Analysis: would give ~10-15% on top of current optimizations. Low
  priority — static branch hints already capture most of the benefit.

## Language Features
- [ ] STM (Software Transactional Memory) — for shared mutable state

## Stdlib Gaps
- [ ] Regex (string pattern matching) — consider PCRE2 C binding
- [ ] Process.exec non-blocking — current exec/execStatus are blocking.
  Should use io_uring or thread pool for async subprocess execution.

## Tooling
- [ ] Package manager / build system
- [ ] LSP server

## Distribution & Packaging
- [ ] RPM/DEB/Homebrew/Windows packages
- [ ] Static binary releases via GitHub Releases
- [ ] Docker image, CI/CD pipeline

## Platform Support
- [ ] Multi-arch build setup — CMake cross-compilation presets for
  aarch64/arm64 (Linux, macOS), x86_64 (Linux, macOS, Windows).
  Compiler and codegen are arch-independent (LLVM handles target triple).
  Only runtime platform layer (src/runtime/platform/) has OS-specific code.
- [ ] macOS platform layer (kqueue-based I/O)
- [ ] Windows platform layer (IOCP-based I/O)

## Completed

### Compiler
- Lexer, Parser, AST (newline-aware, juxtaposition, string interpolation)
- LLVM codegen with TypedValue (type-directed, CType tags, monomorphization)
- Proper typed returns (closures return natural types: i1/double/ptr)
- Lambda lifting, HOFs, partial application, currying, pipe operators
- Case expressions (integer, symbol, wildcard, head-tail, tuple, constructor, or-pattern, guards)
- Symbol interning (i64 IDs, icmp eq comparison)
- Generators/comprehensions (loop-based codegen for seq/set/dict + stream fusion)
- ADTs: non-recursive (flat struct), recursive (heap nodes), named fields
- Closures: env-passing convention {fn_ptr, ret_tag, arity, num_caps, heap_mask, ...}
- Branch PHI type normalization (coerce_for_phi/common_phi_type)
- Tuple representation: heap-allocated i64 arrays with RC_TYPE_TUPLE
- Nested closure capture: ExprCall chain recursion in collect_free_vars
- fastcc calling convention (post-compilation pass, HOF-safe)
- Interface files (.yonai), modules, re-exports, extern declarations
- Cross-module generics (GENFN source in .yonai, on-demand monomorphization)
- Exception handling (raise/try/catch via setjmp/longjmp)
- `with` expression, `do` blocks, async codegen (PROMISE, thread pool, io_uring)
- Optimization levels (O0-O3) via LLVM new PassManager
- Persistent Seq RBT (flat ≤32, RBT with head chain + back trie + tail buffer >32)

### Memory Management
- Atomic RC: `__atomic_fetch_add` (RELAXED) / `__atomic_fetch_sub` (ACQ_REL)
- Recursive destructors for all container types (heap_mask bitmasks)
- Weak closure self-references (break RC cycles)
- Hybrid Perceus DUP/DROP (callee-owns for non-seq, callee-borrows for seq)
- Let-bound seq protection (rc_inc prevents unique-owner tail mutation)
- Slab-based pool allocator (5 size classes, ENCODE_TAG/DECODE_TAG)
- Arena allocation for non-escaping let-bound values
- io_uring buffer pinning (copy-on-submit for write/send)
- Seq unique-owner optimization (in-place cons/tail when rc==1)
- Scope-exit RC for let-bound values
- Branch prediction hints (LIKELY/UNLIKELY on hot paths)
- See `docs/memory-management.md` for full details.

### Data Structures
- Persistent Seq: flat array (≤32) + RBT with head chain + back trie + tail buffer
- Persistent Dict: HAMT with splitmix64 hash, O(1) amortized operations
- Persistent Set: HAMT-backed (shared infrastructure with Dict)

### Type System
- Function type signatures, HOF return type inference, type annotations
- Traits: concrete/constrained instances, multi-method, default methods, superclass
- Num trait, cross-module trait dispatch, Bytes type
- infer_return_type for proper closure return types

### I/O Architecture
- io_uring backend (Linux): raw syscalls, submit-and-return, io_context registry
- Platform abstraction: uring.h, file_linux.c, net_linux.c, os_linux.c

### Tooling
- DWARF debug info, rich error messages, warning system, DiagnosticEngine
- Documentation system (`##` doc comments, gendocs.py)
- Benchmark suite: 11 benchmarks, C references, history tracking
- Stale cache detection for LTO bitcode and test runtime

### Standard Library (26 modules)
Pure Yona (12): Option, Result, List, Tuple, Range, Math, Pair, Bool, Test,
Collection, Function, Http

C runtime (14): String, Encoding, Types, IO, File, Process (exec, execStatus,
setenv, hostname, getenv, getcwd, exit), Random, Json, Crypto, Log, Net,
Bytes, Time, Path, Format, Dict, Set
