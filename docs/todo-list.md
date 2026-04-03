# TODO List - Yona-LLVM

## Summary
- **Compiler**: Yona → LLVM IR → native executable via `yonac`
- **REPL**: `yona` — compile-and-run interactive mode
- **Tests**: 685 assertions across 74 test cases (all passing)
- **Stdlib**: 25 modules, ~260 exported functions (12 pure Yona + 13 C runtime)
- **Benchmarks**: 9/9 passing (7 of 9 within 1.5x C, queens 17x due to search tree)

## Performance Optimization

### Completed
- [x] **Generator head/tail iteration** — O(1) per element vs O(n/32).
  list_map_filter: 2.7x → 1.5x C.
- [x] **Closure devirtualization** — direct calls for known lambdas
  at HOF call sites. list_sum: 1.4x→1.2x, list_reverse: 1.6x→1.5x.
- [x] **Inline seq checks** — is_empty inlined as count==0 GEP+load.
- [x] **Function inlining hints** — InlineHint on small functions.
- [x] **fastcc calling convention** — for internal non-HOF functions.
- [x] **Tail call marking** — self-recursive calls → LLVM TCE loops.

### Remaining
- [ ] **Stream fusion** — fuse map/filter/fold chains into single loops.
- [ ] **LTO** — cross-module inlining of C runtime seq functions.
- [ ] **Hash-based Dict/Set** — HAMT for O(1) lookup.
- [ ] **Seq snoc** — O(1) append via tail-end offset.
- [ ] **Mutual tail call optimization** — musttail for A→B→A chains.
- [ ] **Persistent Seq trie** — O(log n) indexed access/concat.
- [ ] **Profile-guided optimization** — runtime profiling for LLVM.

## Language Features
- [ ] STM (Software Transactional Memory) — for shared mutable state

## Stdlib Gaps
- [ ] Regex (string pattern matching) — consider PCRE2 C binding
- [ ] Buffered I/O / readLines
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
- Generators/comprehensions (loop-based codegen for seq/set/dict)
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
- Persistent Seq (flat ≤32, chunked list >32, O(1) cons/head/tail)

### Memory Management
- Atomic RC: `__atomic_fetch_add` (RELAXED) / `__atomic_fetch_sub` (ACQ_REL)
- Recursive destructors for all 7 container types (heap_mask bitmasks)
- Weak closure self-references (break RC cycles)
- Hybrid Perceus DUP/DROP (callee-owns for non-seq, callee-borrows for seq)
- Slab-based pool allocator (4 size classes, ENCODE_TAG/DECODE_TAG)
- Arena allocation for non-escaping let-bound values
- io_uring buffer pinning (copy-on-submit for write/send)
- Seq unique-owner optimization (in-place cons/tail when rc==1)
- Scope-exit RC for let-bound values
- Temp arg cleanup for seq args with different return type
- Last-use analysis framework (ready for future Perceus extensions)
- See `docs/memory-management.md` for full details.

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
- Benchmark suite: 9 benchmarks, C references, history tracking

### Standard Library (25 modules)
Pure Yona (12): Option, Result, List, Tuple, Range, Math, Pair, Bool, Test,
Collection, Function, Http

C runtime (13): String, Encoding, Types, IO, File, Process (exec, execStatus,
setenv, hostname, getenv, getcwd, exit), Random, Json, Crypto, Log, Net,
Bytes, Time, Path, Format
