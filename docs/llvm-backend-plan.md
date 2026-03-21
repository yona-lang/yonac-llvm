# LLVM Backend Implementation Plan

## Overview

Yona is **statically typed** with Hindley-Milner type inference. After type checking, every expression has a known type at compile time. This means the LLVM backend can generate native code without runtime type dispatch — `Int` values are `i64`, `Float` values are `double`, and `a + b` where both are `Int` compiles to a single `add i64` instruction.

This is the GHC/OCaml compilation model, not the CPython/Ruby model.

## Architecture

```
Yona Source → Lexer → Parser → AST → TypeChecker → Codegen → LLVM IR
                                          ↓                      ↓
                                   Type annotations      LLVM Opt Passes
                                   on each node                ↓
                                                        Object Code
                                                              ↓
                                                    Linker + Runtime Lib
                                                              ↓
                                                         Executable
```

The Codegen class is `AstVisitor<llvm::Value*>` — each AST node produces an LLVM value. The type checker's annotations tell the codegen what LLVM types to use.

## Type Mapping

| Yona Type | LLVM Type | Notes |
|-----------|-----------|-------|
| `Int` (SignedInt64) | `i64` | Native integer arithmetic |
| `Float` (Float64) | `double` | Native float arithmetic |
| `Bool` | `i1` | Single bit |
| `Byte` | `i8` | |
| `Char` | `i32` | Unicode codepoint |
| `Unit` | `void` or `{}` | |
| `String` | `%String*` | Pointer to runtime string struct |
| `Symbol` | `%Symbol*` | Interned string pointer |
| `(A, B, C)` | `{ A_llvm, B_llvm, C_llvm }` | Struct with typed fields |
| `[T]` (Seq) | `%Seq*` | Pointer to runtime array of T |
| `{T}` (Set) | `%Set*` | Pointer to runtime set |
| `{K: V}` (Dict) | `%Dict*` | Pointer to runtime hash map |
| `A -> B` | `{ %Fn*, %Env* }` | Closure: function pointer + environment |
| `Promise<T>` | coroutine handle | LLVM coroutine returning T |

Primitives (`Int`, `Float`, `Bool`, `Byte`, `Char`) are **unboxed** — no heap allocation. Collections and strings are heap-allocated and managed by the runtime.

## Codegen Examples

### Arithmetic

```yona
1 + 2 * 3
```

Type checker infers: all `Int`. Codegen:

```llvm
%0 = mul i64 2, 3
%1 = add i64 1, %0
```

No runtime calls. Pure native instructions.

### Let Bindings

```yona
let x = 10, y = 20 in x + y
```

```llvm
%x = i64 10
%y = i64 20
%result = add i64 %x, %y
```

### If Expression

```yona
if x > 0 then x else -x
```

```llvm
%cond = icmp sgt i64 %x, 0
br i1 %cond, label %then, label %else

then:
  br label %merge

else:
  %neg = sub i64 0, %x
  br label %merge

merge:
  %result = phi i64 [%x, %then], [%neg, %else]
```

### Functions and Closures

```yona
let add = \x y -> x + y in add 3 4
```

Type: `Int -> Int -> Int`. Since no free variables, compiles to a direct function:

```llvm
define i64 @add(i64 %x, i64 %y) {
  %result = add i64 %x, %y
  ret i64 %result
}

; call site
%result = call i64 @add(i64 3, i64 4)
```

When closures capture free variables:

```yona
let y = 10 in \x -> x + y
```

```llvm
; Closure environment
%env = type { i64 }  ; captured y

; Closure function (receives environment as first arg)
define i64 @lambda(%env* %env, i64 %x) {
  %y = getelementptr %env, %env* %env, i32 0, i32 0
  %y_val = load i64, i64* %y
  %result = add i64 %x, %y_val
  ret i64 %result
}
```

### Pattern Matching

```yona
case x of
  0 -> "zero"
  n | n > 0 -> "positive"
  _ -> "negative"
end
```

Compiles to a decision tree:

```llvm
  %is_zero = icmp eq i64 %x, 0
  br i1 %is_zero, label %case_0, label %check_pos

check_pos:
  %is_pos = icmp sgt i64 %x, 0
  br i1 %is_pos, label %case_pos, label %case_default

case_0:
  ; return "zero"
case_pos:
  ; return "positive"
case_default:
  ; return "negative"
```

### Transparent Async (Coroutines)

```yona
let content = readFile "data.txt" in
length content
```

`readFile` has type `String -> Promise<String>`. The type checker marks the coercion point at `length content` (where `Promise<String>` meets `String`).

```llvm
; readFile is a coroutine
%promise = call %coro_handle @readFile(%String* @"data.txt")

; Auto-await at the coercion point
%content = call %String* @yona_coro_await(%coro_handle %promise)

; length is a direct call (String -> Int)
%result = call i64 @string_length(%String* %content)
```

For parallel let bindings:

```yona
let a = readFile "x", b = readFile "y" in a ++ b
```

