# Type System & Codegen Architecture Plan

## Design Principles

1. **No boxing.** Monomorphization, not boxing. Types are always known at
   compile time via inference and deferred compilation at call sites.
2. **Homogeneous collections.** `[1, "hello"]` is a type error. Collections
   are parameterized: `Seq<Int>`, `Dict<String, Int>`, `Set<Symbol>`.
3. **Symbols are integers.** Interned at compile time to `i64` IDs. ✅ Done.
4. **Algebraic data types.** Proper sum types with named constructors.
   Replaces the `(:some, value)` / `:none` tuple+symbol encoding.
5. **Constructors are functions.** First-class, partially applicable,
   passable to higher-order functions (`map Some [1, 2, 3]`).

## Completed Work

### Symbol Interning ✅

Symbols are interned to sequential `i64` IDs at compile time.
Comparison is `icmp eq i64` (1 cycle, was `strcmp` ~20 cycles).
Pattern matching on symbols generates integer switch/select.

### Dict/Set Construction ✅

`CType::SET` and `CType::DICT` added. Element types tracked via
`TypedValue::subtypes`. Runtime functions for alloc/set/print.

## Algebraic Data Types (ADTs)

### Syntax

Haskell-style with uppercase constructors. Module-level only.
Multi-line supported via `|` continuation (lexer already suppresses
newlines after `|`).

```yona
module Std\Option exports Some, None, isSome, isNone, unwrapOr, map as

type Option a = Some a | None

isSome opt =
    case opt of
        Some _ -> true
        None   -> false
    end

isNone opt =
    case opt of
        None   -> true
        Some _ -> false
    end

unwrapOr default opt =
    case opt of
        Some value -> value
        None       -> default
    end

map fn opt =
    case opt of
        Some value -> Some (fn value)
        None       -> None
    end

end
```

Multi-line type declarations:

```yona
type Result a e =
    Ok a |
    Err e

type Tree a =
    Leaf a |
    Branch (Tree a) (Tree a)
```

Enum-style (no payload):

```yona
type Color = Red | Green | Blue
```

### Constructors as Functions

Each constructor is a first-class function:

```yona
Some 42               -- construction: Some is a 1-arity function
None                  -- zero-arity: auto-evaluated to the ADT value
map Some [1, 2, 3]   -- higher-order: produces [Some 1, Some 2, Some 3]
let f = Ok in f 42    -- first-class: bind constructor to variable
```

Constructors are registered as deferred functions in the codegen,
reusing the entire existing infrastructure: deferred compilation,
monomorphization at call sites, partial application.

Constructors must be unique within a module scope. Bare names (not
qualified by type name) — `Some x` not `Option.Some x`.

### Compiled Representation

#### Non-recursive ADTs (Option, Result, Color)

Flat LLVM struct: `{i8 tag, payload...}`. Stack-allocatable, no heap.

```llvm
; type Option a = Some a | None
; Monomorphized for Int: Option Int
%Option.Int = type { i8, i64 }

; Some 42:
%x = insertvalue { i8, i64 } undef, i8 0, 0    ; tag = 0 (Some)
%x1 = insertvalue { i8, i64 } %x, i64 42, 1    ; payload = 42

; None:
%y = insertvalue { i8, i64 } undef, i8 1, 0    ; tag = 1 (None)
; payload left undef — never read

; Pattern matching:
%tag = extractvalue { i8, i64 } %scrutinee, 0
switch i8 %tag, label %default [
    i8 0, label %some_arm
    i8 1, label %none_arm
]
some_arm:
    %value = extractvalue { i8, i64 } %scrutinee, 1
    ; ... use %value ...
```

For variants with different payload sizes, the struct uses the
largest variant's payload size. Smaller variants leave trailing
fields undef.

```llvm
; type Result a e = Ok a | Err e
; Monomorphized for (Int, String): Result Int String
; Ok has i64 payload, Err has ptr payload — both fit in i64
%Result.Int.String = type { i8, i64 }
```

#### Zero-payload variants (None, Nil, Red, Green, Blue)

