# Yona Language Syntax Reference

## Overview

Yona is a functional programming language with pattern matching, first-class functions, and a module system. This document provides a comprehensive syntax reference.

## Newlines and Semicolons

Yona uses **newlines as expression delimiters**, similar to Python. Semicolons (`;`) are equivalent to newlines, allowing multiple expressions on one line.

```yona
# Newlines delimit expressions in case arms, do blocks, and module functions
case x of
  :ok -> handle_ok x      # newline ends this arm
  :error -> handle_error x # newline ends this arm
  _ -> default_handler x
end

# Semicolons for one-liners
case x of :ok -> 1; :error -> 2; _ -> 0 end

# Do blocks use newlines between expressions
do
  print "hello"
  print "world"
end
```

**Inside brackets** (`()`, `[]`, `{}`), newlines are ignored — expressions can span multiple lines freely:

```yona
let result = (
  some_function
    argument1
    argument2
) in result

let list = [
  1, 2, 3,
  4, 5, 6
] in list
```

**After binary operators** (`+`, `-`, `*`, `->`, `=`, `,`, etc.), newlines are also suppressed, allowing natural line continuation:

```yona
let total = price +
  tax +
  shipping in
  total
```

## Basic Types

### Literals

```yona
# Integers
42
-17
1_000_000  # Underscores for readability

# Floats
3.14
-0.5
1.23e-4

# Strings
"Hello, World!"
"Escaped \"quotes\" and \n newlines"

# Characters
'a'
'Z'
'\n'

# Booleans
true
false

# Unit (empty value)
()

# Symbols (atoms)
:ok
:error
:my_symbol
```

### Collections

```yona
# Lists/Sequences
[1, 2, 3, 4]
["apple", "banana", "cherry"]
[]  # Empty list

# Tuples
(1, "hello", true)
(42,)  # Single element tuple
()     # Unit/empty tuple

# Sets
{1, 2, 3}
{"a", "b", "c"}

# Dictionaries
{name: "Alice", age: 30}
{"key": value, :symbol: 42}
{}  # Empty dict
```

## Variables and Bindings

### Let Expressions

```yona
# Simple binding
let x = 42 in x + 1

# Multiple bindings (comma-separated)
let x = 10, y = 20 in x + y

# Function-style binding (desugars to lambda)
let add x y = x + y in add 3 4

# Pattern matching in let
let (a, b) = (1, 2) in a + b
let [head | tail] = [1, 2, 3] in head

# Discard binding
let _ = println "side effect" in 42
```

## Functions

### Function Definitions

```yona
# Simple function
add(x, y) -> x + y

# Pattern matching parameters
factorial(0) -> 1
factorial(n) -> n * factorial(n - 1)

# Guards
abs(x) | x >= 0 -> x
abs(x) | x < 0  -> -x

# Anonymous functions (lambdas)
\x -> x * 2
\(x, y) -> x + y

# Zero-argument lambda (thunk)
\-> some_expression
```

### String Interpolation

Strings can contain interpolated expressions using `{...}`:

```yona
let name = "World" in "Hello {name}!"     # "Hello World!"
let x = 6 in "result is {(x * 7)}"        # "result is 42"
"{a} + {b} = {(a + b)}"                   # "1 + 2 = 3"
```

Simple variable references use `{name}`. For expressions with operators, wrap in parentheses: `{(expr)}`. Non-string values are auto-converted.

### Zero-Argument Functions

Yona uses strict evaluation: zero-arity functions auto-evaluate when referenced by name. To pass a zero-arity function as a value (without calling it), wrap it in a thunk:

```yona
let get_time = \-> System.nanoTime in
  get_time          # Calls the function, returns current time

# To pass as a value without calling:
let deferred = \-> get_time in
  run_later deferred  # Passes the function, doesn't call it
```

### Transparent Async

Yona uses transparent async — I/O operations and other async functions return `Promise<T>` internally, but the type system automatically inserts await coercions when the value is used. The user never writes `async` or `await`:

```yona
# read_file returns Promise<String> internally, but the user sees String
let content = read_file "data.txt" in
let lines = split "\n" content in     # content is auto-awaited here
length lines

# Independent async operations run in parallel automatically
let
  a = read_file "foo.txt",     # Both reads start in parallel
  b = read_file "bar.txt"
in
  a ++ b                       # Both auto-awaited when values are needed
```

