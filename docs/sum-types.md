# Anonymous Sum Types

## Overview

Yona supports anonymous (inline) sum types that allow a value to be one of several types. Unlike ADTs which require a named type declaration, anonymous sum types use the `|` operator directly:

```yona
Int | String        -- a value that is either an Int or a String
Int | Float | Bool  -- one of three types
```

## Type-Based Pattern Matching

Use typed patterns `(name : Type)` in case expressions to match on the runtime type of a sum value:

```yona
let x = 42 in
case x of
    (n : Int) -> n + 1
    (s : String) -> 0
end
-- Result: 43
```

```yona
let x = "hello" in
case x of
    (n : Int) -> 0
    (s : String) -> 42
end
-- Result: 42
```

## How It Works

### Runtime Representation

Sum values are represented as tagged 2-tuples: `(type_tag, value)`. The type tag is an integer that identifies the runtime type of the wrapped value:

| Type | Tag |
|------|-----|
| Int | 0 |
| Float | 1 |
| Bool | 2 |
| String | 3 |
| Seq | 4 |
| Tuple | 5 |
| Symbol | 8 |
| Set | 10 |
| Dict | 11 |

### Auto-Boxing

When a typed pattern is used in a case expression, the compiler automatically boxes the scrutinee into a sum value if it isn't already one. This means you can use typed patterns on any value:

```yona
-- x is a plain Int, auto-boxed for typed pattern matching
case x of
    (n : Int) -> n + 1
    (f : Float) -> 0
end
```

### Pattern Matching Dispatch

At the LLVM level, typed pattern matching:
1. Extracts the type tag from the sum tuple (element 0)
2. Compares it with the expected tag for the pattern's type
3. On match, extracts the value (element 1) and converts it back to the appropriate LLVM type
4. On mismatch, falls through to the next pattern

## Supported Types in Patterns

All built-in types can be used in typed patterns:

- `Int` -- 64-bit integer
- `Float` -- 64-bit float
- `Bool` -- boolean
- `String` -- heap-allocated string
- `Symbol` -- interned symbol
- `Seq` -- persistent sequence
- `Set` -- persistent set
- `Dict` -- persistent dict
- `Bytes` -- byte array
- `Tuple` -- boxed tuple

## Type Checker

The type checker handles typed patterns by:
- Binding the pattern variable to the annotated type in the pattern's scope
- Returning a fresh type variable for the scrutinee (since the scrutinee type is the union of all alternatives)

## Parser Syntax

Typed patterns use parentheses with a colon separator:

```
(name : Type)
```

The parser recognizes this in pattern position by lookahead: if it sees `( IDENT : IDENT )`, it creates a `TypedPattern` node instead of a tuple pattern.

## Type Annotations on Functions

The parser already supports sum type annotations using the `|` operator:

```yona
parse_value : String -> Int | String
```

This is parsed via `parse_sum_type()` which creates a `SumType` containing all the alternative types.
