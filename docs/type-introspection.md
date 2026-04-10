# Runtime Type Introspection

Yona provides `typeOf` as a built-in compile-time intrinsic that returns
a symbol representing the type of any value. This enables type-based
pattern matching, generic dispatch, and reflection-like patterns —
with **zero runtime cost**.

## Usage

```yona
typeOf 42         -- :int
typeOf 3.14       -- :float
typeOf "hello"    -- :string
typeOf true       -- :bool
typeOf :foo       -- :symbol
typeOf [1, 2, 3]  -- :seq
typeOf {1, 2}     -- :set
typeOf {1: 2}     -- :dict
typeOf (1, 2)     -- :tuple
typeOf (Some 42)  -- :Option
typeOf None       -- :Option
typeOf (Ok 1)     -- :Result
```

## Type Symbols

| CType | Symbol returned |
|-------|----------------|
| Int | `:int` |
| Float | `:float` |
| Bool | `:bool` |
| String | `:string` |
| Symbol | `:symbol` |
| Unit | `:unit` |
| Seq | `:seq` |
| Set | `:set` |
| Dict | `:dict` |
| Tuple | `:tuple` |
| Function | `:function` |
| Promise | `:promise` |
| ByteArray | `:byteArray` |
| IntArray | `:intArray` |
| FloatArray | `:floatArray` |
| ADT | The ADT type name as a symbol (e.g., `:Option`, `:Result`, `:Color`) |
| Sum | `:sum` |
| Record | `:record` |

## Pattern Matching

Combine `typeOf` with `case` for type-based dispatch:

```yona
case typeOf x of
    :int    -> "an integer"
    :string -> "a string"
    :seq    -> "a sequence"
    :dict   -> "a dictionary"
    _       -> "something else"
end
```

## Implementation: Zero Runtime Cost

`typeOf` is a **compile-time intrinsic**. The compiler intercepts calls
to `typeOf x` and replaces them with a constant symbol literal based on
the argument's compile-time type. There is no runtime type tag lookup
and no overhead.

```yona
typeOf 42
-- compiles to:
:int   -- a single i64 constant (the interned symbol ID)
```

This means:
- **Zero runtime cost** — just a constant
- **Always accurate** — the compiler knows every expression's type
- **Works everywhere** — no need for special instrumentation
- **No reflection metadata** — no extra storage per value

## Limitation: Polymorphic Functions

Inside a polymorphic function, the type parameter is monomorphized at
the call site. So `typeOf x` inside a generic function gets the *first*
type the function is compiled with:

```yona
let describe x = typeOf x in
describe 42       -- :int  (compiles describe with x: Int)
describe "hello"  -- :int  (reuses compiled version, NOT :string)
```

This is a consequence of Yona's monomorphization-based generics. To get
correct type dispatch on each call, write the `typeOf` check at the
call site rather than inside a shared function:

```yona
let describe x = case typeOf x of
    :int -> ...
    :string -> ...
end in ...

-- Or specialize via separate functions:
let describe_int (x : Int) = ... in
let describe_str (x : String) = ... in
```

Future work: support per-call-site monomorphization of `typeOf` in
polymorphic functions, similar to how Rust handles `TypeId::of::<T>()`.

## Use Cases

- **Generic serialization**: dispatch on type to choose encoding
- **Debugging/logging**: include type info in error messages
- **Type-based error handling**: match on the type of an exception value
- **Schema introspection**: build runtime descriptions from compile-time types
- **REPL display**: show different formats for different types
