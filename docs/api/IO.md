# Std.IO

IO -- standard input and output operations.

Provides printing to stdout/stderr and reading from stdin.
`readLine` is async (io_uring on Linux).

## Functions

### `print`

```yona
print str =
```

Print a string to stdout without a trailing newline.

```yona
import print from Std\IO in
print "hello "
```

### `println`

```yona
println str =
```

Print a string to stdout followed by a newline.

```yona
import println from Std\IO in
println "hello world"
```

### `eprint`

```yona
eprint str =
```

Print a string to stderr without a trailing newline.

```yona
import eprint from Std\IO in
eprint "warning: "
```

### `eprintln`

```yona
eprintln str =
```

Print a string to stderr followed by a newline.

```yona
import eprintln from Std\IO in
eprintln "error: something went wrong"
```

### `readLine`

```yona
readLine =
```

Read a line from stdin. Async (io_uring). Blocks until input is available.

```yona
import readLine, println from Std\IO in
do
  println "What is your name?"
  let name = readLine in
  println ("Hello, " ++ name)
end
```

### `printInt`

```yona
printInt n =
```

Print an integer to stdout (no trailing newline).

```yona
import printInt from Std\IO in
printInt 42
```

### `printFloat`

```yona
printFloat x =
```

Print a float to stdout (no trailing newline).

```yona
import printFloat from Std\IO in
printFloat 3.14
```
