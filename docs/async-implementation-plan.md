# Yona Async System Implementation Plan

## Status: Phase 1 Complete (Core Infrastructure)

## Overview
Yona's execution model provides transparent async/await without explicit syntax, automatic parallelization of independent operations, and STM for advanced concurrency. This plan outlines the implementation for both interpreter and LLVM compiler.

## Completed Components (Phase 1)

### 1. Promise Runtime Type (`include/runtime_async.h`, `src/runtime_async.cpp`)
- ✅ Promise state management (PENDING, FULFILLED, REJECTED)
- ✅ Fulfill/reject operations with thread safety
- ✅ Blocking await and non-blocking try_get
- ✅ Callback registration and execution
- ✅ Promise pipelining support

### 2. Thread Pool (`include/thread_pool.h`, `src/thread_pool.cpp`)
- ✅ Standard thread pool with configurable worker threads
- ✅ Work-stealing thread pool for load balancing
- ✅ Task submission with futures
- ✅ Graceful shutdown and wait operations

### 3. AsyncContext (`include/runtime_async.h`, `src/runtime_async.cpp`)
- ✅ Global async context singleton
- ✅ Promise registry and lifecycle management
- ✅ Async task submission
- ✅ Parallel task execution
- ✅ Integration with InterpreterState

### 4. Dependency Analyzer (`include/dependency_analyzer.h`, `src/dependency_analyzer.cpp`)
- ✅ AST analysis for variable dependencies
- ✅ Parallel group identification
- ✅ Topological sorting for execution order
- ✅ Side effect detection
- ✅ Async operation detection

## Core Concepts

### 1. Promise Type
Yona uses transparent promises that are automatically unwrapped by the runtime. Unlike JavaScript or other languages, developers don't explicitly handle promises.

### 2. Dependency Analysis
The runtime analyzes expression dependencies to determine what can run in parallel.

### 3. Promise Pipelining
Operations on promises are queued and executed when the promise resolves, without blocking.

## Phase 1: Core Runtime Infrastructure

### 1.1 Promise Runtime Type
```cpp
// include/runtime_async.h
namespace yona::runtime::async {

enum class PromiseState {
    PENDING,
    FULFILLED,
    REJECTED
};

struct Promise : public RuntimeObject {
    PromiseState state;
    RuntimeObjectPtr value;
    RuntimeObjectPtr error;
    vector<function<void(RuntimeObjectPtr)>> callbacks;
    mutex state_mutex;
    condition_variable cv;

    // Promise pipelining support
    queue<function<RuntimeObjectPtr(RuntimeObjectPtr)>> pipeline;

    void fulfill(RuntimeObjectPtr val);
    void reject(RuntimeObjectPtr err);
    RuntimeObjectPtr await();  // Blocking wait
    void then(function<void(RuntimeObjectPtr)> callback);
};

struct AsyncContext {
    ThreadPool* executor;
    unordered_map<size_t, shared_ptr<Promise>> active_promises;
    atomic<size_t> next_promise_id;
};

}
```

### 1.2 Thread Pool Implementation
```cpp
// include/thread_pool.h
class ThreadPool {
    vector<thread> workers;
    queue<function<void()>> tasks;
    mutex queue_mutex;
    condition_variable cv;
    atomic<bool> stop;

public:
    void submit(function<void()> task);
    future<RuntimeObjectPtr> submit_async(function<RuntimeObjectPtr()> task);
};
```

### 1.3 Dependency Graph Builder
```cpp
// include/dependency_analyzer.h
class DependencyAnalyzer {
public:
    struct Node {
        AstNode* expr;
        set<string> reads;   // Variables read
        set<string> writes;  // Variables written
        vector<Node*> dependencies;
    };

    Graph analyze(AstNode* expr);
    vector<vector<Node*>> get_parallel_groups(Graph& g);
};
```

## Phase 2: Interpreter Integration

### 2.1 Async Interpreter State
```cpp
// Extend InterpreterState in Interpreter.h
struct InterpreterState {
    // ... existing members ...

    // Async execution context
    shared_ptr<AsyncContext> async_ctx;
    bool is_async_context = false;

    // Current promise if executing async
    shared_ptr<Promise> current_promise;
};
```