Just the tag. The struct still has the payload field (for uniform
type), but it's left undef and never read.

For enum-style ADTs (all zero-payload), the struct degenerates to
just `i8` (the tag). The codegen can optimize this.

#### Recursive ADTs (List, Tree)

Heap-allocated via runtime. The value is a `ptr` to a node:

```c
// In compiled_runtime.c:
typedef struct {
    int8_t tag;
    int64_t fields[];   // flexible array member
} yona_adt_node_t;

void* yona_rt_adt_alloc(int8_t tag, int64_t num_fields);
int8_t yona_rt_adt_get_tag(void* node);
int64_t yona_rt_adt_get_field(void* node, int64_t index);
void yona_rt_adt_set_field(void* node, int64_t index, int64_t value);
```

```llvm
; type List a = Cons a (List a) | Nil

; Cons 1 (Cons 2 Nil):
%nil = call ptr @yona_rt_adt_alloc(i8 1, i64 0)          ; Nil
%inner = call ptr @yona_rt_adt_alloc(i8 0, i64 2)        ; Cons
call void @yona_rt_adt_set_field(ptr %inner, i64 0, i64 2)  ; head = 2
call void @yona_rt_adt_set_field(ptr %inner, i64 1, i64 %nil) ; tail = Nil
%outer = call ptr @yona_rt_adt_alloc(i8 0, i64 2)        ; Cons
call void @yona_rt_adt_set_field(ptr %outer, i64 0, i64 1)  ; head = 1
call void @yona_rt_adt_set_field(ptr %outer, i64 1, i64 %inner) ; tail

; Pattern matching:
%tag = call i8 @yona_rt_adt_get_tag(ptr %list)
switch i8 %tag, label %default [
    i8 0, label %cons_arm
    i8 1, label %nil_arm
]
cons_arm:
    %head = call i64 @yona_rt_adt_get_field(ptr %list, i64 0)
    %tail = call i64 @yona_rt_adt_get_field(ptr %list, i64 1)
    ; tail is a ptr to another List node
```

**Recursiveness detection:** During ADT declaration processing, check
if any variant's field types reference the type being defined. If yes,
use pointer-based layout. Otherwise, use flat struct layout.

### AST Changes

New AST node types:

```cpp
// ADT variant constructor: "Some a" or "Nil"
class AdtConstructor : public AstNode {
    string name;                      // "Some", "None", "Cons", etc.
    vector<AstNode*> field_types;     // type expressions for fields
};

// Full ADT declaration: "type Option a = Some a | None"
class AdtDeclNode : public AstNode {
    string name;                      // "Option"
    vector<string> type_params;       // ["a"]
    vector<AdtConstructor*> variants; // [Some a, None]
};

// Constructor pattern: "Some x" or "Cons h t"
class ConstructorPattern : public PatternNode {
    string constructor_name;          // "Some"
    vector<PatternNode*> sub_patterns; // [x]
};
```

Extend `ModuleExpr` with `vector<AdtDeclNode*> adt_declarations`.

### Parser Changes

1. **`parse_type_definition`**: Already handles `type Name = ...`.
   Extend to parse `|`-separated variant constructors. Each variant
   is an uppercase identifier followed by zero or more type arguments.

2. **Pattern parsing**: When `parse_pattern_primary` encounters an
   uppercase identifier that's a registered constructor, parse it as
   a `ConstructorPattern` with subsequent sub-patterns. The constructor
   registry is populated during module-level type declaration parsing.

3. **Multi-line**: `|` at end of line suppresses following newline
   (already works). So `type Foo = A Int |\n    B String` parses
   correctly.

4. **Constructor expressions**: `Some 42` is parsed as normal
   juxtaposition: `Some` is an identifier, `42` is an argument.
   During codegen, `Some` resolves to the constructor function.

### Codegen Changes

1. **`CType::ADT`**: New enum variant.

2. **Constructor registry**: Maps constructor name → type name, tag,
   arity. Populated when processing `AdtDeclNode` in `compile_module`.

