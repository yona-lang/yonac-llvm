# Std.Math

Integer math operations — arithmetic helpers and number theory.

All operations work on integers. For floating-point math, use
extern C functions via the FFI.

## Functions

### `abs`

```yona
abs x = if x < 0 then 0 - x else x
```

Returns the absolute value.

```
abs 5       # => 5
abs (0 - 3) # => 3
```

### `max`

```yona
max a b = if a > b then a else b
```

Returns the larger of two values.

```
max 3 7   # => 7
```

### `min`

```yona
min a b = if a < b then a else b
```

Returns the smaller of two values.

```
min 3 7   # => 3
```

### `clamp`

```yona
clamp lo hi x = if x < lo then lo else if x > hi then hi else x
```

Restricts a value to the range `[lo, hi]`.

```
clamp 0 10 15    # => 10
clamp 0 10 5     # => 5
clamp 0 10 (0-3) # => 0
```

### `sign`

```yona
sign x = if x > 0 then 1 else if x < 0 then 0 - 1 else 0
```

Returns 1 for positive, -1 for negative, 0 for zero.

```
sign 42      # => 1
sign (0 - 5) # => -1
sign 0       # => 0
```

### `isEven`

```yona
isEven x = x % 2 == 0
```

Returns `true` if the integer is even.

```
isEven 4   # => true
isEven 3   # => false
```

### `isOdd`

```yona
isOdd x = x % 2 != 0
```

Returns `true` if the integer is odd.

```
isOdd 3   # => true
isOdd 4   # => false
```

### `gcd`

```yona
gcd a b =
```

Greatest common divisor (Euclidean algorithm).

```
gcd 12 8   # => 4
gcd 15 5   # => 5
```

### `pow`

```yona
pow base exp =
```

Integer exponentiation. Uses fast exponentiation (squaring).

```
pow 2 10   # => 1024
pow 3 4    # => 81
```

### `factorial`

```yona
factorial n =
```

Factorial: `n! = 1 * 2 * ... * n`.

```
factorial 5    # => 120
factorial 0    # => 1
```

