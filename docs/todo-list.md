# TODO List - Yona-LLVM

## Summary
- **Test Coverage**: 353 tests, 1625 assertions, 86 codegen fixtures ✅
- **LLVM Compiler**: Feature-complete — type-directed codegen, modules, async, optimizations ✅
- **Interpreter**: Feature-complete with transparent async ✅
- **C Embedding API**: Stable C interface with sandboxing ✅
- **Native Stdlib**: 19 modules, 150+ functions ✅

## Remaining Work

### Tooling
- [ ] Side effect hook callback (intercept native calls for auditing)
- [ ] AST-to-Yona pretty printer (round-trip: parse → AST → source)
- [ ] Monaco/CodeMirror language definition (syntax highlighting, bracket matching)
- [ ] REPL improvements (history, completion, multi-line input)

### Codegen Gaps
- [ ] Dict/Set construction in codegen
- [ ] Module type metadata (infer import function types from source — currently defaults to i64)
- [ ] Monomorphization of polymorphic functions at import site
- [ ] Remaining native stdlib in compiled runtime (map, filter, fold — need closure calling convention in C)

### Stdlib Gaps (vs yona-lang.org)
- [ ] Exception utilities, stack traces
- [ ] Transducers (composable reducer transformers)
- [ ] context\Local, Scheduler, eval
- [ ] HTTP client with TLS (libcurl or similar)
- [ ] String interpolation alignment formatting (`{value,10}`)
- [ ] Brace escaping in string literals (`\{`)

### Future Improvements
- [ ] LLVM coroutine intrinsics (replace thread pool for zero-overhead async)
- [ ] `.yonai` interface files (pre-compiled module metadata)
- [ ] Dynamic linking (`.so`/`.dylib`) for hot-reloadable modules
- [ ] Whole-program optimization across module boundaries
- [ ] Incremental compilation
- [ ] STM, CSP channels, advanced concurrency

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

### Type System
- Hindley-Milner with polymorphism
- Promise\<T\> auto-coercion in unification
- Type-directed auto-await at all evaluation points

### Interpreter
- Tree-walking with frame-based scoping, pattern matching, currying
- Transparent async via thread pool + DependencyAnalyzer
- 19 native stdlib modules (Math, IO, System, List, Option, Result, Tuple, Range, String, Set, Dict, Timer, Http, Json, Regexp, File, Random, Time, Types)

### LLVM Compiler
- Type-directed codegen with TypedValue (CType tags propagate through all expressions)
- Deferred function compilation at call sites with known argument types
- Lambda lifting for closures, forward declaration for recursion
- Higher-order functions (function pointer passing, indirect calls)
- Partial application (compile-time wrapper generation, zero runtime overhead)
- Case expressions (decision tree: integer, symbol, variable, wildcard, head-tail, empty seq, tuple, or-patterns)
- Tuples (LLVM structs), sequences (runtime heap arrays), symbols (interned strings)
- Module compilation with C-ABI exports, cross-language linking
- Import resolution, native stdlib shims, `extern`/`extern async` for C FFI
- Async codegen: CType::PROMISE, thread pool, auto-await, parallel let
- Optimization passes: tail call elimination, constant folding, GVN, DCE
- 86 E2E codegen test fixtures

### Infrastructure
- C embedding API (`yona_api.h`) with native function registration
- Sandboxing (module whitelist/blacklist, execution/memory limits)
- Compiled runtime library (compiled_runtime.c)
