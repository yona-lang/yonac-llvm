# Type System & Codegen Architecture Plan

## Problem Statement

The LLVM codegen uses an ad-hoc type system (`CType` enum with 10 flat tags) that
has reached its limits. The language already has a rich type system in `types.h`
(parameterized collections, function types, product types, sum types, records)
and a Hindley-Milner TypeChecker — but neither is connected to the codegen.

This gap blocks: typed collections (Dict, Set), algebraic data types (Option,
Result), polymorphic module functions, records, and correct symbol representation.

## Design Principles

1. **No boxing.** Yona has static H-M inference — types are always known at
   compile time. Monomorphization, not boxing, is the strategy.
2. **Homogeneous collections.** `[1, "hello"]` is a type error. Collections
   are parameterized: `Seq<Int>`, `Dict<String, Int>`, `Set<Symbol>`.
3. **Symbols are integers.** Interned at compile time to numeric IDs.
   Comparison is `icmp eq`, not `strcmp`.
4. **Bridge the TypeChecker to the codegen.** The TypeChecker provides
   resolved types; the codegen maps them to LLVM types.
5. **Monomorphize polymorphic functions.** Each call site compiles a
   concrete specialization. The existing deferred compilation is already this.

## Symbol Interning

### Current State (wrong)

Symbols (`:ok`, `:none`, `:range`) are stored as `char*` pointers to string
constants. Comparison uses `strcmp` via `yona_rt_symbol_eq`. This is:
- Slow (string comparison for every match)
- Wasteful (stores full string in binary)
- Fragile (pointer identity doesn't work, need runtime call)

### New Design

Symbols are interned to sequential `i64` IDs at compile time. A global
symbol table maps names to IDs. Comparison is a single `icmp eq i64`.

```
:ok     → 0
:err    → 1
:none   → 2
:some   → 3
:range  → 4
:pass   → 5
:fail   → 6
...
```

**In the codegen:**
- `codegen_symbol(":ok")` → `ConstantInt::get(i64, symbol_table_[":ok"])`
- Symbol comparison: `builder_->CreateICmpEQ(lhs, rhs)`
- `CType::SYMBOL` maps to `i64` (not `ptr`)
- Pattern matching on symbols: integer comparison (can use switch)

**In the runtime:**
- `yona_rt_print_symbol(i64 id)` → looks up name from a global string table
- The string table is emitted as a global array in the LLVM module:
  `@yona_symbol_names = [N x ptr] [ptr @"ok", ptr @"err", ...]`

**In the interpreter:**
- `SymbolValue` can optionally carry an integer ID alongside the string name
  for compatibility, or the interpreter continues using string-based comparison

**Impact on existing code:**
- `codegen_symbol` returns `{i64_constant, CType::SYMBOL}` instead of `{global_string_ptr, CType::SYMBOL}`
- `llvm_type(CType::SYMBOL)` returns `i64` instead of `ptr`
- Case expression symbol matching uses `icmp eq` instead of `call @yona_rt_symbol_eq`
- `codegen_print` for symbols calls `yona_rt_print_symbol(i64)` with the string table
- All codegen test fixtures with symbols continue to work (output is the same)

## Phase 1: Bridge TypeChecker → Codegen

The foundational change that enables everything else.

### 1.1 Add resolved types to AST nodes

```cpp
// In ast.h, add to AstNode:
optional<compiler::types::Type> resolved_type;
```

After type checking, every expression node carries its inferred type.

### 1.2 Complete the TypeChecker

The TypeChecker has ~40 visitor methods that return `nullptr` (stubs). These
must be completed to return correct types for:

- Binary operators (arithmetic, comparison, logical, bitwise)
- Cons (`::`) and join (`++`) operators
- Pipe operators (`|>`, `<|`)
- Collection literals (seq, set, dict)
- Pattern matching (case expressions)
- Let expressions, do blocks
- String interpolation
- Function application (apply)
- Tuple construction
- Module imports

Priority: complete the visitors needed for the codegen test suite first.

### 1.3 Wire TypeChecker into yonac pipeline

```
Parse → TypeCheck → Codegen
```

In `cli/main.cpp`, after parsing and before codegen:
1. Create a TypeChecker
2. Visit the AST root — this fills `resolved_type` on every node
3. Pass the typed AST to the codegen

### 1.4 Use resolved types in codegen

Replace the ad-hoc inference (`infer_type_from_pattern`, `infer_param_types`,
`infer_expr_type`) with reads from `node->resolved_type`:

```cpp
TypedValue Codegen::codegen(AstNode* node) {
    // The TypeChecker has resolved the type of this node
    auto type = node->resolved_type;
    // ... use type for LLVM type selection, collection element types, etc.
}
```

### 1.5 Replace CType with Type-aware TypedValue

`CType` can remain as a convenience for LLVM type dispatch, but `TypedValue`
should also carry the full `Type` for decisions that need parameterization:

```cpp
struct TypedValue {
    llvm::Value* val = nullptr;
    CType ctype = CType::INT;                        // for LLVM type dispatch
    optional<compiler::types::Type> type;             // full type for parameterized decisions
    std::vector<CType> subtypes;                      // keep for backward compat
};
```

### 1.6 Type-to-LLVM mapping

```cpp
llvm::Type* Codegen::llvm_type_of(const compiler::types::Type& type) {
    if (auto* bt = get_if<BuiltinType>(&type)) {
        switch (*bt) {
            case SignedInt64:    return i64;
            case Float64:       return double;
            case Bool:          return i1;
            case String:        return ptr;
            case Symbol:        return i64;   // interned ID
            case Unit:          return i64;   // dummy
            ...
        }
    }
    if (auto* coll = get_if<shared_ptr<SingleItemCollectionType>>(&type)) {
        return ptr;  // Seq<T> and Set<T> are heap pointers
    }
    if (auto* dict = get_if<shared_ptr<DictCollectionType>>(&type)) {
        return ptr;  // Dict<K,V> is a heap pointer
    }
    if (auto* prod = get_if<shared_ptr<ProductType>>(&type)) {
        // Tuple: LLVM struct with typed fields
        vector<llvm::Type*> fields;
        for (auto& t : (*prod)->types) fields.push_back(llvm_type_of(t));
        return StructType::get(*context_, fields);
    }
    if (auto* fn = get_if<shared_ptr<FunctionType>>(&type)) {
        return ptr;  // function pointer
    }
    if (auto* rec = get_if<shared_ptr<RecordType>>(&type)) {
        return ptr;  // pointer to named struct
    }
    if (auto* prom = get_if<shared_ptr<PromiseType>>(&type)) {
        return ptr;  // promise handle
    }
    return i64;  // fallback
}
```

## Phase 2: Typed Collections

With element types known from the TypeChecker, collections become type-safe.

### 2.1 Sequences: `Seq<T>`

**Layout:** `[count: i64, elem0: T, elem1: T, ...]`

For elements that fit in 64 bits (Int, Float, Bool, Symbol, String ptr,
Seq ptr, Function ptr), the current `int64_t*` layout works unchanged.

For compound elements (tuples, records), elements are pointers to
heap-allocated structs: `[count, ptr0, ptr1, ...]`.

The codegen uses `node->resolved_type` to determine the element type `T`,
then selects the appropriate `seq_get`/`seq_set` operations:
- `Seq<Int>`: `seq_get` returns `i64`, interpret as INT
- `Seq<String>`: `seq_get` returns `i64`, interpret as `ptr` (inttoptr)
- `Seq<(Int, String)>`: `seq_get` returns `i64`, interpret as `ptr` to struct

### 2.2 Sets: `Set<T>`

Same memory layout as `Seq<T>`. Additional runtime functions:
- `yona_rt_set_alloc(count)` → allocate
- `yona_rt_set_add(set, elem)` → returns new set with dedup
- `yona_rt_set_contains(set, elem)` → boolean check

For integer/symbol elements, dedup uses `==`. For string elements, dedup
uses `strcmp`. The comparison function is selected at compile time based on
the element type (monomorphized).

### 2.3 Dicts: `Dict<K, V>`

**Layout:** `[count: i64, key0: K, val0: V, key1: K, val1: V, ...]`

Runtime functions:
- `yona_rt_dict_alloc(count)` → allocate
- `yona_rt_dict_get(dict, key, key_size, val_size)` → find value by key
- `yona_rt_dict_put(dict, key, value, key_size, val_size)` → returns new dict
- `yona_rt_dict_contains(dict, key, key_size)` → boolean check

Key comparison is monomorphized:
- `Dict<Int, V>`: integer equality
- `Dict<Symbol, V>`: integer equality (symbols are interned IDs)
- `Dict<String, V>`: string comparison

### 2.4 Codegen for collection literals

```cpp
TypedValue Codegen::codegen_dict(DictExpr* node) {
    // Get key and value types from resolved_type: Dict<K, V>
    auto dict_type = get<shared_ptr<DictCollectionType>>(*node->resolved_type);
    auto key_ctype = ctype_of(dict_type->keyType);
    auto val_ctype = ctype_of(dict_type->valueType);

    size_t n = node->values.size();
    auto count = ConstantInt::get(i64, n);
    auto dict = builder_->CreateCall(rt_dict_alloc_, {count});

    for (size_t i = 0; i < n; i++) {
        auto key_tv = codegen(node->values[i].first);
        auto val_tv = codegen(node->values[i].second);
        auto idx = ConstantInt::get(i64, i);
        builder_->CreateCall(rt_dict_set_, {dict, idx, key_tv.val, val_tv.val});
    }
    return {dict, CType::DICT};
}
```

## Phase 3: Sum Types / Algebraic Data Types

### 3.1 Pragmatic encoding (now)

Use uniform tuples for tagged unions. Option becomes:

```yona
some value = (:some, value)
none = (:none, 0)           -- always a tuple, never bare symbol
```

Both are `(Symbol, T)` — same LLVM struct `{i64, T}` (since symbols are now
`i64` IDs). Pattern matching:

```yona
case opt of
    (:some, v) -> ...    -- compare first element == symbol_id("some")
    (:none, _) -> ...    -- compare first element == symbol_id("none")
end
```

Compiles to:
```llvm
%tag = extractvalue {i64, i64} %opt, 0
switch i64 %tag, label %default [
    i64 3, label %some_arm    ; symbol_id("some") = 3
    i64 2, label %none_arm    ; symbol_id("none") = 2
]
```

This is fast (integer switch, no string comparison) and uniform (both
variants are the same LLVM type).

### 3.2 Proper ADTs (future language extension)

```yona
type Option a = Some a | None
type Result a e = Ok a | Err e
type Tree a = Leaf a | Branch (Tree a) (Tree a)
```

Requires: new syntax, parser changes, TypeChecker support for user-defined
types, codegen for tagged union construction and destruction.

Not needed for the current stdlib — the tuple+symbol encoding works.

## Phase 4: Cross-Module Monomorphization

### 4.1 Whole-program compilation (now)

`yonac` compiles all modules together. When `main.yona` imports `map` from
`Std\List`, the compiler:

1. Parses `Std/List.yona` to get the AST
2. Type-checks it
3. Stores `map`'s AST as a deferred function
4. At each call site in `main.yona`, compiles a monomorphized `map`
   with concrete element types

This is the existing deferred compilation mechanism, extended to work
across module boundaries.

### 4.2 Separate compilation with interface files (future)

For large projects, whole-program compilation is slow. The solution:

1. Compile each module to `.o` + `.yonai` (interface file with AST of
   polymorphic functions)
2. The importer reads `.yonai` and monomorphizes at its compilation
3. Link all `.o` files together

The `.yonai` file contains:
- Module FQN
- Export list with full type signatures
- AST of polymorphic (generic) exported functions
- Pre-compiled monomorphic specializations for common types

## Phase 5: Records

Records compile as named LLVM structs:

```yona
record Person = (name : String, age : Int)
```
→ `%Person = type { ptr, i64 }`

- Construction: `Person("Alice", 30)` → `insertvalue` chain
- Field access: `person.name` → `extractvalue %Person %p, 0`
- Pattern matching: `case p of Person(n, a) -> ...` → extract fields
- Functional update: `person { age = 31 }` → copy struct, replace field

## Type Classes (Future Consideration)

Not implemented now. If needed later, implement as dictionary passing:

```yona
class Eq a where
    eq : a -> a -> Bool

instance Eq Int where
    eq x y = x == y
```

Compiles to:
```
-- Dictionary struct for Eq
type EqDict a = { eq : a -> a -> Bool }

-- Instance dictionary
eqIntDict = { eq = \x y -> x == y }

-- Polymorphic function using type class
contains : Eq a => a -> [a] -> Bool
-- becomes:
contains : EqDict a -> a -> [a] -> Bool
```

The type class constraint `Eq a =>` becomes an implicit first argument
(the dictionary). The compiler inserts the dictionary at each call site.

This layers cleanly on top of monomorphization — each monomorphized version
gets the concrete dictionary inlined and optimized away.

## Migration Strategy

### Incremental, backward-compatible

Each phase is independently implementable and testable. Existing tests
never break because monomorphic integer/float/string code paths are unchanged.

```
Phase 1: TypeChecker → Codegen bridge
  ├── 1.1: resolved_type on AST nodes
  ├── 1.2: Complete TypeChecker visitors
  ├── 1.3: Wire into yonac pipeline
  ├── 1.4: Use resolved types in codegen
  ├── 1.5: Type-aware TypedValue
  └── 1.6: Symbol interning (i64 IDs)

Phase 2: Typed collections
  ├── 2.1: Seq<T> with typed elements
  ├── 2.2: Set<T> construction + runtime
  └── 2.3: Dict<K,V> construction + runtime

Phase 3: Sum types
  ├── 3.1: Uniform tuple encoding for Option/Result
  └── 3.2: Proper ADT syntax (future)

Phase 4: Cross-module monomorphization
  ├── 4.1: Whole-program compilation
  └── 4.2: .yonai interface files (future)

Phase 5: Records
  └── Named LLVM structs with field access
```

## Performance Characteristics

| Operation | Current | After |
|-----------|---------|-------|
| Integer arithmetic | `add i64` (1 cycle) | unchanged |
| Symbol comparison | `call strcmp` (~20 cycles) | `icmp eq i64` (1 cycle) |
| Symbol pattern match | string compare per arm | integer switch |
| Seq element access | `load i64` | unchanged for `Seq<Int>` |
| Dict lookup | not supported | linear scan with typed compare |
| Function call (mono) | direct call | unchanged |
| Function call (poly) | indirect call via ptr | monomorphized → direct call |
| Tuple construction | LLVM struct | unchanged |
| Collection memory | `[count, i64, i64, ...]` | `[count, T, T, ...]` (same size for primitives) |

## What Changes in Each File

### `include/ast.h`
- Add `optional<compiler::types::Type> resolved_type` to `AstNode`

### `include/types.h`
- No structural changes needed (already rich enough)
- May add convenience functions for type queries

### `src/TypeChecker.cpp`
- Complete all stub visitors (return concrete types instead of nullptr)
- Store resolved type on each visited node via `node->resolved_type = ...`

### `include/Codegen.h`
- Add `optional<compiler::types::Type>` to `TypedValue`
- Add `llvm_type_of(Type)` method
- Add `ctype_of(Type)` convenience method
- Add symbol table: `unordered_map<string, int64_t> symbol_ids_`
- Add dict/set runtime function pointers
- Add `codegen_dict`, `codegen_set` methods

### `src/Codegen.cpp`
- Implement `llvm_type_of(Type)` and `ctype_of(Type)`
- Update `codegen_symbol` to use integer IDs
- Update symbol pattern matching to use `icmp eq`
- Remove `infer_type_from_pattern`, `infer_param_types` (replaced by TypeChecker)
- Implement `codegen_dict`, `codegen_set`
- Update `codegen_print` for new symbol representation
- Emit symbol name table as global constant

### `src/compiled_runtime.c`
- Change `yona_rt_print_symbol` to take `i64` ID + table pointer
- Add set runtime: `set_alloc`, `set_add`, `set_contains`, `set_size`
- Add dict runtime: `dict_alloc`, `dict_get`, `dict_put`, `dict_contains`
- Remove `yona_rt_symbol_eq` (replaced by integer comparison)

### `cli/main.cpp`
- Add TypeChecker pass between parsing and codegen

### `lib/Std/Option.yona`, `lib/Std/Result.yona`
- Change `none = :none` → `none = (:none, 0)` (uniform tuple encoding)
- Change `err error = (:err, error)` to ensure consistent tuple shape
- All pattern matches use tuple patterns, not bare symbol patterns
