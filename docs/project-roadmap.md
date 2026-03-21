# Yona-LLVM Project Roadmap

## Project Status
- **Interpreter**: ✅ Feature-complete with transparent async
- **TypeChecker**: ✅ Hindley-Milner with Promise<T> coercion
- **Parser**: ✅ Newline-aware with juxtaposition
- **Async**: ✅ Parallel let bindings, type-directed auto-await
- **Stdlib**: ✅ 19 native modules, 150+ functions
- **C API**: ✅ Stable embedding interface with native function registration
- **Sandboxing**: ✅ Module whitelist/blacklist, execution/memory limits
- **Test Coverage**: 343 tests, 1425 assertions (100%)

## Completed Work

### Language
- Newline-aware lexer (significant newlines, bracket suppression, semicolons)
- Juxtaposition function application (`f x y`)
- Bare lambda syntax (`\x -> body`), thunks (`\-> expr`)
- Function-style let bindings (`let f x y = body in ...`)
- String interpolation (`"hello {name}"`)
- Non-linear patterns, OR patterns, tuple/list destructuring in let
- Zero-arity function auto-evaluation (strict semantics)
- Symbol value equality, string concatenation with auto-conversion
- Import alias binding (`import M as X`)

### Type System
- Hindley-Milner with polymorphism
- `PromiseType` — `Promise<T>` auto-coerces to `T` in unification
- Type-directed auto-await at operators, function args, conditions, collections

### Runtime
- Tree-walking interpreter with frame-based scoping
- Parallel let bindings via DependencyAnalyzer + thread pool
- Async function support (`is_async` flag, thread pool execution)
- Promise in RuntimeObjectData variant
- Auto-await in binary ops, comparisons, if/case, tuple/list/dict construction

### Standard Library (19 modules)
Math, IO, System, List, Option, Result, Tuple, Range, String, Set, Dict, Timer, Http, Json, Regexp, File, Random, Time, Types

### Infrastructure
- C embedding API (`yona_api.h`) with native function registration
- Sandboxing (module whitelist/blacklist, execution/memory limits)
- Module system (FQN, filesystem resolution, native + file-based)
- 343 test cases, 1425 assertions

## Remaining Work

### Phase 1: LLVM Backend (High Priority)

Yona is statically typed — the compiler generates native code with unboxed primitives.
See [LLVM Backend Plan](llvm-backend-plan.md) for details.

- Pipeline: parse → type check → codegen → LLVM IR → optimize → link
- Unboxed primitives: `Int` → `i64`, `Float` → `double`
- Closures: function pointer + typed environment
- Pattern matching: decision tree codegen
- Async: LLVM coroutines at PromiseType coercion points

### Phase 3: Remaining Stdlib
- Exception utilities, Transducers, Scheduler, eval
- HTTP client with TLS (libcurl/OpenSSL)

### Phase 4: Advanced Concurrency
- STM (TVar, transactions, atomically)
- CSP-style channels
- Auto-parallelization optimizations

## Technical Decisions
- **Transparent async**: Promise<T> in the type system, not exposed to users
- **Newline-aware parsing**: Python-style, replaces the `suppress_juxtaposition_` hack
- **Thread pool for interpreter**: Correct but not optimal; LLVM backend will use coroutines
- **C API over C++**: Stable ABI, works from any FFI (Ruby, Python, Go, etc.)

## References
- [Language Syntax](./language-syntax.md)
- [TODO List](./todo-list.md)
- [LLVM Backend Plan](./llvm-backend-plan.md)
- [C Embedding API](./c-embedding-api.md)
- [Async Implementation](./async-implementation-plan.md)
