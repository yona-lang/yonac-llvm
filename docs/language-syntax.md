# Yona Language Syntax Reference

## Overview

Yona is a functional programming language with pattern matching, first-class functions, and a module system. This document provides a comprehensive syntax reference.

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

# Multiple bindings
let 
  x = 10,
  y = 20
in 
  x + y

# Pattern matching in let
let (a, b) = (1, 2) in a + b
let [head | tail] = [1, 2, 3] in head
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
\x, y -> x + y
```

### Function Application

```yona
# Regular application
add(1, 2)
factorial(5)

# Partial application
let add5 = add(5) in
  add5(10)  # Returns 15

# Pipe operators
value |> function1 |> function2  # Left-to-right
function2 <| function1 <| value  # Right-to-left
```

## Pattern Matching

### Case Expressions

```yona
case value of
  0 -> "zero"
  1 -> "one"
  n | n > 0 -> "positive"
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

### Raise Expressions

```yona
raise :error
raise :not_found "Item not found"
```

### Try-Catch Expressions

```yona
try
  risky_operation()
catch
  :error -> "An error occurred"
  :not_found msg -> "Not found: " ++ msg
  _ -> "Unknown error"
end
```

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

## Records and Types

### Record Definitions

```yona
record Person(name: String, age: Int)
record Point(x: Float, y: Float)
```

### Record Creation and Access

```yona
let p = Person("Alice", 30) in
  p.name  # Field access

# Pattern matching
case person of
  Person(n, a) | a >= 18 -> n ++ " is an adult"
  Person(n, _) -> n ++ " is a minor"
end
```

### Type Definitions

```yona
type Maybe a = Just a | Nothing
type Either a b = Left a | Right b
type Tree a = Leaf | Node a (Tree a) (Tree a)
```

## Module Syntax

### Module Definition

```yona
module Package\ModuleName exports func1, func2 as
  # Type definitions
  type Status = Active | Inactive
  
  # Record definitions
  record User(id: Int, name: String, status: Status)
  
  # Function definitions
  func1(x) -> x + 1
  func2(a, b) -> a * b
  
  # Private helper (not exported)
  helper(x) -> x * 2
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