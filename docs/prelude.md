# Prelude Module

## Overview

The Prelude is a Yona module (`lib/Prelude.yona`) that is automatically loaded for all programs. Its types and functions are available without explicit imports.

## Types

```yona
type Linear a = Linear a          -- Resource wrapper (linearity checked)
type Option a = Some a | None     -- Optional value
type Result a e = Ok a | Err e    -- Error handling
type Iterator a = Iterator (() -> Option a)  -- Streaming iterator
```

These constructors are always in scope:
- `Some`, `None` — wrap/unwrap optional values
- `Ok`, `Err` — success/failure
- `Linear` — resource lifecycle tracking
- `Iterator` — streaming data source

## Functions

```yona
identity x = x           -- returns its argument
const x _ = x             -- ignores second argument
flip f a b = f b a         -- swaps argument order
compose f g x = f (g x)   -- function composition
```

## How It Works

### Loading

The Prelude is loaded in three places:

1. **Codegen constructor** (`src/Codegen.cpp`): registers ADT constructors as fallback
2. **`load_prelude()` method**: loads `Prelude.yonai` from module search paths
3. **Parser**: `register_prelude_constructors()` enables pattern matching

### Auto-Loading

In `cli/main.cpp`, after setting module paths:
```cpp
codegen.load_prelude();          // loads Prelude.yonai
parser.register_prelude_constructors();  // enables pattern matching
type_checker.register_adt(...)   // type inference for prelude ADTs
```

### Coexistence with Std Modules

`Std\Option` and `Std\Result` still exist with utility functions (`map`, `flatMap`, `unwrapOr`, etc.). The Prelude provides the types; the Std modules provide the functions.

## Updating the Prelude

1. Edit `lib/Prelude.yona`
2. Recompile: `yonac lib/Prelude.yona`
3. Move generated files: `mv Prelude.yonai lib/`
4. If new ADT constructors added, update:
   - `Parser::register_prelude_constructors()` in `src/Parser.cpp`
   - `Codegen::load_prelude()` fallback in `src/Codegen.cpp`
   - Type checker registrations in `cli/main.cpp`
5. Rebuild compiler and test
6. Commit both `.yona` and `.yonai`
