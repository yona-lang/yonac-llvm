# TODO List - Yona-LLVM

## Summary
- **Critical Issues**: 0 (all resolved ✅)
- **Test Coverage**: 343 test cases passing (100%), 1425 assertions ✅
- **C Embedding API**: Stable C interface (`yona_api.h`) for FFI ✅
- **Sandboxing**: Module whitelist/blacklist, execution limits, memory limits ✅
- **Interpreter**: Feature-complete with transparent async ✅
- **TypeChecker**: Fully implemented with Promise<T> coercion ✅
- **Native Stdlib**: 19 modules, 150+ functions ✅
- **Newline-aware lexer**: Significant newlines, bracket suppression, semicolons ✅
- **Juxtaposition**: Function application by adjacency (`f x y`) ✅
- **String interpolation**: `"hello {name}"` with auto-conversion ✅
- **Async Integration**: Promise-aware type system, parallel let bindings, type-directed auto-await ✅

## Phase 1: Embeddability (High Priority)

Make Yona usable as an embedded scripting/extension language in host applications.

### 1.1 C API

- [ ] Stable C header (`yona.h`) with create/destroy/eval/call functions
- [ ] Opaque handle types for interpreter state and runtime objects
- [ ] Error reporting across FFI boundary (error codes + message retrieval)
- [ ] Callback registration for host-provided native functions
- [ ] Shared library build target with C linkage

### 1.2 Sandboxing (mostly complete ✅)

- [x] Module whitelist/blacklist with wildcard patterns
- [x] Execution step limits (resets per eval)
- [x] Memory allocation tracking and limits
- [ ] Side effect hook callback (intercept native calls for auditing)

### 1.3 Tooling

- [ ] AST-to-Yona pretty printer (round-trip: parse → AST → source)
- [ ] Monaco/CodeMirror language definition (syntax highlighting, bracket matching)
- [ ] REPL improvements (history, completion, multi-line input)

## Phase 2: LLVM Backend (High Priority)

Yona is statically typed (HM inference). The compiler generates native code with unboxed
primitives (`Int` → `i64`, `Float` → `double`). See [LLVM Backend Plan](llvm-backend-plan.md).

### Phase 2.1: Pipeline + Arithmetic
- [ ] Wire `yonac` CLI: parse → type check → codegen → emit → link
- [ ] Codegen for literals (Int, Float, Bool, String)
- [ ] Codegen for arithmetic and comparison operators (native instructions)
- [ ] Codegen for let bindings and if-then-else
- [ ] Runtime library: string allocation, print, entry point

### Phase 2.2: Functions
- [ ] Lambda → closure allocation (function pointer + typed environment)
- [ ] Free variable analysis for closure capture
- [ ] Function application (direct call or closure call)
- [ ] Partial application / currying

### Phase 2.3: Collections + Pattern Matching
- [ ] Tuple → LLVM struct, Seq/Dict/Set → runtime allocation
- [ ] Case expression → decision tree codegen
- [ ] Pattern types: literal, variable, tuple, seq, head-tail, or-pattern

### Phase 2.4: Modules
- [ ] Compile modules to object files, link-time import resolution
- [ ] Native module FFI (call existing C++ stdlib)

### Phase 2.5: Coroutines (Async)
- [ ] LLVM coroutine intrinsics for async functions
- [ ] Auto-await at PromiseType coercion points
- [ ] Parallel let via coroutine spawning
- [ ] Event loop scheduler

### Phase 2.6: Optimization
- [ ] Tail call optimization (`musttail`)
- [ ] Function inlining, escape analysis
- [ ] Type specialization (monomorphize polymorphic functions)

## Phase 3: Remaining Stdlib (Medium Priority)

### Missing modules (vs yona-lang.org stdlib)

- [ ] **Exception** — exception utilities, stack traces
- [ ] **Transducers** — composable reducer transformers (`reduce` support for List/Set/Dict)
- [ ] **context\Local** — custom context manager support
- [ ] **Scheduler** — task scheduling
- [ ] **eval** — dynamic expression evaluation from string

### HTTP Client (proper implementation)

The current `Std\Http` is a minimal POSIX socket implementation (HTTP only, no TLS).
Full implementation requires an HTTP library (libcurl or similar):

- [ ] Session-based API with configuration (authenticator, redirects, body encoding)
- [ ] Full HTTP methods: GET, POST, PUT, DELETE with custom headers
- [ ] Response as `(status_code, headers_dict, body)` tuple
- [ ] TLS/HTTPS support
- [ ] Binary vs text body encoding
- [ ] Non-blocking I/O in compiled mode (via LLVM coroutines)

### Existing module gaps

- [ ] `Std\List` — `encode`/`decode` (UTF-8 byte conversion)
- [ ] `Std\String` — alignment formatting in interpolation (`{value,10}`)
- [ ] String interpolation — `{` in string literals needs escape syntax (`\{`)

### Not applicable (Java-specific in original)

- `java\Types`, `Java` — Java interop (original runs on GraalVM)
- `http\Server` — Java HTTP server
- `socket\tcp\*` — TCP socket modules (defer to LLVM backend)

## Phase 4: Advanced Concurrency (Low Priority)

- [ ] STM: TVar, Transaction types, `atomically` primitive, retry/orElse
- [ ] Channel module (CSP-style communication)
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
- ✅ Type-directed auto-await — binary ops, comparisons, function args, if/case/tuple/list/dict auto-await promises
- ✅ Async function support — `is_async` flag on FunctionValue, thread pool execution, Promise return
- ✅ Parallel let bindings — DependencyAnalyzer identifies independent bindings, evaluates in thread pool
- ✅ AST — comprehensive node hierarchy with visitor pattern
- ✅ Interpreter — tree-walking, frame-based, pattern matching, currying, transparent async
- ✅ TypeChecker — Hindley-Milner with polymorphism and Promise coercion
- ✅ Module system — FQN-based, filesystem resolution, caching, native + file-based
- ✅ Native modules (19): Math, IO, System, List, Option, Result, Tuple, Range, String, Set, Dict, Timer, Http, Json, Regexp, File, Random, Time, Types
- ✅ Async infrastructure — Promise type, thread pool (standard + work-stealing), AsyncContext, dependency analyzer
- ✅ C embedding API — `yona_api.h` with eval, value creation/inspection, function calling, native registration, module access
- ✅ Sandboxing — module whitelist/blacklist with wildcards, execution step limits, memory limits
- ✅ Record types, field access, generators, exceptions
- ✅ 343 test cases, 1425 assertions passing

## Next Steps

1. Begin LLVM IR generation (Phase 2.1: pipeline + arithmetic)
2. Implement closures and function application codegen
3. Add proper HTTP client with TLS (libcurl or OpenSSL)
