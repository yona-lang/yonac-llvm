#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <unordered_map>
#include <vector>
#include "runtime.h"

namespace yona::runtime::async {

using namespace std;
using namespace yona::interp::runtime;
using RuntimeObjectPtr = yona::interp::runtime::RuntimeObjectPtr;

enum class PromiseState {
    PENDING,
    FULFILLED,
    REJECTED
};

class ThreadPool;  // Forward declaration

struct Promise {
    PromiseState state;
    RuntimeObjectPtr value;
    RuntimeObjectPtr error;
    vector<function<void(RuntimeObjectPtr)>> callbacks;
    mutable mutex state_mutex;
    condition_variable cv;

    // Promise pipelining support
    queue<function<RuntimeObjectPtr(RuntimeObjectPtr)>> pipeline;

    Promise() : state(PromiseState::PENDING) {}

    void fulfill(RuntimeObjectPtr val);
    void reject(RuntimeObjectPtr err);
    RuntimeObjectPtr await();  // Blocking wait
    void then(function<void(RuntimeObjectPtr)> callback);

    // Pipeline a function to be called when promise resolves
    void pipe(function<RuntimeObjectPtr(RuntimeObjectPtr)> transform);

    // Check if promise is resolved
    bool is_resolved() const {
        lock_guard<mutex> lock(state_mutex);
        return state != PromiseState::PENDING;
    }

    // Get the resolved value without blocking (returns nullptr if pending)
    RuntimeObjectPtr try_get() const {
        lock_guard<mutex> lock(state_mutex);
        if (state == PromiseState::FULFILLED) {
            return value;
        } else if (state == PromiseState::REJECTED) {
            return error;
        }
        return nullptr;
    }
};

using PromisePtr = shared_ptr<Promise>;

struct AsyncContext {
    shared_ptr<ThreadPool> executor;
    unordered_map<size_t, PromisePtr> active_promises;
    atomic<size_t> next_promise_id{1};
    mutable mutex promises_mutex;

    AsyncContext();
    ~AsyncContext();

    // Register a promise and get its ID
    size_t register_promise(PromisePtr promise);

    // Get a promise by ID
    PromisePtr get_promise(size_t id) const;

    // Remove a completed promise
    void remove_promise(size_t id);

    // Submit an async task
    PromisePtr submit_async(function<RuntimeObjectPtr()> task);

    // Run multiple tasks in parallel
    vector<PromisePtr> run_parallel(vector<function<RuntimeObjectPtr()>> tasks);

    // Wait for all active promises to complete
    void wait_all();
};

// Global async context for the runtime
extern shared_ptr<AsyncContext> get_global_async_context();

// Helper to create a resolved promise
PromisePtr make_resolved_promise(RuntimeObjectPtr value);

// Helper to create a rejected promise
PromisePtr make_rejected_promise(RuntimeObjectPtr error);

// Helper to wrap a value in a promise if it isn't already
PromisePtr ensure_promise(RuntimeObjectPtr value);

} // namespace yona::runtime::async
