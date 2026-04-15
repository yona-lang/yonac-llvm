# Persistent Data Structures in Yona

All of Yona's built-in collections are **persistent** (immutable with structural sharing). Operations return new values without modifying the original. This enables safe concurrent access, simple reasoning, and efficient undo/versioning.

## Sequences (Lists)

Yona sequences use a hybrid representation:

- **Small (<=32 elements)**: flat array with offset-based O(1) tail
- **Large (>32 elements)**: radix-balanced trie (RBT) with head chain + tail buffer

### Complexity

| Operation | Small | Large | Notes |
|-----------|-------|-------|-------|
| cons (prepend) | O(1) | O(1) amortized | Head chain absorbs prepends |
| head | O(1) | O(1) | Direct field access |
| tail | O(1) | O(1) amortized | Offset bump (small), chain pull (large) |
| get(i) | O(1) | O(log32 n) | Flat array (small), trie descent (large) |
| length | O(1) | O(1) | Stored in header |
| concat | O(n) | O(n) | Flatten + rebuild |

### Usage

```yona
let xs = [1, 2, 3, 4, 5] in
let ys = 0 :: xs in          -- [0, 1, 2, 3, 4, 5]  — O(1)
case xs of [h|t] -> h end    -- 1  — O(1)
```

### Unique-Owner Optimization

When a sequence has reference count = 1 (only one reference), cons and tail modify it in place instead of copying. This makes recursive list processing nearly zero-allocation:

```yona
-- This foldl runs with O(1) allocation per iteration
-- because each tail result has rc=1
let foldl fn acc seq = case seq of
    [] -> acc
    [h|t] -> foldl fn (fn acc h) t
end
```

## Dictionaries (Maps)

Yona dictionaries use a **Hash Array Mapped Trie (HAMT)** with splitmix64 hashing:

### Complexity

| Operation | Time | Notes |
|-----------|------|-------|
| put | O(1) amortized | 7 levels max for 32-bit hash |
| get | O(1) amortized | |
| contains | O(1) amortized | |
| size | O(1) | Stored in root node |
| keys | O(n) | Collect all entries |

### Transient Inserts

When a HAMT node has reference count = 1, `put` modifies it in place instead of path-copying. This makes batch builds nearly as fast as mutable hash tables:

```yona
-- Building a 10K-entry dict: only ~300 allocations (trie growth)
-- instead of ~10K (one per insert)
import put, size from Std\Dict in
let build n d = if n <= 0 then d else build (n - 1) (put d n (n * n)) in
size (build 10000 {})
```

### Callee-owns ABI (Perceus)

As of 2026-04-15, `Dict.put` / `Set.insert` follow a callee-owns
convention: the callee consumes its input and returns a ref the
caller owns.

- If the implementation path-copies (growth beyond the current node,
  promotion to a child subtrie), the runtime `rc_dec`'s the old
  input before returning the new one.
- If it mutates in place (rc==1 fast path), the returned pointer
  aliases the input and the ref is simply forwarded.

At the call site, the codegen skips the rc_inc when passing a named
Dict/Set binding whose textual use-count in the enclosing function
body is 1 (last-use), marking it in `transferred_maps_` so the
function-exit drop skips it too. Combined with the runtime consume
paths, this eliminates double-drops in recursive build-chain
patterns (e.g. `let a = build N {} in let b = build N {} in …`).
`YONA_ALLOC_STATS=1` on a 10K dict_build reports `leaked=0` with a
let-bound result.

### Usage

```yona
import put, get, contains from Std\Dict in
let d = put (put {} "name" "Alice") "age" 30 in
get d "name" "unknown"      -- "Alice"
contains d "email"           -- false
```

## Sets

Sets share the HAMT infrastructure with dictionaries (elements stored as keys with value=1):

```yona
import insert, contains, union, intersection from Std\Set in
let a = {1, 2, 3, 4, 5} in
let b = {3, 4, 5, 6, 7} in
contains a 3                 -- true
Std\Set.size (union a b)     -- 7
Std\Set.size (intersection a b)  -- 3
```

## Structural Sharing

All persistent data structures share unchanged subtrees between versions:

```yona
let original = {1: "one", 2: "two", 3: "three"} in
let updated = put original 4 "four" in
-- original is unchanged
-- updated shares the subtree containing keys 1, 2, 3 with original
-- only the path to key 4 is newly allocated
```

This makes "copy-on-write" semantics efficient: modifying a 10K-entry dict only allocates O(log n) new nodes.

## Performance

Benchmarks vs C (gcc -O2), startup-adjusted; see
[benchmark-results.md](./benchmark-results.md) for the full matrix
across 7 languages.

| Benchmark | Yona | C | Ratio |
|-----------|------|---|-------|
| list_map_filter (10K, stream-fused) | 0.41ms | 0.27ms | 1.5x |
| list_reverse (10K) | 0.38ms | 0.14ms | 2.7x |
| list_sum (10K) | 0.28ms | 0.13ms | 2.2x |
| dict_build (10K HAMT inserts) | 1.08ms | 0.26ms | 4.1x |
| set_build (10K HAMT inserts) | 1.00ms | 0.26ms | 3.8x |
| queens (N=10, heavy allocation) | 14.8ms | 1.95ms | 7.6x |

The C baseline uses open-addressed linear-probing tables (a flat
array); Yona's HAMT is a 32-way trie. For workloads that benefit
from persistence (versioning, concurrent access, undo, structural
sharing between versions), this is the cost of that power. The
callee-owns ABI + unique-owner in-place optimization keep the
constant factor low enough that `dict_build`/`set_build` are
competitive with JVM `HashMap` / `HashSet` after startup adjustment
(Yona ~1 ms vs Java's adjusted 0.01 ms — Java's JIT wins these
micros but Yona's 2.4 MB baseline vs JVM's 39 MB is the real
trade-off for small programs).
