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

using namespace std;

class ThreadPool {
private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    mutable std::mutex queue_mutex;
    std::condition_variable cv;
    std::atomic<bool> stop{false};
    std::atomic<size_t> active_tasks{0};

    // Worker thread function
    void worker_thread();

public:
    // Create thread pool with specified number of threads (default: hardware concurrency)
    explicit ThreadPool(size_t num_threads = 0);
    ~ThreadPool();

    // Submit a task for execution
    void submit(std::function<void()> task);

    // Submit an async task and get a future
    template<typename T>
    std::future<T> submit_async(std::function<T()> task) {
        auto promise = std::make_shared<std::promise<T>>();
        auto future = promise->get_future();

        submit([promise, task]() {
            try {
                promise->set_value(task());
            } catch (...) {
                promise->set_exception(std::current_exception());
            }
        });

        return future;
    }

    // Get the number of worker threads
    size_t thread_count() const { return workers.size(); }

    // Get the number of pending tasks
    size_t pending_tasks() const {
        std::lock_guard<std::mutex> lock(queue_mutex);
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
    std::deque<T> queue;
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
        std::thread thread;
        std::unique_ptr<WorkStealingQueue<std::function<void()>>> local_queue;

        WorkerThread() : local_queue(std::make_unique<WorkStealingQueue<std::function<void()>>>()) {}
    };

    std::vector<std::unique_ptr<WorkerThread>> workers;
    WorkStealingQueue<std::function<void()>> global_queue;
    std::atomic<bool> stop{false};
    std::atomic<size_t> next_worker{0};

    void worker_thread(size_t worker_id);

public:
    explicit WorkStealingThreadPool(size_t num_threads = 0);
    ~WorkStealingThreadPool();

    void submit(std::function<void()> task);
    void shutdown();
};

} // namespace yona::runtime::async
