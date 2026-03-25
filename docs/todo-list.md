# TODO List - Yona-LLVM

## Summary
- **Test Coverage**: 533 tests, 2249 assertions, 100 codegen fixtures ✅
- **LLVM Compiler**: Feature-complete — type-directed codegen, modules, async, optimizations ✅
- **Interpreter**: Feature-complete with transparent async, lexical closures ✅
- **C Embedding API**: Stable C interface with sandboxing ✅
- **Native Stdlib**: 19 modules, 150+ functions ✅
- **Yona Stdlib**: 6 pure Yona modules in `lib/Std/` (Option, Result, Tuple, Range, List, Test) ✅

## Remaining Work

### Type System & Codegen Architecture (see docs/type-system-plan.md)

Phase 1: Extend Codegen Type System
- [x] Symbol interning: compile symbols to `i64` IDs, integer comparison ✅
- [x] Add `DICT`, `SET` to CType, dict/set construction + runtime ✅
- [x] Propagate element types via `TypedValue::subtypes` for all collections ✅
- [ ] Compile-time type error checking (validate types during codegen, clear error messages)

Phase 2: Typed Collections
- [ ] `Seq<T>` with typed elements (element type from TypeChecker)
- [ ] `Set<T>` construction + runtime (alloc, add, contains, size)
- [ ] `Dict<K,V>` construction + runtime (alloc, get, put, contains)
- [ ] Homogeneous collection enforcement (heterogeneous = type error)

Phase 3: Sum Types
- [ ] Uniform tuple encoding for Option/Result (`(:tag, value)` always a tuple)
- [ ] Pattern matching on symbol-tagged tuples via integer switch
- [ ] Future: proper ADT syntax (`type Option a = Some a | None`)

Phase 4: Cross-Module Monomorphization
- [ ] Whole-program compilation mode (import AST, monomorphize at call site)
- [ ] Future: `.yonai` interface files for separate compilation

Phase 5: Records
- [ ] Named LLVM structs for record types
- [ ] Field access via `extractvalue` / `getelementptr`
- [ ] Record patterns in case expressions

### Tooling
- [ ] Compiler error messages review (make errors clear, actionable, and user-friendly)
- [ ] Side effect hook callback (intercept native calls for auditing)
- [ ] AST-to-Yona pretty printer (round-trip: parse → AST → source)
- [ ] Monaco/CodeMirror language definition (syntax highlighting, bracket matching)
- [ ] REPL improvements (history, completion, multi-line input)

### Testing
- [ ] Http module tests (requires network access)
- [ ] System module edge case tests (exit, setEnv — side effects)

### Codegen Gaps (non-type-system)
- [ ] Large struct return ABI (>16 bytes / 3+ element tuples need sret convention)

### Stdlib Gaps (vs yona-lang.org)
- [ ] Exception utilities, stack traces
- [ ] Transducers (composable reducer transformers)
- [ ] context\Local, Scheduler, eval
- [ ] HTTP client with TLS (libcurl or similar)
- [ ] String interpolation alignment formatting (`{value,10}`)
- [ ] Brace escaping in string literals (`\{`)

### Future Improvements
- [ ] LLVM coroutine intrinsics (replace thread pool for zero-overhead async)
- [ ] Dynamic linking (`.so`/`.dylib`) for hot-reloadable modules
- [ ] Whole-program optimization across module boundaries
- [ ] Incremental compilation
- [ ] STM, CSP channels, advanced concurrency
- [ ] Type classes (dictionary-passing, layers on monomorphization)
- [ ] Algebraic data type syntax (`type Option a = Some a | None`)

## Completed Work

### Language
- Newline-aware lexer (significant newlines, bracket suppression, semicolons)
- Juxtaposition function application (`f x y`)
- Bare lambda syntax (`\x -> body`), thunks (`\-> expr`), function-style let
- String interpolation (`"hello {name}"`)
- Non-linear patterns, OR patterns, tuple/list destructuring in let
- Import aliases, symbol equality, string concatenation with auto-conversion
- `extern` and `extern async` declarations for C FFI
- Zero-arity function auto-evaluation (strict semantics)
- Constructor pattern uppercase convention (lowercase identifiers are variables, not constructors)

### Type System
- Hindley-Milner with polymorphism
- Promise\<T\> auto-coercion in unification
- Type-directed auto-await at all evaluation points

### Interpreter
- Tree-walking with frame-based scoping, pattern matching, currying
- Lexical closures (function bodies execute in defining scope, not call-site scope)
- Transparent async via thread pool + DependencyAnalyzer
- Yona source modules override native C++ modules when available via YONA_PATH
- 19 native stdlib modules (Math, IO, System, List, Option, Result, Tuple, Range, String, Set, Dict, Timer, Http, Json, Regexp, File, Random, Time, Types)

### LLVM Compiler
- Type-directed codegen with TypedValue (CType tags propagate through all expressions)
- Deferred function compilation at call sites with known argument types (monomorphization)
- Lambda lifting for closures, forward declaration for recursion
- Higher-order functions (function pointer passing, indirect calls)
- Partial application (compile-time wrapper generation, zero runtime overhead)
- Case expressions (decision tree: integer, symbol, variable, wildcard, head-tail, empty seq, tuple, or-patterns)
- Tuples (LLVM structs), sequences (runtime heap arrays), symbols (interned strings)
- Module compilation with C-ABI exports, cross-language linking
- Module type metadata (pattern + body-based type inference for parameters and return types)
- Import resolution, native stdlib shims, `extern`/`extern async` for C FFI
- Async codegen: CType::PROMISE, thread pool, auto-await, parallel let
- Optimization passes: tail call elimination, constant folding, GVN, DCE
- 100 E2E codegen test fixtures (including 18 stdlib import tests)

### Yona Stdlib (`lib/Std/`)
- Option module (some, none, isSome, isNone, unwrapOr, map)
- Result module (ok, err, isOk, isErr, unwrapOr, map, mapErr)
- Tuple module (fst, snd, swap, mapBoth, zip, unzip)
- Range module (range, toList, contains, length, take, drop)
- List module (map, filter, fold, foldl, foldr, length, head, tail, reverse, take, drop, flatten, zip, any, all, contains, isEmpty, lookup, splitAt)
- Test module (assertEqual, assertNotEqual, assertTrue, assertFalse, assertGreater, assertLess, assertContains, assertEmpty, suite, run)
- Yona source modules take priority over native C++ when found in YONA_PATH
- Interpreter supports lexical closures and proper module-internal function scoping

### Test Coverage
- Comprehensive stdlib tests: 581 tests, 2486 assertions
- All 19 stdlib modules tested (Math, IO, System, List, Option, Result, Tuple, Range, String, Set, Dict, Timer, Http, Json, Regexp, File, Random, Time, Types)
- Codegen stdlib tests: Math (abs, max, min, factorial, sqrt, sin, cos), String (length, upper, lower), List (len, head, tail, reverse), Types (toInt, toFloat)
- Integration tests: cross-module operations (List+String, Regexp+List, Math combined, List chaining)

### Infrastructure
- C embedding API (`yona_api.h`) with native function registration
- Sandboxing (module whitelist/blacklist, execution/memory limits)
- Compiled runtime library (compiled_runtime.c)
