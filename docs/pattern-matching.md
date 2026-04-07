# Pattern Matching in Yona

Yona's pattern matching is a core language feature used in `case` expressions, function definitions, and let bindings. It supports deep structural decomposition with exhaustiveness checking.

## Case Expressions

```yona
case value of
    pattern1 -> result1
    pattern2 -> result2
    _ -> default_result
end
```

## Pattern Types

### Literals and Wildcards

```yona
case x of
    0 -> "zero"
    1 -> "one"
    _ -> "other"
end
```

### Symbols

```yona
case status of
    :ok -> "success"
    :error -> "failure"
    :pending -> "waiting"
end
```

### Or-Patterns

Match multiple alternatives:

```yona
case x of
    1 | 2 | 3 -> "small"
    _ -> "big"
end
```

### Head-Tail Decomposition

Destructure sequences into head element(s) and tail:

```yona
case list of
    [] -> "empty"
    [x] -> "one element"
    [x, y | rest] -> "at least two"
end
```

This is the primary way to iterate sequences recursively:

```yona
let sum seq = case seq of
    [] -> 0
    [h|t] -> h + sum t
end in
sum [1, 2, 3, 4, 5]
-- => 15
```

### Tuple Patterns

```yona
case (1, "hello", :ok) of
    (n, msg, :ok) -> msg
    (_, _, :error) -> "failed"
end
```

### Constructor Patterns (ADTs)

Match algebraic data type constructors and bind fields:

```yona
type Option a = Some a | None

case maybeValue of
    Some x -> x * 2
    None -> 0
end
```

Nested patterns:

```yona
type List a = Cons a (List a) | Nil

case myList of
    Cons x (Cons y _) -> x + y    -- first two elements
    Cons x Nil -> x               -- single element
    Nil -> 0                      -- empty
end
```

### Record Patterns

Match named fields of record-style ADTs:

```yona
type Person = Person { name : String, age : Int }

case person of
    Person { name = n, age = a } -> n ++ " is " ++ show a
end
```

## Exhaustiveness

The compiler warns when pattern matches are non-exhaustive:

```yona
type Color = Red | Green | Blue

case color of
    Red -> "red"
    Green -> "green"
    -- Warning: non-exhaustive pattern match — missing constructor Blue
end
```

## Guards (in function definitions)

```yona
classify x = case x of
    0 -> "zero"
    _ -> if x > 0 then "positive" else "negative"
end
```
