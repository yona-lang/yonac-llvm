# Std.FloatArray

Contiguous unboxed array of `Float` (64-bit double) values. No per-element
reference counting. O(1) random access, cache-friendly iteration.

## Functions

### alloc

```
alloc : Int -> FloatArray
```

Allocate an uninitialized FloatArray with the given number of elements.

### fill

```
fill : Int -> Float -> FloatArray
```

Create a FloatArray of `n` elements, all set to the given value.

```yona
import fill from Std\FloatArray in
fill 1000 0.0   -- 1000 zeros
```

### length

```
length : FloatArray -> Int
```

O(1) element count.

### get

```
get : FloatArray -> Int -> Float
```

O(1) indexed access.

### set

```
set : FloatArray -> Int -> Float -> FloatArray
```

Persistent set — returns a new array with the element replaced.

### head

```
head : FloatArray -> Float
```

First element. O(1).

### tail

```
tail : FloatArray -> FloatArray
```

All elements except the first.

### cons

```
cons : Float -> FloatArray -> FloatArray
```

Prepend an element.

### join

```
join : FloatArray -> FloatArray -> FloatArray
```

Concatenate two arrays.

### map

```
map : (Float -> Float) -> FloatArray -> FloatArray
```

Apply a function to each element. SIMD-eligible for simple operations.

### foldl

```
foldl : (Float -> Float -> Float) -> Float -> FloatArray -> Float
```

Left fold over all elements.

```yona
import fill, foldl from Std\FloatArray in
foldl (\acc x -> acc + x) 0.0 (fill 100 1.5)   -- 150.0
```