### 2.2 Async Expression Evaluation
```cpp
// New methods in Interpreter
class Interpreter {
    // Async evaluation methods
    shared_ptr<Promise> eval_async(AstNode* node);
    RuntimeObjectPtr await_if_promise(RuntimeObjectPtr obj);

    // Parallel let binding evaluation
    InterpreterResult visit_parallel_let(LetExpr* node);

    // Async function calls
    InterpreterResult call_async(FunctionValue* func, vector<RuntimeObjectPtr> args);
};
```

### 2.3 Automatic Promise Unwrapping
```cpp
// Modify all visitor methods to handle promises
InterpreterResult Interpreter::visit(AddExpr* node) const {
    auto left = node->left->accept(*this).value;
    auto right = node->right->accept(*this).value;

    // Automatic unwrapping
    left = await_if_promise(left);
    right = await_if_promise(right);

    // Perform addition
    return add_values(left, right);
}
```

## Phase 3: STM Implementation

### 3.1 STM Runtime Types
```cpp
// include/stm.h
namespace yona::runtime::stm {

struct TVar {
    RuntimeObjectPtr value;
    version_t version;
    mutex lock;
};

struct Transaction {
    unordered_map<TVar*, RuntimeObjectPtr> read_set;
    unordered_map<TVar*, RuntimeObjectPtr> write_set;
    version_t start_version;

    RuntimeObjectPtr read(TVar* tvar);
    void write(TVar* tvar, RuntimeObjectPtr val);
    bool commit();
    void abort();
};

class STM {
    atomic<version_t> global_version;

public:
    shared_ptr<TVar> new_tvar(RuntimeObjectPtr initial);
    RuntimeObjectPtr atomically(function<RuntimeObjectPtr(Transaction&)> f);
};

}
```

### 3.2 STM Language Integration
```yona
# STM module interface
module STM exports new_tvar, read_tvar, write_tvar, atomically as

# Create a new transactional variable
new_tvar value = <native>

# Read from TVar
read_tvar tvar = <native>

# Write to TVar (only in transaction)
write_tvar tvar value = <native>

# Run atomic transaction
atomically action = <native>

end
```

## Phase 4: Standard Library Async Modules

### 4.1 Async Module
```yona
module Async exports async, await, parallel, timeout as

# Run function asynchronously
async f = <native>

# Explicitly await a promise (usually automatic)
await promise = <native>

# Run functions in parallel
parallel functions = <native>

# Add timeout to async operation
timeout duration promise = <native>

end
```

### 4.2 IO Module Enhancement
```cpp
// Modify existing IO operations to return promises
RuntimeObjectPtr IOModule::file_open(string path, string mode) {
    auto promise = make_shared<Promise>();

    async_ctx->executor->submit([=]() {
        try {
            auto file = open_file_impl(path, mode);
            promise->fulfill(file);
        } catch (const exception& e) {
            promise->reject(make_error(e.what()));
        }
    });

    return promise;
}
```

### 4.3 HTTP Client/Server
```yona
module HTTP\Client exports get, post, put, delete as

# All return promises automatically
get url headers = <native>
post url body headers = <native>
put url body headers = <native>
delete url headers = <native>

end

module HTTP\Server exports listen, respond as

listen port handler = <native>
respond request response = <native>

end
```

## Phase 5: LLVM Compiler Integration

### 5.1 Coroutine Support
```cpp
// Use LLVM coroutines for async functions
class AsyncCodegen {
    llvm::Function* generate_async_function(FunctionExpr* func);
    llvm::Value* emit_await(llvm::Value* promise);
    llvm::Value* emit_async_call(llvm::Function* func, vector<llvm::Value*> args);
};
```

### 5.2 Runtime Library
```cpp
// Runtime support library (linked with compiled code)
extern "C" {
    void* yona_promise_create();
    void yona_promise_fulfill(void* promise, void* value);
    void* yona_promise_await(void* promise);
    void* yona_async_execute(void* (*func)(void*), void* arg);
}
```