The type system uses `Promise<T>` internally to track which values are async. When `Promise<T>` appears where `T` is expected (in operators, function args, conditions), the type checker records a coercion and the runtime auto-awaits. This is invisible to the user — their code looks synchronous.

### Function Application

```yona
# Parenthesized application
add(1, 2)
factorial(5)

# Juxtaposition application (space-separated, like Haskell)
add 1 2
println "Hello, World!"
map (\(x) -> x * 2) [1, 2, 3]

# Partial application (automatic currying)
let add5 = add 5 in
  add5 10  # Returns 15

# Pipe operators
value |> function1 |> function2  # Left-to-right
function2 <| function1 <| value  # Right-to-left
```

Juxtaposition has higher precedence than all binary operators, so `f x + g y` is `(f x) + (g y)`.

## Pattern Matching

### Case Expressions

```yona
case value of
  0 -> "zero"
  1 -> "one"
  n if n > 0 -> "positive"
  _ -> "negative"
end

# Pattern matching on structures
case list of
  [] -> "empty"
  [x] -> "single: " ++ toString(x)
  [head | tail] -> "multiple"
end

# Tuple patterns
case tuple of
  (0, 0) -> "origin"
  (x, 0) -> "x-axis"
  (0, y) -> "y-axis"
  (x, y) -> "point"
end
```

### Pattern Types

```yona
# Literal patterns
42
"hello"
true

# Variable patterns (binding)
x
name

# Wildcard (ignore)
_

# Tuple patterns
(a, b, c)
(first, _, third)

# List patterns
[]
[x]
[first, second]
[head | tail]
[a, b | rest]

# Record patterns (named fields)
Person{name: n, age: a}
Point{x: _, y: yCoord}

# As patterns (alias)
[head | tail] as list
```

## Control Flow

### If Expressions

```yona
if condition then
  trueValue
else
  falseValue

# Nested if
if x > 0 then
  "positive"
else if x < 0 then
  "negative"
else
  "zero"
```

### Do Expressions

```yona
do
  expr1
  expr2
  expr3
end
```

## Exception Handling

Exceptions are ADT values. Define exception types, raise them, and catch with pattern matching.

### Exception Types

```yona
type Error = RuntimeError String | NotFound String | IOError String
```

### Raise Expressions

```yona
raise (RuntimeError "something went wrong")
raise (NotFound "file.txt")
```

### Try-Catch Expressions

```yona
try
    risky_operation
catch
    RuntimeError msg -> handle msg
    NotFound path -> report path
    _ -> default_handler
end
```

Unmatched exceptions are re-raised. Unhandled exceptions print a stack trace and abort.

## Operators

### Arithmetic

```yona
a + b   # Addition
a - b   # Subtraction
a * b   # Multiplication
a / b   # Division
a % b   # Modulo
a ** b  # Power
```

### Comparison

```yona
a == b  # Equal
a != b  # Not equal
a < b   # Less than
a > b   # Greater than
a <= b  # Less than or equal
a >= b  # Greater than or equal
```

### Logical

```yona
a && b  # Logical AND
a || b  # Logical OR
!a      # Logical NOT
```

### Bitwise

```yona
a & b   # Bitwise AND
a | b   # Bitwise OR
a ^ b   # Bitwise XOR
~a      # Bitwise NOT
a << b  # Left shift
a >> b  # Right shift
a >>> b # Zero-fill right shift
```

### List Operations

```yona
a :: b  # Cons (prepend)
a ++ b  # Concatenate
a -- b  # Remove elements
```

### Other Operators

```yona
a in b      # Membership test
a |> f      # Pipe right
f <| a      # Pipe left
record.field # Field access
```

## Generators

### List Generators

```yona
[x * 2 for x in list]
[x for x in list, x > 0]
[x + y for x in list1, y in list2]
```

### Set Generators

```yona
{x * 2 for x in list}
{x for x in list, isEven(x)}
```

### Dictionary Generators

```yona
{k: v * 2 for k, v in dict}
{name: length(name) for name in names}
```

