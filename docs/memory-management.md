# Memory Management in Yona-LLVM

## Overview

Yona is a compiled functional language with immutable data structures, structural
sharing, and transparent concurrency (thread pool + io_uring). Its memory system
uses **reference counting** with several optimizations:

- **Atomic RC** for thread safety
- **Recursive destructors** for all container types
- **Hybrid Perceus** DUP/DROP for non-seq heap types
- **Unique-owner optimization** for seqs (in-place modify when rc==1)
- **Weak self-references** to break recursive closure cycles
- **io_uring buffer pinning** for async I/O safety
- **Scope-exit RC** for let-bound values
- **Slab-based pool allocator** for common allocation sizes
- **Arena allocation** for non-escaping let-bound values

## RC Header Layout

Every heap-allocated value has a 2-word header before the payload:

```
[refcount: i64] [type_tag_encoded: i64] [... payload ...]
                                         ^-- pointer returned to user
```

- `refcount`: starts at 1 from `rc_alloc`. Atomic increment/decrement.
- `type_tag_encoded`: lower 8 bits = type tag, upper bits = pool class
  index + 1 (0 = not pooled, 1-4 = pool class 0-3). Encoded via
  `ENCODE_TAG(tag, cls)`, decoded via `DECODE_TAG` / `DECODE_POOL_CLASS`.
- The user-visible pointer is `header + 2`, so `rc_inc`/`rc_dec` access
  the header at `ptr - 2`.

### Type Tags

| Tag | Name | Description |
|-----|------|-------------|
| 1 | `RC_TYPE_SEQ` | Flat sequence |
| 2 | `RC_TYPE_SET` | Set |
| 3 | `RC_TYPE_DICT` | Dictionary |
| 4 | `RC_TYPE_ADT` | Algebraic data type (recursive, heap-allocated) |
| 5 | `RC_TYPE_CLOSURE` | General closure (env-passing) |
| 6 | `RC_TYPE_STRING` | String |
| 7 | `RC_TYPE_BOX` | Boxed value (generic) |
| 8 | `RC_TYPE_BYTES` | Binary data |
| 9 | `RC_TYPE_TUPLE` | Tuple |
| 11 | `RC_TYPE_CHUNKED` | Chunked sequence node |

## Container Layouts

### Flat Sequence (`RC_TYPE_SEQ`)
```
[count: i64] [heap_flag: i64] [elem0: i64] [elem1: i64] ...
```
- `SEQ_HDR_SIZE = 2` (count + heap_flag)
- Elements at `payload[SEQ_HDR_SIZE + index]`
- `heap_flag`: 1 if elements are heap-typed pointers (for recursive rc_dec)

### Chunked Sequence (`RC_TYPE_CHUNKED`)
```
[total_length: i64] [offset: i64] [count: i64] [elems: i64 * 32] [next: ptr]
```
- Linked list of 32-element chunks
- `offset`: first valid element index in `elems[]`
- `count`: number of valid elements from `offset`
- `next`: RC-managed pointer to the next chunk (or NULL)
- Structural sharing: `cons` creates a new head chunk pointing to the old

### Closure (`RC_TYPE_CLOSURE`)
```
[fn_ptr: i64] [ret_type: i64] [arity: i64] [num_captures: i64] [heap_mask: i64] [cap0: i64] ...
```
- `CLOSURE_HDR_SIZE = 5` (fn_ptr, ret_type, arity, num_captures, heap_mask)
- Captures at `payload[CLOSURE_HDR_SIZE + index]`
- `heap_mask`: bitmask — bit `i` set if capture `i` is heap-typed

### ADT (`RC_TYPE_ADT`, recursive only)
```
[tag: i64] [num_fields: i64] [heap_mask: i64] [field0: i64] [field1: i64] ...
```
- `ADT_HDR_SIZE = 3` (tag, num_fields, heap_mask)
- Fields at `payload[ADT_HDR_SIZE + index]`
- Non-recursive ADTs use LLVM struct values (no RC header)

### Tuple (`RC_TYPE_TUPLE`)
```
[num_elements: i64] [heap_mask: i64] [elem0: i64] [elem1: i64] ...
```
- Elements at `payload[2 + index]`
- `heap_mask`: bitmask of heap-typed elements

### Set (`RC_TYPE_SET`)
```
[count: i64] [heap_flag: i64] [elem0: i64] [elem1: i64] ...
```
- Elements at `payload[2 + index]`

### Dict — HAMT (`RC_TYPE_DICT`)
```
[datamap: i64] [nodemap: i64] [size: i64] [k0: i64] [v0: i64] ... [child0: ptr] ...
```
- Hash Array Mapped Trie: 32-way bitmap-compressed persistent hash map
- `datamap`: bitmap of slots with inline key-value entries
- `nodemap`: bitmap of slots with child sub-nodes
- `size`: total entries in this subtree
- Inline entries (popcount(datamap) pairs) followed by child pointers (popcount(nodemap))
- Hash function: splitmix64 for i64 keys
- O(1) amortized lookup/insert (max 7 levels)
- Persistent: insert creates new path-copy nodes, shares structure with old