### 5.3 Optimization Passes
```cpp
// LLVM optimization passes for async code
class AsyncOptimizationPass : public llvm::FunctionPass {
    // Inline small async functions
    // Eliminate unnecessary promise allocations
    // Merge sequential async operations
};
```

## Phase 6: Dependency Analysis & Auto-Parallelization

### 6.1 Static Analysis
```cpp
class ParallelizationAnalyzer {
    // Analyze let bindings for dependencies
    bool can_parallelize(vector<AliasExpr*> bindings);

    // Extract independent expression groups
    vector<vector<ExprNode*>> get_parallel_groups(ExprNode* expr);
};
```

### 6.2 Runtime Scheduling
```cpp
class Scheduler {
    // Work-stealing queue for load balancing
    deque<function<void()>> local_queue;

    // Schedule parallel tasks
    vector<shared_ptr<Promise>> schedule_parallel(vector<function<RuntimeObjectPtr()>> tasks);
};
```

## Implementation Timeline

### Month 1: Core Infrastructure
- [x] Promise type and basic operations - COMPLETED
- [x] Thread pool implementation - COMPLETED
- [x] AsyncContext integration - COMPLETED
- [x] Dependency analyzer for parallel execution - COMPLETED

### Month 2: Interpreter Integration
- [ ] Async expression evaluation
- [ ] Automatic promise unwrapping
- [ ] Parallel let bindings

### Month 3: STM Implementation
- [ ] TVar and Transaction types
- [ ] STM runtime
- [ ] Language integration

### Month 4: Standard Library
- [ ] Async module
- [ ] IO async operations
- [ ] HTTP client/server

### Month 5: LLVM Compiler
- [ ] Coroutine generation
- [ ] Runtime library
- [ ] Optimization passes

### Month 6: Advanced Features
- [ ] Dependency analysis
- [ ] Auto-parallelization
- [ ] Performance tuning

## Testing Strategy

### Unit Tests
```cpp
TEST_CASE("Promise fulfillment") {
    auto promise = make_shared<Promise>();
    auto result = make_shared<RuntimeObject>(Int, 42);

    thread t([=]() {
        this_thread::sleep_for(100ms);
        promise->fulfill(result);
    });

    auto value = promise->await();
    CHECK(value->get<int>() == 42);
    t.join();
}
```

### Integration Tests
```yona
# Test parallel file operations
let
    file1 = File::open "test1.txt" {:read}
    file2 = File::open "test2.txt" {:read}
    content1 = File::read_all file1
    content2 = File::read_all file2
in
    # Both files read in parallel
    assert (String::length content1) > 0
    assert (String::length content2) > 0
end
```

### Performance Benchmarks
- Measure speedup for parallel operations
- Compare with synchronous execution
- Profile thread pool efficiency
- Benchmark STM transaction throughput

## Key Design Decisions

1. **Transparent Promises**: Promises are automatically unwrapped, no explicit await syntax
2. **Dependency Analysis**: Static analysis determines parallelizable operations
3. **STM over Locks**: Use STM for safe concurrent state management
4. **Work Stealing**: Thread pool uses work-stealing for load balancing
5. **Zero-Cost Abstractions**: Async operations compile to efficient native code

## Standard Library Modules to Implement

1. **Async** - Core async operations
2. **STM** - Software Transactional Memory
3. **Channel** - CSP-style channels (optional)
4. **Parallel** - Parallel collections operations
5. **Timer** - Delays and timeouts
6. **HTTP** - Async HTTP client/server
7. **Socket** - Async TCP/UDP
8. **File** - Async file operations

## Performance Considerations

1. **Promise Allocation**: Use object pools to reduce allocation overhead
2. **Thread Pool Size**: Default to CPU cores, configurable
3. **Context Switching**: Minimize with continuation-passing style
4. **Lock Contention**: Use lock-free data structures where possible
5. **Memory Ordering**: Use appropriate atomics for performance

## Compatibility Notes

1. All existing synchronous code continues to work
2. Async operations are opt-in through module usage
3. Promises integrate transparently with existing types
4. STM transactions compose with regular functions

This implementation plan provides a complete async system that aligns with Yona's philosophy of transparent concurrency without explicit async/await syntax.
