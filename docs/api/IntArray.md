# Std.IntArray

Contiguous unboxed array of `Int` values. No per-element reference counting —
the array itself is a single RC-managed allocation. O(1) random access,
cache-friendly iteration, SIMD auto-vectorizable by LLVM.

Implements the `Array` trait — `length` and `get` work via trait dispatch
without explicit imports. The Prelude's polymorphic `foldl` also works on
IntArray via runtime type detection.

```yona
import fromSeq from Std\IntArray in
let arr = fromSeq [1, 2, 3, 4, 5] in
length arr               -- 5 (Array trait)
get arr 2                -- 3 (Array trait)
foldl (\a b -> a + b) 0 arr   -- 15 (polymorphic Prelude foldl)
```

## Functions

### alloc

```
alloc : Int -> IntArray
```

Allocate an uninitialized IntArray with the given number of elements.

### fill

```
fill : Int -> Int -> IntArray
```

Create an IntArray of `n` elements, all set to the given value.

```yona
import fill from Std\IntArray in
fill 1000 0   -- 1000 zeros
```

### length

```
length : IntArray -> Int
```

O(1) element count.

### get

```
get : IntArray -> Int -> Int
```

O(1) indexed access. No bounds checking.

```yona
import fromSeq, get from Std\IntArray in
get (fromSeq [10, 20, 30]) 1   -- 20
```

### set

```
set : IntArray -> Int -> Int -> IntArray
```

Persistent set — returns a new array with the element at the given index
replaced. O(n) copy.

### head

```
head : IntArray -> Int
```

First element. O(1).

### tail

```
tail : IntArray -> IntArray
```

All elements except the first. Returns a new array. O(n) copy.

### cons

```
cons : Int -> IntArray -> IntArray
```

Prepend an element. Returns a new array. O(n) copy.

### join

```
join : IntArray -> IntArray -> IntArray
```

Concatenate two arrays. O(n+m).

### slice

```
slice : IntArray -> Int -> Int -> IntArray
```

Extract a sub-array starting at `start` with `length` elements.

```yona
import fromSeq, slice, foldl from Std\IntArray in
foldl (\acc x -> acc + x) 0 (slice (fromSeq [10, 20, 30, 40, 50]) 1 3)
-- 90 (20 + 30 + 40)
```

### map

```
map : (Int -> Int) -> IntArray -> IntArray
```

Apply a function to each element, returning a new array. Single-pass,
SIMD-eligible for simple operations.

```yona
import fromSeq, map, foldl from Std\IntArray in
foldl (\acc x -> acc + x) 0 (map (\x -> x * x) (fromSeq [1, 2, 3, 4, 5]))
-- 55
```

### foldl

```
foldl : (Int -> Int -> Int) -> Int -> IntArray -> Int
```

Left fold over all elements. Single-pass, cache-friendly.

```yona
import fill, foldl from Std\IntArray in
foldl (\acc x -> acc + x) 0 (fill 100 1)   -- 100
```

### filter

```
filter : (Int -> Bool) -> IntArray -> IntArray
```

Keep elements satisfying the predicate. Two-pass (count + fill).

```yona
import fromSeq, filter, foldl from Std\IntArray in
foldl (\acc x -> acc + x) 0 (filter (\x -> x % 2 == 0) (fromSeq [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]))
-- 30
```

### fromSeq

```
fromSeq : [Int] -> IntArray
```

Convert a sequence to an IntArray. O(n) copy from boxed to unboxed.

### toSeq

```
toSeq : IntArray -> [Int]
```

Convert an IntArray to a sequence. O(n) copy from unboxed to boxed.
