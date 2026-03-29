# Std.Pair

ADT-based pairs with named fields — an alternative to tuples.

Unlike tuples, `Pair` is a proper ADT with named fields (`fst`, `snd`),
enabling dot access and named pattern matching.

## Types

### `type Pair a b = Pair { fst : a, snd : b }`

A pair with named fields.

## Functions

### `pair`

```yona
pair a b = Pair { fst = a, snd = b }
```

Creates a pair from two values.

```
pair 1 2   # => Pair { fst = 1, snd = 2 }
```

### `first`

```yona
first p = case p of Pair { fst = a } -> a end
```

Extracts the first element.

```
first (pair 1 2)   # => 1
```

### `second`

```yona
second p = case p of Pair { snd = b } -> b end
```

Extracts the second element.

```
second (pair 1 2)   # => 2
```

### `mapFirst`

```yona
mapFirst fn p =
```

Transforms the first element.

```
mapFirst (\x -> x * 10) (pair 3 5)   # => Pair { fst = 30, snd = 5 }
```

### `mapSecond`

```yona
mapSecond fn p =
```

Transforms the second element.

```
mapSecond (\x -> x * 10) (pair 3 5)   # => Pair { fst = 3, snd = 50 }
```

### `mapPair`

```yona
mapPair fn gn p =
```

Transforms both elements with two functions.

```
mapPair (\x -> x + 1) (\x -> x * 2) (pair 3 5)   # => Pair { fst = 4, snd = 10 }
```

### `swap`

```yona
swap p =
```

Swaps the two elements.

```
swap (pair 1 2)   # => Pair { fst = 2, snd = 1 }
```

### `toTuple`

```yona
toTuple p =
```

Converts to a tuple `(a, b)`.

```
toTuple (pair 1 2)   # => (1, 2)
```

### `fromTuple`

```yona
fromTuple (a, b) = Pair { fst = a, snd = b }
```

Creates a pair from a tuple.

```
fromTuple (1, 2)   # => Pair { fst = 1, snd = 2 }
```

