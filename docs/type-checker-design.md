# Type Checker Design

## Overview

Yona's type checker implements Hindley-Milner type inference with extensions
for ADTs, traits, effects, and sum types. It runs as a separate pass between
parsing and codegen:

```
Source → Parser → AST → TypeChecker → TypeMap → Codegen → LLVM IR
```

## Architecture

### Core Components

| Component | File | Purpose |
|-----------|------|---------|
| `MonoType` | `InferType.h` | Type representation with variables, constructors, arrows |
| `TypeArena` | `TypeArena.h` | Arena allocator for types (no shared_ptr overhead) |
| `UnionFind` | `UnionFind.h` | Union-find with path compression for unification |
| `Unifier` | `Unification.h` | Unification algorithm with occurs check |
| `TypeEnv` | `TypeEnv.h` | Lexical scope chain for type bindings |
| `TypeChecker` | `TypeChecker.h` | Main inference pass, produces type map |
| `TypeError` | `TypeError.h` | Structured error messages |

### Type Representation

```
MonoType variants:
  Var(id, level)          — unification variable
  Con(Int|Float|Bool|...) — built-in type
  App("Option", [a])     — type application
  Arrow(param, ret)       — function type
  Tuple([types...])       — product type
  Effect("State", [s])    — effect row entry
  RowEmpty                — empty effect row
  RowExtend(name, args, tail) — row extension
```

### Unification Algorithm

Standard HM unification with levels for efficient generalization:
1. Chase union-find links to find representatives
2. If both are same variable → success
3. If one is variable → occurs check, level adjustment, bind
4. If both are constructors → check match, unify args pairwise
5. For effect rows → row-unification with rewriting

### Let-Polymorphism

```yona
let id = \x -> x in
(id 1, id "hello")  -- id is polymorphic: forall a. a -> a
```

1. Infer `\x -> x` at level+1 → `a -> a`
2. Generalize: variables at level > current become quantified → `forall a. a -> a`
3. Each use of `id` instantiates with fresh variables
4. `id 1` → `Int -> Int`, `id "hello"` → `String -> String`

### Error Messages

Every error includes:
- Source location (line, column)
- Expected vs actual type
- Context ("in the 2nd argument of 'map'")
- Suggestions ("did you mean 'toString x'?")

```
test.yona:15:10 error: type mismatch
  expected: Int
  found:    String
  in: the 2nd argument of 'add'

test.yona:22:5 error: missing trait instance
  no instance for 'Num String'
  in: expression 'abs name'
```

## Implementation Phases

1. **Foundation**: TypeArena, UnionFind, MonoType, basic Unification
2. **Environment**: TypeEnv, builtin types/operators registration
3. **Core Inference**: literals, identifiers, let, functions, apply, if
4. **Collections**: tuples, seqs, sets, dicts, patterns, comprehensions
5. **ADTs**: constructor types, constructor patterns, recursive types
6. **Traits**: constrained polymorphism, instance resolution
7. **Effects**: effect rows, perform/handle types
8. **Codegen Integration**: wire into pipeline, replace ad-hoc inference
9. **Polish**: error suggestions, exhaustiveness warnings
