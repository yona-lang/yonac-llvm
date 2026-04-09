# Std.Types

Types -- runtime type conversions.

Provides functions to convert between Yona's primitive types:
Int, Float, Bool, and String.

## Functions

### `toInt`

```yona
toInt str =
```

Parse a string as an integer.

```yona
import toInt from Std\Types in
toInt "42"   # => 42
```

### `toFloat`

```yona
toFloat str =
```

Parse a string as a float.

```yona
import toFloat from Std\Types in
toFloat "3.14"   # => 3.14
```

### `intToString`

```yona
intToString n =
```

Convert an integer to its string representation.

```yona
import intToString from Std\Types in
intToString 42   # => "42"
```

### `floatToString`

```yona
floatToString x =
```

Convert a float to its string representation.

```yona
import floatToString from Std\Types in
floatToString 3.14   # => "3.14"
```

### `boolToString`

```yona
boolToString b =
```

Convert a boolean to `"true"` or `"false"`.

```yona
import boolToString from Std\Types in
boolToString true   # => "true"
```