```llvm
; Spawn both coroutines
%coro_a = call %coro_handle @readFile(%String* @"x")
%coro_b = call %coro_handle @readFile(%String* @"y")

; Await both (scheduler runs them concurrently)
%a = call %String* @yona_coro_await(%coro_handle %coro_a)
%b = call %String* @yona_coro_await(%coro_handle %coro_b)

; Concatenate
%result = call %String* @string_concat(%String* %a, %String* %b)
```

## Runtime Library

Even with static types, some operations need runtime support:

```c
// yona_runtime.h — linked with compiled code

// String operations
yona_string_t* yona_rt_string_alloc(const char* data, size_t len);
yona_string_t* yona_rt_string_concat(yona_string_t* a, yona_string_t* b);
size_t         yona_rt_string_length(yona_string_t* s);

// Collection operations
yona_seq_t*    yona_rt_seq_alloc(size_t elem_size, size_t count);
void           yona_rt_seq_set(yona_seq_t* seq, size_t index, void* elem);
void*          yona_rt_seq_get(yona_seq_t* seq, size_t index);

// Coroutine scheduling
yona_coro_t    yona_rt_coro_spawn(void* coro_frame);
void*          yona_rt_coro_await(yona_coro_t handle);
void           yona_rt_coro_yield(yona_coro_t handle, void* value);

// Memory management
void*          yona_rt_alloc(size_t bytes);
void           yona_rt_free(void* ptr);

// Module loading
void*          yona_rt_load_module(const char* fqn);
void*          yona_rt_module_get(void* module, const char* name);

// Exceptions
void           yona_rt_raise(const char* type, const char* message);
void*          yona_rt_try(void* (*body)(void*), void* (*handler)(void*, void*), void* ctx);
```

Note: arithmetic, comparison, boolean logic — all compile to native LLVM instructions. No runtime calls needed.

## Implementation Phases

### Phase 1: Pipeline + Arithmetic ✅

**Goal**: `let x = 1 + 2 * 3 in x` compiles to a native executable.

- Wire up `yonac` CLI: parse → type check → codegen → emit object → link
- Implement Codegen for: `IntegerExpr`, `FloatExpr`, `BoolExpr`, `StringExpr`
- Implement: `AddExpr`, `SubtractExpr`, `MultiplyExpr`, `DivideExpr`
- Implement: `EqExpr`, `LtExpr`, `GtExpr`, comparison operators
- Implement: `LetExpr` (simple value bindings)
- Implement: `IfExpr`
- Runtime library: string allocation, print, program entry point
- Emit `main()` that calls the compiled expression and prints the result

### Phase 2: Functions ✅

- Lambda expressions → closure allocation
- Free variable analysis (identify captured variables)
- Function application → direct call (no free vars) or closure call
- Currying / partial application → runtime helper or codegen'd wrapper
- Recursive functions → named function definitions

### Phase 3: Pattern Matching + Do Blocks ✅ (partial)

- Tuple construction → LLVM struct
- Sequence construction → runtime array allocation
- Dict/set → runtime hash map
- Case expression → decision tree compilation
- Pattern types: literal, variable, wildcard, tuple, sequence, head-tail, or-pattern

### Phase 4: Module System

- Compile modules to object files
- Module exports as symbol table
- Import → link-time resolution
- Native module FFI (call into existing C++ stdlib)

### Phase 5: Coroutines (Async)

- LLVM coroutine intrinsics (`@llvm.coro.id`, `@llvm.coro.begin`, etc.)
- Async function → coroutine frame
- Auto-await → coroutine resume
- Parallel let → spawn multiple coroutines, await all
- Event loop scheduler (replaces thread pool)

### Phase 6: Optimization

- Tail call optimization (`musttail` attribute)
- Function inlining
- Escape analysis (stack-allocate closures that don't escape)
- Specialization (monomorphize polymorphic functions when possible)
- Dead code elimination, constant folding (LLVM handles these automatically)

## Existing Infrastructure

- `include/Codegen.h` — stub class definition
- `src/Codegen.cpp` — stub implementation
- LLVM 16+ linked via CMake (`llvm_map_components_to_libnames`)
- `yonac` CLI executable exists and links `yona_lib`
- TypeChecker annotates AST nodes with types
- `PromiseType` in type system identifies async coercion points
- `is_async` flag on `FunctionValue` identifies async functions

## Key Design Decisions

1. **Unboxed primitives**: `Int` is `i64`, not a heap object. This is possible because types are known statically.

2. **Closures as {fn_ptr, env_ptr}**: Standard closure representation. Environment is a typed struct, not a generic map.

3. **Coroutines for async**: LLVM's coroutine intrinsics map directly to Yona's transparent async model. The type system's `PromiseType` coercion points become coroutine suspend/resume points.

4. **Shared runtime with interpreter**: The runtime library uses the same `RuntimeObject` for interop with the interpreter (e.g., calling native modules). Pure compiled code uses unboxed types.

5. **Incremental compilation**: Start with `yonac` compiling simple expressions. Expand coverage over time. The interpreter remains the development/REPL runtime.
