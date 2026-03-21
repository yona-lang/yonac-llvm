# TODO List - Yona-LLVM

## Summary
- **Critical Issues**: 0 (all resolved ✅)
- **Test Coverage**: 319 test cases passing (100%), 1350 assertions ✅
- **Interpreter**: Feature-complete with transparent async ✅
- **TypeChecker**: Fully implemented with Promise<T> coercion ✅
- **Native Stdlib**: Math, IO, System, List, Option, Result, Tuple, Range modules ✅
- **Newline-aware lexer**: Significant newlines, bracket suppression, semicolons ✅
- **Juxtaposition**: Function application by adjacency (`f x y`) ✅
- **String interpolation**: `"hello {name}"` with auto-conversion ✅
- **Async Integration**: Promise-aware type system, parallel let bindings, type-directed auto-await ✅

## Phase 1: Embeddability (High Priority)

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

### Missing modules (vs yona-lang.org stdlib)

- [ ] **Exception** — exception utilities, stack traces (original: `Exception` module)
- [ ] **Transducers** — composable reducer transformers (original: `Transducers` module)
- [ ] **context\Local** — custom context manager support (original: `context\Local`)
- [ ] **Scheduler** — task scheduling (original: `Scheduler`)
- [ ] **eval** — dynamic expression evaluation from string

### HTTP Client improvements

The current `Std\Http` module is a minimal POSIX socket implementation (HTTP only, no TLS).
The original Yona HTTP client (`http\Client`) supports:

- [ ] Session-based API with configuration (authenticator, redirects, body encoding)
- [ ] Full HTTP methods: GET, POST, PUT, DELETE with headers
- [ ] Response as `(status_code, headers_dict, body)` tuple
- [ ] TLS/HTTPS support (requires OpenSSL or similar)
- [ ] Binary vs text body encoding

### Existing module gaps (vs yona-lang.org)

- [ ] `Std\List` — missing: `encode`/`decode` (UTF-8 byte conversion), `reduce` (transducer support)
- [ ] `Std\Set` — missing: `reduce` (transducer support)
- [ ] `Std\Dict` — missing: `reduce` (transducer support), `entries` (have `toList`)

### Not applicable (Java-specific in original)

- `java\Types`, `Java` — Java interop (original runs on GraalVM)
- `http\Server` — Java HTTP server
- `socket\tcp\*` — TCP socket modules (defer to LLVM backend with proper async runtime)
- `terminal\Colors` — terminal color codes (low priority)

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
- ✅ Promise-aware type system — `PromiseType` in types.h, auto-coercion in unification (`Promise<T>` → `T`)
- ✅ Type-directed auto-await — binary ops, comparisons, function args, if/case conditions auto-await promises
- ✅ Async function support — `is_async` flag on FunctionValue, async functions submit to thread pool and return Promise
- ✅ Parallel let bindings — DependencyAnalyzer identifies independent bindings, evaluates in thread pool
- ✅ AST — comprehensive node hierarchy with visitor pattern
- ✅ Interpreter — tree-walking, frame-based, pattern matching, currying, transparent async
- ✅ TypeChecker — Hindley-Milner with polymorphism
- ✅ Module system — FQN-based, filesystem resolution, caching, native + file-based
- ✅ Native modules — Math, IO, System, List, Option, Result, Tuple, Range, String, Set, Dict, Timer, Http, Json, Regexp, File, Random, Time, Types
- ✅ Async infrastructure — Promise type, thread pool (standard + work-stealing), AsyncContext, dependency analyzer
- ✅ Record types, field access, generators, exceptions
- ✅ 319 test cases, 1350 assertions passing

## Next Steps

1. Fix async infrastructure compilation errors
2. Implement async interpreter integration (eval_async, await_if_promise, parallel let)
3. Design C embedding API
4. Expand stdlib (HTTP, String, Timer modules)
