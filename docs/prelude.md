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

## Functions

```yona
identity x = x             -- returns its argument
const x _ = x               -- ignores second argument
flip f a b = f b a           -- swaps argument order
compose f g x = f (g x)     -- function composition
foldl fn acc seq             -- left fold (C loop, no stack overflow)
foldr fn acc seq             -- right fold (C loop)
```

## How It Works

### Unified Loading

`load_prelude(parser, type_checker)` is the single entry point. It reads
`Prelude.yonai` and automatically populates all subsystems:

1. **Codegen**: `register_all_imports("Prelude")` — functions available by local name
2. **Parser**: `register_constructor()` for each ADT — enables pattern matching
3. **Type checker**: `register_adt()` + `register_trait_method()` — type inference
4. **Linker**: `Prelude.o` linked with executables

No manual registration in `cli/main.cpp`, `Parser.cpp`, or anywhere else.

### Coexistence with Std Modules

`Std\Option` and `Std\Result` provide utility functions (`map`, `flatMap`, `unwrapOr`). The Prelude provides the types; Std modules provide the functions.

## Updating the Prelude

**For Yona functions/types:**
1. Edit `lib/Prelude.yona`
2. Recompile: `yonac lib/Prelude.yona && mv Prelude.yonai lib/`
3. Rebuild compiler. Done.

**For C-backed functions (foldl, foldr):**
1. Add implementation in `src/compiled_runtime.c`
2. Add `FN yona_Prelude__funcname ...` line to `lib/Prelude.yonai`
3. Rebuild. Done.

No other files need changes — `load_prelude()` reads .yonai automatically.
