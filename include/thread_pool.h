#pragma once

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace yona::runtime::async {

class ThreadPool {
private:
    vector<thread> workers;
    queue<function<void()>> tasks;
    mutable mutex queue_mutex;
    condition_variable cv;
    atomic<bool> stop{false};
    atomic<size_t> active_tasks{0};

    // Worker thread function
    void worker_thread();

public:
    // Create thread pool with specified number of threads (default: hardware concurrency)
    explicit ThreadPool(size_t num_threads = 0);
    ~ThreadPool();

    // Submit a task for execution
    void submit(function<void()> task);

    // Submit an async task and get a future
    template<typename T>
    future<T> submit_async(function<T()> task) {
        auto promise = make_shared<std::promise<T>>();
        auto future = promise->get_future();

        submit([promise, task]() {
            try {
                promise->set_value(task());
            } catch (...) {
                promise->set_exception(current_exception());
            }
        });

        return future;
    }

    // Get the number of worker threads
    size_t thread_count() const { return workers.size(); }

    // Get the number of pending tasks
    size_t pending_tasks() const {
        lock_guard<mutex> lock(queue_mutex);
        return tasks.size();
    }

    // Get the number of active tasks
    size_t get_active_tasks() const { return active_tasks.load(); }

    // Wait for all tasks to complete
    void wait_all();

    // Shutdown the thread pool
    void shutdown();

    // Check if the pool is shutting down
    bool is_stopping() const { return stop.load(); }
};

// Work-stealing queue for better load balancing
template<typename T>
class WorkStealingQueue {
private:
    deque<T> queue;
    mutable std::mutex mtx;

public:
    void push(T item) {
        std::lock_guard<std::mutex> lock(mtx);
        queue.push_back(std::move(item));
    }

    bool try_pop(T& item) {
        std::lock_guard<std::mutex> lock(mtx);
        if (queue.empty()) return false;
        item = std::move(queue.front());
        queue.pop_front();
        return true;
    }

    bool try_steal(T& item) {
        std::lock_guard<std::mutex> lock(mtx);
        if (queue.empty()) return false;
        item = std::move(queue.back());
        queue.pop_back();
        return true;
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(mtx);
        return queue.empty();
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mtx);
        return queue.size();
    }
};

// Advanced thread pool with work stealing
class WorkStealingThreadPool {
private:
    struct WorkerThread {
        thread thread;
        unique_ptr<WorkStealingQueue<function<void()>>> local_queue;

        WorkerThread() : local_queue(make_unique<WorkStealingQueue<function<void()>>>()) {}
    };

    vector<unique_ptr<WorkerThread>> workers;
    WorkStealingQueue<function<void()>> global_queue;
    atomic<bool> stop{false};
    atomic<size_t> next_worker{0};

    void worker_thread(size_t worker_id);

public:
    explicit WorkStealingThreadPool(size_t num_threads = 0);
    ~WorkStealingThreadPool();

    void submit(function<void()> task);
    void shutdown();
};

} // namespace yona::runtime::async
