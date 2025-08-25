# Yona-LLVM Project Roadmap

## Project Status
- **Interpreter**: ✅ Feature-complete
- **TypeChecker**: ✅ Fully implemented
- **Parser**: ✅ Working correctly
- **Async Infrastructure**: ✅ Phase 1 Complete
- **Test Coverage**: 48/48 tests passing (100%)

## Current Phase: Async System Phase 2 - Interpreter Integration

### Immediate Tasks
1. Fix compilation errors in async infrastructure
2. Implement async evaluation methods in Interpreter
3. Create test suite for async operations
4. Begin STM implementation

## Roadmap

### Phase 2: Interpreter Async Integration (Current)
**Timeline**: Month 2

- [ ] Implement `eval_async` method for async expression evaluation
- [ ] Implement `await_if_promise` for automatic promise unwrapping
- [ ] Implement `visit_parallel_let` for parallel let bindings
- [ ] Implement `call_async` for async function calls
- [ ] Update visitor methods to handle promises transparently

### Phase 3: STM Implementation
**Timeline**: Month 3

- [ ] Implement TVar and Transaction types
- [ ] Create STM runtime with versioned memory
- [ ] Add `atomically` primitive
- [ ] Implement retry and orElse combinators
- [ ] Create STM standard library module

### Phase 4: Async Standard Library
**Timeline**: Month 4

#### Core Modules
- [ ] **Async** - async, await, parallel, timeout operations
- [ ] **IO** - Async file operations
- [ ] **HTTP** - Client and server with async handlers
- [ ] **Socket** - TCP/UDP async operations
- [ ] **Timer** - Delays and scheduled tasks
- [ ] **Channel** - CSP-style communication

### Phase 5: LLVM Backend
**Timeline**: Month 5

- [ ] Basic LLVM IR generation from AST
- [ ] Coroutine support for async functions
- [ ] Runtime library for async support
- [ ] Optimization passes
- [ ] Executable generation

### Phase 6: Auto-Parallelization
**Timeline**: Month 6

- [ ] Static analysis for parallel opportunities
- [ ] Runtime scheduling optimizations
- [ ] Work-stealing integration
- [ ] Performance profiling and tuning

## Architecture Overview

### Completed Components
- **Lexer & Parser**: Full Yona language support
- **AST**: Comprehensive node hierarchy with visitor pattern
- **Interpreter**: Tree-walking with pattern matching
- **Type System**: Hindley-Milner with extensions
- **Module System**: Import/export with FQN support
- **Async Core**: Promises, thread pool, dependency analysis

### In Development
- **Async Integration**: Transparent promise handling in interpreter
- **STM**: Software transactional memory for safe concurrency
- **Standard Library**: Async-first module ecosystem

### Future Work
- **LLVM Backend**: Native code generation
- **Optimizations**: Auto-parallelization, inlining, fusion
- **Tooling**: REPL enhancements, debugger, profiler

## Technical Decisions

### Async System Design
- **Transparent Promises**: No explicit async/await keywords
- **Automatic Parallelization**: Dependency analysis for parallel execution
- **STM over Locks**: Composable, deadlock-free concurrency
- **Work-Stealing**: Efficient load balancing across threads

### Implementation Strategy
1. Core infrastructure first (✅ Complete)
2. Interpreter integration for testing
3. STM for safe concurrent state
4. Standard library for real-world usage
5. LLVM backend for production performance
6. Optimizations based on profiling data

## Success Metrics
- All async operations transparent to user code
- Parallel let bindings show measurable speedup
- STM transactions compose without deadlocks
- Standard library modules cover common use cases
- LLVM backend generates efficient native code

## References
- [Yona Execution Model](https://yona-lang.org/about/#execution-model)
- [Async Implementation Plan](./async-implementation-plan.md)
- [TODO List](./todo-list.md)
