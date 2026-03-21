# Function Currying and Partial Application

## Status: Fully Implemented ✅

Yona supports automatic currying and partial application. Functions can be called with fewer arguments than their arity, producing a new function that waits for the remaining arguments.

## How It Works

### Automatic Partial Application

When a function is called with fewer arguments than it expects, the interpreter creates a closure that captures the provided arguments:

```yona
let add x y = x + y in
let add5 = add 5 in       # Partial application: returns a function waiting for y
add5 10                     # Returns 15
```

### Implementation

In `visit(ApplyExpr*)`, when `all_args.size() < func_val->arity`:
1. A new `FunctionValue` is created with the remaining arity
2. The provided arguments are captured in the closure
3. When the closure is later called with more arguments, the captured args are prepended

### Juxtaposition Application

Functions can be called via juxtaposition (space-separated arguments):

```yona
map (\x -> x * 2) [1, 2, 3]     # map applied to lambda, then to list
println "Hello"                    # println applied to string
add 3 4                           # add applied to 3, then to 4
```

Juxtaposition has higher precedence than binary operators:
```yona
f x + g y    # means (f x) + (g y)
```

### Function-Style Let Bindings

Functions can be defined with named parameters in let bindings:

```yona
let double x = x * 2 in double 5     # 10
let add x y = x + y in add 3 4       # 7
```

This desugars to `let double = \(x) -> x * 2`.

### Zero-Arity Functions

Zero-arity functions auto-evaluate when referenced (strict semantics):

```yona
let pi_val = \-> 3.14159 in
pi_val                            # Returns 3.14159 (auto-called)
```

To pass a zero-arity function as a value without calling it, wrap in a thunk:
```yona
\-> pi_val                        # Returns the function, doesn't call it
```

### Lambda Syntax

Both parenthesized and bare parameter syntax are supported:

```yona
\(x) -> x + 1      # Parenthesized
\x -> x + 1         # Bare parameter
\x y -> x + y       # Multiple bare params
\-> 42               # Zero-argument thunk
```