## Algebraic Data Types

### Type Definitions

```yona
type Option a = Some a | None
type Result a e = Ok a | Err e
type Color = Red | Green | Blue
type List a = Cons a (List a) | Nil
```

### Named Fields

```yona
type Person = Person { name : String, age : Int }
```

### Construction

```yona
Some 42              # positional
None                 # zero-arity
Person { name = "Alice", age = 30 }  # named
```

### Field Access and Update

```yona
p.name               # dot syntax
p { age = 31 }       # functional update (copy with field replaced)
```

### Pattern Matching

```yona
case opt of
    Some x -> x + 1
    None   -> 0
end

case p of
    Person { name = n } -> n    # named field pattern
end
```

## Module Syntax

### Module Definition

```yona
module Package\ModuleName exports func1, func2, Active, Inactive as
  type Status = Active | Inactive

  func1 x = x + 1
  func2 a b = a * b
end
```

### Import Expressions

```yona
# Import specific functions
import func1, func2 from Package\Module in
  func1(10)

# Import with aliases
import
  longFunctionName as short,
  anotherFunction as af
from Package\Module in
  short(5)

# Import entire module
import Package\Module as M in
  M.function1(42)
```

### Extern Declarations (C FFI)

Declare external C functions with type annotations. The linker resolves the symbols.

```yona
# Call C math functions
extern sqrt : Float -> Float in
extern pow : Float -> Float -> Float in
  sqrt (pow 2.0 10.0)   # sqrt(1024.0) = 32.0

# Call C string functions
extern strlen : String -> Int in
  strlen "hello"         # 5

# Call C I/O functions
extern puts : String -> Int in
  puts "Hello from C!"
```

The type annotation uses Yona types: `Int` → `i64`, `Float` → `double`, `String` → `char*`, `Bool` → `i1`. Multiple parameters use curried syntax: `Float -> Float -> Float` means two `Float` args returning `Float`.

### Async Extern (Non-Blocking I/O)

The `async` modifier makes extern calls non-blocking — they submit to a thread pool and return a promise immediately. Auto-await happens lazily at use sites.

```yona
# Async function: returns promise immediately, runs in thread pool
extern async readFile : String -> String in
extern async httpGet : String -> String in

# Parallel let: both calls run concurrently!
let
  a = readFile "foo.txt",     # submits to pool → returns instantly
  b = httpGet "example.com"   # submits to pool → returns instantly
in
  a ++ b                       # auto-await both here (both already running)
```

Parallel let is automatic: async calls return immediately, so multiple let bindings submit all tasks before any are awaited. Total time ≈ max(task times), not sum.

## Special Forms

### With Expressions (Context Management)

```yona
with resource = acquire() as r in
  use(r)
  # resource automatically released

# Daemon mode (resource persists)
with daemon resource = startService() as s in
  s.process()
```

## Comments

```yona
# Single line comment

# Multi-line comments are just
# multiple single-line comments
# in sequence
```

## Operator Precedence

From highest to lowest:

1. Field access (`.`)
2. Function application
3. Power (`**`)
4. Unary (`!`, `~`, unary `-`)
5. Multiplicative (`*`, `/`, `%`)
6. Additive (`+`, `-`)
7. Shift (`<<`, `>>`, `>>>`)
8. Join (`++`)
9. Cons (`::`, `:>`)
10. Comparison (`<`, `>`, `<=`, `>=`)
11. Equality (`==`, `!=`)
12. Bitwise AND (`&`)
13. Bitwise XOR (`^`)
14. Bitwise OR (`|`)
15. Membership (`in`)
16. Logical AND (`&&`)
17. Logical OR (`||`)
18. Pipe (`|>`, `<|`)

## Style Conventions

1. **Naming**:
   - Functions and variables: `camelCase`
   - Modules: `PascalCase`
   - Symbols: `:snake_case`

2. **Indentation**: 2 spaces

3. **Line Length**: 80-100 characters

4. **Function Definitions**: Arrow on same line for simple functions
   ```yona
   add(x, y) -> x + y

   complex(x, y) ->
     let temp = x * y in
       if temp > 0 then temp else -temp
   ```