## Atomic Reference Counting

All RC operations use C11 atomics for thread safety:

```c
// rc_inc: relaxed ordering (just a counter increment)
__atomic_fetch_add(&header[0], 1, __ATOMIC_RELAXED);

// rc_dec: acquire-release ordering (ensures writes visible before free)
int64_t old = __atomic_fetch_sub(&header[0], 1, __ATOMIC_ACQ_REL);
if (old <= 1) { /* free path */ }
```

### Arena Allocation

Non-escaping let-bound values are bump-allocated from a per-scope arena
instead of malloc. The escape analysis in `codegen_let` determines which
bindings don't escape (not returned, not captured by closures, not passed
to opaque functions). Non-escaping bindings use `yona_rt_arena_alloc`
which prepends an RC header with `refcount = INT64_MAX` (sentinel).
`rc_dec` checks for the sentinel and skips — arena values are freed
in bulk by `yona_rt_arena_destroy` at scope exit.

Benefits:
- Bump allocation is ~3x faster than malloc
- No per-object free (bulk deallocation)
- No RC overhead for arena values (sentinel skips rc_dec)

## Recursive Destructors

When `rc_dec` brings refcount to 0, the runtime recursively frees children
based on the type tag:

| Type | Children freed |
|------|----------------|
| Flat Seq | Each element if `heap_flag` set |
| Chunked Seq | `next` chunk pointer |
| Closure | Each capture per `heap_mask` |
| ADT | Each field per `heap_mask` |
| Tuple | Each element per `heap_mask` |
| Set | Each element if `heap_flag` set |
| Dict (HAMT) | All child sub-nodes (nodemap entries) |

The `heap_mask` is a 64-bit bitmask set by the codegen at allocation time.
It encodes which children are heap-typed (pointers that need `rc_dec`).

## Scope-Exit RC

Let-bound values are managed by the codegen at scope boundaries:

```
let x = create() in body
```

1. `create()` allocates with rc=1
2. Body is compiled
3. At scope exit: `rc_inc(result)`, then `rc_dec(x)`
4. If result IS x: net zero (rc_inc + rc_dec cancel)
5. If result is NOT x: x freed, result survives

This is implemented in `codegen_let` (`CodegenExpr.cpp`).

## Hybrid Perceus DUP/DROP

For non-seq heap types (closures, strings, ADTs, tuples), the codegen uses
a Perceus-inspired callee-owns convention:

**At call sites** (`codegen_apply`):
- For each named heap-typed arg that is NOT a seq: `rc_inc` (DUP)
- Temps (not in `named_values_`) keep their initial rc=1

**At function exit** (`compile_function`):
- For each non-seq heap-typed param: runtime `icmp eq` with return value
- If param != return: `rc_dec` (DROP)
- If param == return: skip (ownership transferred to caller)
- Params consumed by `seq_tail` (tracked in `consumed_params_`) are skipped

**Seqs use callee-borrows** (no DUP/DROP) because the seq unique-owner
optimization is incompatible with DUP-induced rc>1 at chunk boundaries.
Seq temps are freed by the different-type temp cleanup at call sites.

### Why Hybrid?

Full Perceus (DUP/DROP for all types including seqs) was implemented and
extensively tested. DUP-induced rc>1 on seq chunks triggers glibc-specific
tcache metadata corruption at chunk boundaries (N>64 elements). ASan and
valgrind find zero memory errors — the issue is in how atomic rc_dec
cascades interact with glibc's internal tcache linked lists, not in our
code. This is a known class of allocator-level constraints (discussed in
the Koka Perceus paper). The hybrid approach gives Perceus benefits for
closures/ADTs/tuples/strings while keeping fast in-place seq operations
via the callee-borrows convention.

## Seq Unique-Owner Optimization

When a seq's refcount is 1 (unique owner), operations modify it in place:

**`seq_cons` (prepend):**
- rc==1 + offset>0: write element in place, bump offset. O(1), no allocation.
- rc>1 or offset==0: allocate new chunk, copy elements. O(chunk_size).

**`seq_tail`:**
- rc==1 + count>1: bump offset in place. O(1).
- rc>1: allocate new chunk, copy elements.
- count==1: return next chunk (rc_inc next).

Unique-owner checks use `__atomic_load_n(&hdr[0], __ATOMIC_ACQUIRE)`.

## Weak Self-References

Recursive closures (e.g., `let f x = ... f (x-1) ...`) capture themselves.
Without special handling, the self-capture creates an RC cycle (closure
references itself, rc never reaches 0).

Fix: when a closure captures itself for recursion, the `rc_inc` for the
self-capture is skipped. The `heap_mask` bit for the self-capture is NOT
set, so the recursive destructor doesn't `rc_dec` it either.

Detection: `def.free_vars[ci] == fn_name` in `codegen_function_def`.

## io_uring Buffer Pinning