3. **Constructor functions**: Registered as deferred functions.
   `Some` compiles to `\x -> insertvalue {i8, i64} undef, ...`.
   `None` compiles to a constant `{i8 1, undef}`.

4. **`codegen_case` extension**: New branch for `ConstructorPattern`.
   Extract tag from scrutinee, compare, extract payload fields.

5. **Module exports**: Type constructors added to module exports
   when the type name is exported.

### types.h Extension

```cpp
struct AdtVariant {
    string name;               // "Some", "None"
    vector<Type> field_types;  // empty for nullary constructors
};

struct AdtType {
    string name;                  // "Option"
    vector<string> type_params;   // ["a"]
    vector<AdtVariant> variants;  // [{Some, [a]}, {None, []}]
};

// Add to Type variant:
using Type = variant<..., shared_ptr<AdtType>, nullptr_t>;
```

### Implementation Phases

```
Phase 1: AST and Parser
  ├── New AST nodes (AdtDeclNode, ConstructorPattern)
  ├── Extend parse_type_definition for ADT variants
  ├── Constructor registry in parser
  ├── Constructor pattern parsing (uppercase identifiers)
  └── Multi-line type declarations

Phase 2: Codegen (non-recursive ADTs)
  ├── CType::ADT, constructor registry
  ├── Constructor functions as deferred compilation
  ├── Flat struct layout: {i8 tag, payload...}
  ├── Pattern matching: tag check + field extraction
  └── Print support for ADT values

Phase 3: Stdlib migration
  ├── Rewrite Std/Option with type Option a = Some a | None
  ├── Rewrite Std/Result with type Result a e = Ok a | Err e
  └── Update tests

Phase 4: Recursive ADTs
  ├── Recursiveness detection
  ├── Runtime: adt_alloc, get_tag, get_field, set_field
  ├── Pointer-based layout
  └── Recursive pattern matching

Phase 5: Polish
  ├── Exhaustiveness checking (warnings)
  ├── Better error messages
  └── Cross-module constructor resolution
```

### Stdlib After Migration

```yona
-- Std/Option.yona
module Std\Option exports Some, None, isSome, isNone, unwrapOr, map as

type Option a = Some a | None

isSome opt = case opt of Some _ -> true; None -> false end
isNone opt = case opt of None -> true; Some _ -> false end
unwrapOr default opt = case opt of Some v -> v; None -> default end
map fn opt = case opt of Some v -> Some (fn v); None -> None end

end

-- Std/Result.yona
module Std\Result exports Ok, Err, isOk, isErr, unwrapOr, map, mapErr as

type Result a e = Ok a | Err e

isOk r = case r of Ok _ -> true; Err _ -> false end
isErr r = case r of Err _ -> true; Ok _ -> false end
unwrapOr default r = case r of Ok v -> v; Err _ -> default end
map fn r = case r of Ok v -> Ok (fn v); Err e -> Err e end
mapErr fn r = case r of Err e -> Err (fn e); Ok v -> Ok v end

end
```

## Other Phases (unchanged)

### Cross-Module Monomorphization

Whole-program compilation first (import AST, monomorphize at call site).
`.yonai` interface files for separate compilation later.

### Records

Named LLVM structs. Field access via `extractvalue`/`getelementptr`.

### Type Classes (Future)

Dictionary passing on top of monomorphization. Not planned now.

## Performance Characteristics

| Operation | Before | After |
|-----------|--------|-------|
| Symbol comparison | `strcmp` (~20 cycles) | `icmp eq i64` (1 cycle) ✅ |
| Symbol pattern match | string compare chain | integer switch ✅ |
| ADT construction | N/A (tuple+symbol hack) | `insertvalue` (1-2 cycles) |
| ADT pattern match | N/A | `extractvalue` + `icmp` (2-3 cycles) |
| Option Some/None | 2-tuple with string tag | `{i8, i64}` struct |
| Dict/Set construction | not supported | runtime alloc + fill ✅ |
| Polymorphic functions | indirect call | monomorphized direct call |
