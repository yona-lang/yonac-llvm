# Std.Bool

Boolean combinators and conditional helpers.

Provides logical operations beyond the built-in `&&` and `||` operators,
plus conditional execution helpers.

## Functions

### `not`

```yona
not x = if x then false else true
```

Logical negation.

```
not true    # => false
not false   # => true
```

### `and`

```yona
and a b = if a then b else false
```

Logical AND (short-circuiting).

```
and true true    # => true
and true false   # => false
```

### `or`

```yona
or a b = if a then true else b
```

Logical OR (short-circuiting).

```
or false true    # => true
or false false   # => false
```

### `xor`

```yona
xor a b = if a then not b else b
```

Exclusive OR — true when exactly one argument is true.

```
xor true false   # => true
xor true true    # => false
```

### `implies`

```yona
implies a b = if a then b else true
```

Logical implication: `a → b`. False only when `a` is true and `b` is false.

```
implies true true    # => true
implies true false   # => false
implies false true   # => true
```

### `when`

```yona
when cond fn = if cond then fn () else :ok
```

Executes `fn ()` if `cond` is true, otherwise returns `:ok`.

```
when true (\-> 42)    # => 42
when false (\-> 42)   # => :ok
```

### `unless`

```yona
unless cond fn = if cond then :ok else fn ()
```

Executes `fn ()` if `cond` is false, otherwise returns `:ok`.

```
unless false (\-> 42)   # => 42
unless true (\-> 42)    # => :ok
```