Async I/O operations (file write, network send) pass user buffers to the
kernel via io_uring. The kernel holds references to these buffers until
the I/O completes. If the user-side RC frees the buffer before completion,
the kernel reads freed memory.

Fix: write/send operations copy the content to an RC-managed buffer at
submit time. The buffer is `rc_dec`'d in the completer (`yona_rt_io_await`)
after the kernel signals completion.

Read/recv buffers are allocated by the runtime (not user-provided), so
they don't need pinning — they're not reachable until `io_await` returns.

## Temp Arg Cleanup

Heap-typed temporaries created as function arguments (e.g., `f (n :: xs)`)
are freed after the call returns, IF the arg's type differs from the
function's return type. This prevents leaking temps that the callee
doesn't own.

The type check (`all_args[ai].type != cf.return_type`) is conservative:
if the types match, the callee might return the arg, so we skip the DROP.

## Pool Allocator

A slab-based pool allocator reduces malloc/free overhead for common
allocation sizes:

- 4 size classes: 32B, 64B, 128B, 296B
- Thread-local free lists (`__thread pool_freelist[4]`)
- Slab allocation: `pool_grow` allocates 256 blocks at once via `malloc`
- Freed blocks go to the free list (never back to glibc `free`)
- Pool class encoded in upper bits of type_tag word (`ENCODE_TAG`/`DECODE_TAG`)
- Oversized allocations (>296B) fall through to `malloc`/`free`

**Critical constraint:** Pool blocks CANNOT be `realloc`'d because they
come from slabs (contiguous memory), not individual `malloc` calls.
The seq_cons flat realloc path checks `DECODE_POOL_CLASS(header[1])` and
skips realloc for pooled blocks, falling through to the copy path.

## Last-Use Analysis (Framework)

A backward AST walk (`LastUseAnalysis.h/cpp`) determines which reference
to each variable is the last one in a given scope. This enables:

- **DUP at non-last uses**: rc_inc to create extra references
- **No DUP at last use**: ownership transferred directly

The analysis handles:
- Binary expressions (right evaluated after left)
- If/case branching (conservative merge — both branches get DUP)
- Let bindings (body analyzed with bound names as owned set)
- Function applications (args analyzed, ExprCall chain recursed)
- Lambda captures (free variable uses tracked)

Currently used for the Perceus `perceus_tracked_` set but not fully
wired into codegen_identifier (only non-seq types get DUP/DROP).

## Concurrency Model

Yona uses transparent concurrency (async tasks on a thread pool, io_uring
for I/O). Tasks are short-lived thunks that share the caller's heap.

Memory safety is ensured by:
1. **Atomic RC** — all rc_inc/rc_dec are thread-safe
2. **Buffer pinning** — io_uring buffers survive until completion
3. **No per-task heaps** — unnecessary for short-lived tasks with shared heap

Per-task heaps (Erlang-style) were considered but rejected: Yona has no
processes or message passing, so deep-copy-on-spawn would add O(n) overhead
for every async call without significant benefit.

## Summary of RC Lifecycle

```
Value created:    rc_alloc → pool_alloc(total) → rc=1, type_tag encoded
Let binding:      no rc change (rc=1 from alloc)
Non-seq DUP:      rc_inc at call site for named args → rc++
Function param:   callee-borrows (seq) or callee-owns (non-seq)
Callee DROP:      rc_dec at function exit if param != return (non-seq only)
Scope exit:       rc_inc(result), rc_dec(binding)
Closure capture:  rc_inc (except self-capture)
Seq cons/tail:    rc_inc for structural sharing (next chunk)
Container free:   recursive rc_dec of children per heap_mask/heap_flag
Block freed:      pool_free (slab reuse) or free (oversized)
Arena values:     sentinel refcount, bulk free via arena_destroy
Non-escaping:     arena bump-alloc, no per-object RC, bulk free at scope exit
```

## Files

| File | Role |
|------|------|
| `src/compiled_runtime.c` | RC infrastructure, pool allocator, arena |
| `src/runtime/seq.c` | Persistent seq with chunked list |
| `src/codegen/CodegenUtils.cpp` | `emit_rc_inc`, `emit_rc_dec`, `is_heap_type` |
| `src/codegen/CodegenExpr.cpp` | Scope-exit RC in `codegen_let`, Perceus analysis |
| `src/codegen/CodegenFunction.cpp` | DUP at call sites, DROP at function exit |
| `src/codegen/CodegenCase.cpp` | `consumed_params_` for seq_tail |
| `src/codegen/CodegenCollections.cpp` | heap_mask for tuples/seqs |
| `src/codegen/LastUseAnalysis.cpp` | Backward AST walk for last-use detection |
| `include/LastUseAnalysis.h` | Last-use analysis API |
| `include/Codegen.h` | `CompiledFunction.closure_env`, `consumed_params_` |
| `src/runtime/platform/file_linux.c` | io_uring buffer pinning (write) |
| `src/runtime/platform/net_linux.c` | io_uring buffer pinning (send) |
