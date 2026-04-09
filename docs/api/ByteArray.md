# Std.ByteArray

Bytes -- mutable byte buffers for binary data.

Provides allocation, indexing, slicing, and conversion between bytes,
strings, and sequences. Useful for binary I/O, network protocols,
and interop with C libraries.

## Functions

### `alloc`

```yona
alloc size =
```

Allocate a zero-filled byte buffer of `size` bytes.

```yona
import alloc from Std\ByteArray in
let buf = alloc 1024 in
length buf   # => 1024
```

### `length`

```yona
length bytes =
```

Returns the number of bytes in the buffer.

```yona
import length from Std\ByteArray in
length (fromString "hello")   # => 5
```

### `get`

```yona
get bytes index =
```

Returns the byte value (0-255) at the given index.

```yona
import get, fromString from Std\ByteArray in
let buf = fromString "ABC" in
get buf 0   # => 65
```

### `set`

```yona
set bytes index value =
```

Sets the byte at `index` to `value` (0-255). Mutates the buffer in place.

```yona
import alloc, set, get from Std\ByteArray in
let buf = alloc 4 in
do
  set buf 0 42
  get buf 0   # => 42
end
```

### `concat`

```yona
concat a b =
```

Concatenate two byte buffers into a new buffer.

```yona
import concat, fromString from Std\ByteArray in
let buf = concat (fromString "hello ") (fromString "world") in
toString buf   # => "hello world"
```

### `slice`

```yona
slice bytes start end =
```

Extract a sub-buffer from index `start` (inclusive) to `end` (exclusive).

```yona
import slice, fromString, toString from Std\ByteArray in
toString (slice (fromString "hello") 1 4)   # => "ell"
```

### `fromString`

```yona
fromString str =
```

Convert a UTF-8 string to a byte buffer.

```yona
import fromString from Std\ByteArray in
let buf = fromString "hi" in
length buf   # => 2
```

### `toString`

```yona
toString bytes =
```

Convert a byte buffer back to a UTF-8 string.

```yona
import fromString, toString from Std\ByteArray in
toString (fromString "hello")   # => "hello"
```

### `fromSeq`

```yona
fromSeq seq =
```

Create a byte buffer from a sequence of integers (0-255).

```yona
import fromSeq, get from Std\ByteArray in
let buf = fromSeq [72, 105] in
get buf 0   # => 72
```

### `toSeq`

```yona
toSeq bytes =
```

Convert a byte buffer to a sequence of integers.

```yona
import fromString, toSeq from Std\ByteArray in
toSeq (fromString "Hi")   # => [72, 105]
```
