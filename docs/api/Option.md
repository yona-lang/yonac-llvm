# Std.Option

Optional values — represents a value that may or may not exist.

Use `Some value` to wrap a value, `None` for absence. Chain operations
with `flatMap`, filter with predicates, or provide defaults with `unwrapOr`.

## Types

### `type Option a = Some a | None`

An optional value: either `Some value` or `None`.

## Functions

### `isSome`

```yona
isSome opt =
```

Returns `true` if the option contains a value.

```
isSome (Some 42)   # => true
isSome None        # => false
```

### `isNone`

```yona
isNone opt =
```

Returns `true` if the option is empty.

```
isNone None        # => true
isNone (Some 42)   # => false
```

### `unwrapOr`

```yona
unwrapOr default opt =
```

Extracts the value, or returns `default` if empty.

```
unwrapOr 0 (Some 42)   # => 42
unwrapOr 0 None         # => 0
```

### `map`

```yona
map fn opt =
```

Transforms the contained value with `fn`, leaving `None` unchanged.

```
map (\x -> x * 2) (Some 5)   # => Some 10
map (\x -> x * 2) None       # => None
```

### `flatMap`

```yona
flatMap fn opt =
```

Applies `fn` which itself returns an Option, flattening the result.
Useful for chaining operations that may fail.

```
flatMap (\x -> if x > 0 then Some (x * 10) else None) (Some 5)   # => Some 50
flatMap (\x -> if x > 0 then Some (x * 10) else None) (Some 0)   # => None
```

### `filter`

```yona
filter pred opt =
```

Keeps the value only if it satisfies `pred`, otherwise returns `None`.

```
filter (\x -> x > 3) (Some 5)   # => Some 5
filter (\x -> x > 3) (Some 1)   # => None
```

### `orElse`

```yona
orElse alternative opt =
```

Returns this option if it contains a value, otherwise returns `alternative`.

```
orElse (Some 99) None        # => Some 99
orElse (Some 99) (Some 42)   # => Some 42
```

### `toResult`

```yona
toResult err opt =
```

Converts to a Result: `Some v` becomes `(:ok, v)`, `None` becomes `(:err, err)`.

```
toResult "missing" (Some 42)   # => (:ok, 42)
toResult "missing" None        # => (:err, "missing")
```

### `zip`

```yona
zip optA optB =
```

Combines two options into an option of a pair. Returns `None` if either is empty.

```
zip (Some 1) (Some 2)   # => Some (1, 2)
zip (Some 1) None       # => None
```

### `fold`

```yona
fold onNone onSome opt =
```

Eliminates an option: returns `onNone` if empty, applies `onSome` if present.

```
fold 0 (\x -> x * 10) (Some 5)   # => 50
fold 0 (\x -> x * 10) None       # => 0
```

