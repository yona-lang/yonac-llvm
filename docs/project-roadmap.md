# Yona-LLVM Project Roadmap

## Project Status
- **Interpreter**: ✅ Feature-complete
- **TypeChecker**: ✅ Fully implemented
- **Parser**: ✅ Working correctly
- **Native Stdlib**: ✅ Math, IO, System modules
- **Async Infrastructure**: ✅ Phase 1 complete (promises, thread pool, dependency analyzer)
- **Test Coverage**: 210+ test cases passing (100%)

## Roadmap

### Phase 1: Language Completeness (Current)

Complete the async system and fill remaining language gaps.

- **Async interpreter integration** — eval_async, await_if_promise, parallel let bindings, transparent promise unwrapping across all visitor methods
- **Language features** — do-blocks, guards on pattern arms, `in` operator, import syntax
- **Stdlib expansion** — HTTP client, timer, string utilities, collection operations (map, filter, fold)
- **Test framework** — assert, named test cases, `.test.yona` runner, mock support

### Phase 2: Embeddability

Make Yona usable as an embedded language in host applications via FFI.

- **C API** — stable embedding interface with opaque handles and error reporting
- **Sandboxing** — module whitelist, filesystem/network restrictions, CPU/memory budgets
- **Side effect hooks** — intercept and log native module calls
- **Tooling** — AST pretty printer (source round-tripping), editor language definitions

### Phase 3: LLVM Backend

Native code generation for production performance.

- Basic LLVM IR generation from AST
- Coroutine support for async functions
- Runtime library and optimization passes
- Executable generation

### Phase 4: Advanced Concurrency

- STM (TVar, transactions, atomically, retry/orElse)
- CSP-style channels
- Auto-parallelization via static dependency analysis
- Performance optimizations (promise pooling, lock-free structures)

## Completed Components

- **Lexer & Parser**: Recursive descent with Pratt parsing, full language support
- **AST**: Visitor pattern with comprehensive node types
- **Interpreter**: Tree-walking with frame-based execution, pattern matching, currying
- **Type Checker**: Hindley-Milner with polymorphic inference
- **Module System**: FQN-based with filesystem resolution and caching
- **Native Modules**: Math, IO, System via NativeModuleRegistry
- **Async Core**: Promise type, thread pool (standard + work-stealing), dependency analyzer

## Technical Decisions

- **Transparent async**: No explicit async/await keywords — runtime handles parallelization
- **Dependency analysis**: Static analysis determines parallelizable operations
- **STM over locks**: Composable, deadlock-free concurrency (Phase 4)
- **Embeddable by design**: C API + sandboxing enables use as an extension language in host applications

## References
- [Async Implementation Plan](./async-implementation-plan.md)
- [TODO List](./todo-list.md)
