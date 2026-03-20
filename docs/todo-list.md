# TODO List - Yona-LLVM

## Summary
- **Critical Issues**: 0 (all resolved ✅)
- **Test Coverage**: 210+ test cases passing (100%)
- **Interpreter**: Feature-complete ✅
- **TypeChecker**: Fully implemented ✅
- **Native Stdlib**: Math, IO, System modules ✅
- **Async Infrastructure**: Phase 1 complete (core types, thread pool, dependency analyzer) ✅

## Phase 1: Language Completeness (High Priority)

### 1.1 Async Interpreter Integration

- [ ] Fix compilation errors in async infrastructure
- [ ] Implement `eval_async` method for async expression evaluation
- [ ] Implement `await_if_promise` for automatic promise unwrapping
- [ ] Implement `visit_parallel_let` for parallel let bindings
- [ ] Implement `call_async` for async function calls
- [ ] Update visitor methods to handle promises transparently
- [ ] Unit tests for promise operations
- [ ] Integration tests for parallel execution

### 1.2 Language Features

- [ ] `do...end` blocks (sequenced expressions with implicit last-value return)
- [ ] `import X from Module` syntax (if not already supported)
- [ ] `in` operator for collection membership (`p in [:high, :critical]`)
- [ ] Guard expressions on pattern match arms (`| h < 4`)

### 1.3 Standard Library Expansion

- [ ] **HTTP Client** — async HTTP operations (get, post, put, delete)
- [ ] **Timer Module** — delays, timeouts, scheduled tasks
- [ ] **String Module** — formatting, interpolation, manipulation utilities
- [ ] **Seq/Set/Dict Modules** — map, filter, fold, zip, etc.

### 1.4 Test Framework

- [ ] `assert` as a language built-in or stdlib function
- [ ] `test "description" = do ... end` syntax for named test cases
- [ ] `.test.yona` file runner (discover and execute test files)
- [ ] Mock/stub support for native modules
- [ ] CI integration (exit code, structured output)

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

## Phase 3: LLVM Backend (Medium Priority)

- [ ] Basic LLVM IR generation from AST
- [ ] Runtime library (promise create/fulfill/await as C functions)
- [ ] Coroutine support for async functions
- [ ] Optimization passes
- [ ] Executable generation

## Phase 4: Advanced Concurrency (Low Priority)

- [ ] STM: TVar, Transaction types, `atomically` primitive, retry/orElse
- [ ] Channel module (CSP-style)
- [ ] Auto-parallelization: static analysis, runtime scheduling, work-stealing
- [ ] Performance: promise pooling, lock-free structures, CPS optimizations

## Completed Work

- ✅ Lexer, Parser — full Yona language support
- ✅ AST — comprehensive node hierarchy with visitor pattern
- ✅ Interpreter — tree-walking, frame-based, pattern matching, currying
- ✅ TypeChecker — Hindley-Milner with polymorphism
- ✅ Module system — FQN-based, filesystem resolution, caching
- ✅ Native modules — Math, IO, System via NativeModuleRegistry
- ✅ Async infrastructure — Promise type, thread pool (standard + work-stealing), AsyncContext, dependency analyzer
- ✅ Record types, field access, generators, exceptions
- ✅ 210+ test cases passing

## Next Steps

1. Fix async infrastructure compilation errors
2. Implement async interpreter integration (eval_async, await_if_promise, parallel let)
3. Add missing language features (do-blocks, guards, `in` operator)
4. Expand stdlib with collection operations and HTTP client
5. Build test framework for Yona programs
