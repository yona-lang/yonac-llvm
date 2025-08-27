#include "runtime_async.h"
#include "thread_pool.h"
#include <iostream>

namespace yona::runtime::async {

using namespace std;

// Promise implementation

void Promise::fulfill(RuntimeObjectPtr val) {
    vector<function<void(RuntimeObjectPtr)>> callbacks_to_run;
    queue<function<RuntimeObjectPtr(RuntimeObjectPtr)>> pipeline_to_run;

    {
        lock_guard<mutex> lock(state_mutex);
        if (state != PromiseState::PENDING) {
            return; // Already resolved
        }

        state = PromiseState::FULFILLED;
        value = val;
        callbacks_to_run = std::move(callbacks);
        pipeline_to_run = std::move(pipeline);
    }

    cv.notify_all();

    // Execute callbacks
    for (const auto& callback : callbacks_to_run) {
        try {
            callback(val);
        } catch (const exception& e) {
            cerr << "Exception in promise callback: " << e.what() << endl;
        }
    }

    // Execute pipeline transformations
    RuntimeObjectPtr current = val;
    while (!pipeline_to_run.empty()) {
        auto transform = pipeline_to_run.front();
        pipeline_to_run.pop();
        try {
            current = transform(current);
        } catch (const exception& e) {
            cerr << "Exception in promise pipeline: " << e.what() << endl;
            break;
        }
    }
}

void Promise::reject(RuntimeObjectPtr err) {
    vector<function<void(RuntimeObjectPtr)>> callbacks_to_run;

    {
        lock_guard<mutex> lock(state_mutex);
        if (state != PromiseState::PENDING) {
            return; // Already resolved
        }

        state = PromiseState::REJECTED;
        error = err;
        callbacks_to_run = std::move(callbacks);
        // Clear pipeline on rejection
        while (!pipeline.empty()) {
            pipeline.pop();
        }
    }

    cv.notify_all();

    // Execute callbacks with error
    for (const auto& callback : callbacks_to_run) {
        try {
            callback(err);
        } catch (const exception& e) {
            cerr << "Exception in promise error callback: " << e.what() << endl;
        }
    }
}

RuntimeObjectPtr Promise::await() {
    unique_lock<mutex> lock(state_mutex);
    cv.wait(lock, [this] { return state != PromiseState::PENDING; });

    if (state == PromiseState::FULFILLED) {
        return value;
    } else {
        // In a real implementation, we'd throw an exception with the error
        return error;
    }
}

void Promise::then(function<void(RuntimeObjectPtr)> callback) {
    bool should_execute = false;
    RuntimeObjectPtr result;

    {
        lock_guard<mutex> lock(state_mutex);
        if (state == PromiseState::PENDING) {
            callbacks.push_back(callback);
            return;
        }

        should_execute = true;
        result = (state == PromiseState::FULFILLED) ? value : error;
    }

    if (should_execute) {
        try {
            callback(result);
        } catch (const exception& e) {
            cerr << "Exception in promise then callback: " << e.what() << endl;
        }
    }
}

void Promise::pipe(function<RuntimeObjectPtr(RuntimeObjectPtr)> transform) {
    lock_guard<mutex> lock(state_mutex);
    if (state == PromiseState::PENDING) {
        pipeline.push(transform);
    } else if (state == PromiseState::FULFILLED) {
        // Already resolved, execute immediately
        try {
            value = transform(value);
        } catch (const exception& e) {
            cerr << "Exception in promise pipe: " << e.what() << endl;
        }
    }
}

// AsyncContext implementation

AsyncContext::AsyncContext() : executor(make_shared<ThreadPool>()) {}

AsyncContext::~AsyncContext() {
    wait_all();
}

size_t AsyncContext::register_promise(PromisePtr promise) {
    lock_guard<mutex> lock(promises_mutex);
    size_t id = next_promise_id.fetch_add(1);
    active_promises[id] = promise;
    return id;
}

PromisePtr AsyncContext::get_promise(size_t id) const {
    lock_guard<mutex> lock(promises_mutex);
    auto it = active_promises.find(id);
    if (it != active_promises.end()) {
        return it->second;
    }
    return nullptr;
}

void AsyncContext::remove_promise(size_t id) {
    lock_guard<mutex> lock(promises_mutex);
    active_promises.erase(id);
}

PromisePtr AsyncContext::submit_async(function<RuntimeObjectPtr()> task) {
    auto promise = make_shared<Promise>();
    size_t id = register_promise(promise);

    executor->submit([this, promise, task, id]() {
        try {
            auto result = task();
            promise->fulfill(result);
        } catch (const exception& e) {
            auto error = make_shared<RuntimeObject>(RuntimeObjectType::Error, string(e.what()));
            promise->reject(error);
        } catch (...) {
            auto error = make_shared<RuntimeObject>(RuntimeObjectType::Error, string("Unknown error in async task"));
            promise->reject(error);
        }
        remove_promise(id);
    });

    return promise;
}

vector<PromisePtr> AsyncContext::run_parallel(vector<function<RuntimeObjectPtr()>> tasks) {
    vector<PromisePtr> promises;
    promises.reserve(tasks.size());

    for (auto& task : tasks) {
        promises.push_back(submit_async(task));
    }

    return promises;
}

void AsyncContext::wait_all() {
    vector<PromisePtr> promises_to_wait;

    {
        lock_guard<mutex> lock(promises_mutex);
        for (const auto& [id, promise] : active_promises) {
            promises_to_wait.push_back(promise);
        }
    }

    for (const auto& promise : promises_to_wait) {
        promise->await();
    }

    executor->wait_all();
}

// Global async context

static shared_ptr<AsyncContext> g_async_context;
static mutex g_async_mutex;

shared_ptr<AsyncContext> get_global_async_context() {
    lock_guard<mutex> lock(g_async_mutex);
    if (!g_async_context) {
        g_async_context = make_shared<AsyncContext>();
    }
    return g_async_context;
}

// Helper functions

PromisePtr make_resolved_promise(RuntimeObjectPtr value) {
    auto promise = make_shared<Promise>();
    promise->fulfill(value);
    return promise;
}

PromisePtr make_rejected_promise(RuntimeObjectPtr error) {
    auto promise = make_shared<Promise>();
    promise->reject(error);
    return promise;
}

PromisePtr ensure_promise(RuntimeObjectPtr value) {
    // For now, just wrap any value in a resolved promise
    // Later we'll handle Promise types stored in RuntimeObject
    return make_resolved_promise(value);
}

} // namespace yona::runtime::async
