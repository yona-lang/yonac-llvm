# TODO List - Yona-LLVM

## Summary
- **Critical Issues**: 0 (all resolved ✅)
- **Test Coverage**: 236 test cases passing (100%), 1098 assertions ✅
- **Interpreter**: Feature-complete for synchronous execution ✅
- **TypeChecker**: Fully implemented ✅
- **Native Stdlib**: Math, IO, System, List, Option, Result, Tuple, Range modules ✅
- **Newline-aware lexer**: Significant newlines, bracket suppression, semicolons ✅
- **Juxtaposition**: Function application by adjacency (`f x y`) ✅
- **String interpolation**: `"hello {name}"` with auto-conversion ✅
- **Async Infrastructure**: Phase 1 complete (core types, thread pool, dependency analyzer) ✅

## Phase 1: Async Interpreter Integration (High Priority)

- [ ] Fix compilation errors in async infrastructure
- [ ] Implement `eval_async` method for async expression evaluation
- [ ] Implement `await_if_promise` for automatic promise unwrapping
- [ ] Implement `visit_parallel_let` for parallel let bindings
- [ ] Implement `call_async` for async function calls
- [ ] Update visitor methods to handle promises transparently
- [ ] Unit tests for promise operations
- [ ] Integration tests for parallel execution

## Phase 2: Embeddability (Medium Priority)

Make Yona usable as an embedded scripting/extension language in host applications.

### 2.1 C API

- [ ] Stable C header with create/destroy/eval/call functions
- [ ] Opaque handle types for interpreter state and runtime objects
- [ ] Error reporting across FFI boundary
- [ ] Shared library build target with C linkage

### 2.2 Sandboxing

- [ ] Configurable module whitelist (restrict which modules user code can import)
- [ ] Disable filesystem/network access in sandboxed mode
- [ ] CPU execution budget (instruction/step counter with configurable limit)
- [ ] Memory allocation budget
- [ ] Hook system for intercepting and logging side effects

### 2.3 Tooling

- [ ] AST-to-Yona pretty printer (round-trip: parse → AST → source)
- [ ] Monaco/CodeMirror language definition (syntax highlighting, bracket matching)
- [ ] REPL improvements

## Phase 3: Standard Library Expansion (Medium Priority)

- [ ] **HTTP Client** — async HTTP operations (get, post, put, delete)
- [ ] **Timer Module** — delays, timeouts, scheduled tasks
- [ ] **String Module** — formatting, manipulation utilities
- [ ] **Set/Dict Modules** — map, filter, fold operations on sets and dicts

## Phase 4: LLVM Backend (Medium Priority)

- [ ] Basic LLVM IR generation from AST
- [ ] Runtime library (promise create/fulfill/await as C functions)
- [ ] Coroutine support for async functions
- [ ] Optimization passes
- [ ] Executable generation

## Phase 5: Advanced Concurrency (Low Priority)

- [ ] STM: TVar, Transaction types, `atomically` primitive, retry/orElse
- [ ] Channel module (CSP-style)
- [ ] Auto-parallelization: static analysis, runtime scheduling, work-stealing
- [ ] Performance: promise pooling, lock-free structures, CPS optimizations

## Completed Work

- ✅ Lexer, Parser — full Yona language support with newline-aware parsing
- ✅ Newline-aware lexer — significant newlines as expression delimiters, suppressed inside brackets and after operators/keywords
- ✅ Juxtaposition function application — `f x y` style calls with proper precedence
- ✅ Bare lambda syntax — `\x -> body` alongside `\(x) -> body`; thunks via `\-> expr`
- ✅ Function-style let bindings — `let f x y = body in ...`
- ✅ Zero-arity function auto-evaluation — strict semantics, thunk escape via `\-> expr`
- ✅ String interpolation — `"hello {name}"`, `"result is {(x + y)}"` with auto-conversion to string
- ✅ Non-linear patterns — same variable must match same value: `(x, x) -> "equal"`
- ✅ Tuple/list pattern destructuring in let — `let (a, b) = expr in ...`
- ✅ Import alias binding — `import Std\IO as IO in IO.println "hello"`
- ✅ Symbol value equality — runtime comparison by name, not pointer
- ✅ String concatenation via `++` operator with auto-conversion
- ✅ OR pattern matching — `:ok | :success -> ...`
- ✅ AST — comprehensive node hierarchy with visitor pattern
- ✅ Interpreter — tree-walking, frame-based, pattern matching, currying
- ✅ TypeChecker — Hindley-Milner with polymorphism
- ✅ Module system — FQN-based, filesystem resolution, caching, native + file-based
- ✅ Native modules — Math, IO, System, List, Option, Result, Tuple, Range
- ✅ Async infrastructure — Promise type, thread pool (standard + work-stealing), AsyncContext, dependency analyzer
- ✅ Record types, field access, generators, exceptions
- ✅ 236 test cases, 1098 assertions passing

## Next Steps

1. Fix async infrastructure compilation errors
2. Implement async interpreter integration (eval_async, await_if_promise, parallel let)
3. Design C embedding API
4. Expand stdlib (HTTP, String, Timer modules)
