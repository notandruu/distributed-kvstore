#include "thread_pool.h"

ThreadPool::ThreadPool(size_t num_threads) {
    workers_.reserve(num_threads);
    for (size_t i = 0; i < num_threads; ++i) {
        workers_.emplace_back([this]() { worker_loop(); });
    }
}

ThreadPool::~ThreadPool() {
    shutdown();
}

void ThreadPool::submit(std::function<void()> task) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        tasks_.push(std::move(task));
    }
    cv_.notify_one();
}

void ThreadPool::shutdown() {
    if (stop_) return;
    stop_ = true;
    cv_.notify_all();
    for (auto& w : workers_) {
        if (w.joinable()) w.join();
    }
}

void ThreadPool::shutdown_now() {
    stop_now_ = true;
    cv_.notify_all();
    for (auto& w : workers_) {
        if (w.joinable()) w.join();
    }
}

size_t ThreadPool::num_threads() const {
    return workers_.size();
}

size_t ThreadPool::pending_tasks() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return tasks_.size();
}

void ThreadPool::worker_loop() {
    while (true) {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this]() {
            return stop_now_.load() || stop_.load() || !tasks_.empty();
        });
        if (stop_now_) return;
        if (stop_ && tasks_.empty()) return;
        auto task = std::move(tasks_.front());
        tasks_.pop();
        lock.unlock();
        task();
    }
}
