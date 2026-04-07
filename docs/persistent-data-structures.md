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

Benchmarks vs C (gcc -O2):

| Benchmark | Yona | C | Ratio |
|-----------|------|---|-------|
| list_map_filter (10K, stream-fused) | 0.8ms | 0.9ms | **0.9x** |
| list_reverse (10K) | 0.9ms | 0.7ms | 1.3x |
| dict_build (10K HAMT inserts) | 1.4ms | 0.7ms | 2.0x |
| set_build (10K HAMT inserts) | 1.4ms | 0.7ms | 2.0x |
| queens (N=10, heavy allocation) | 14ms | 1.5ms | ~5-10x |

The 2x gap on dict/set build is the inherent cost of persistence (structural sharing + RC) vs mutable flat hash tables. For workloads that benefit from persistence (versioning, concurrent access, undo), the cost is justified.
