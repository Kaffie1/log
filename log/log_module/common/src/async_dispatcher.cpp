#include "async_dispatcher.hpp"

#include <chrono>

namespace naviai::log_module {

ThreadPoolManager::ThreadPoolManager(size_t worker_count, size_t max_queue_size)
    : worker_count_(worker_count == 0 ? 1 : worker_count),
      max_queue_size_(max_queue_size == 0 ? 1 : max_queue_size) {}

ThreadPoolManager::~ThreadPoolManager() {
    Stop();
}

void ThreadPoolManager::Start() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (running_) {
        return;
    }
    running_ = true;
    for (size_t i = 0; i < worker_count_; ++i) {
        workers_.emplace_back([this]() { WorkerLoop(); });
    }
}

void ThreadPoolManager::Stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_) {
            return;
        }
        running_ = false;
    }
    cv_.notify_all();
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    workers_.clear();
}

void ThreadPoolManager::Post(std::function<void()> task) {
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [this]() {
        return !running_ || tasks_.size() < max_queue_size_;
    });
    if (!running_) {
        return;
    }
    {
        tasks_.push(std::move(task));
    }
    lock.unlock();
    cv_.notify_one();
}

void ThreadPoolManager::WorkerLoop() {
    while (true) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this]() { return !running_ || !tasks_.empty(); });
            if (!running_ && tasks_.empty()) {
                return;
            }
            task = std::move(tasks_.front());
            tasks_.pop();
        }
        cv_.notify_one();
        if (task) {
            task();
        }
    }
}

AsyncDispatcher::AsyncDispatcher(size_t worker_count, size_t max_queue_size)
    : thread_pool_(worker_count, max_queue_size) {}

void AsyncDispatcher::Start() {
    thread_pool_.Start();
}

void AsyncDispatcher::Stop() {
    thread_pool_.Stop();
}

void AsyncDispatcher::Dispatch(std::function<void()> task) {
    thread_pool_.Post(std::move(task));
}

FlushScheduler::FlushScheduler(size_t interval_seconds)
    : interval_seconds_(interval_seconds == 0 ? 1 : interval_seconds) {}

FlushScheduler::~FlushScheduler() {
    Stop();
}

void FlushScheduler::Start(std::function<void()> flush_callback) {
    if (running_) {
        return;
    }
    running_ = true;
    worker_ = std::thread([this, flush_callback = std::move(flush_callback)]() {
        while (running_) {
            std::this_thread::sleep_for(
                std::chrono::seconds(interval_seconds_));
            if (running_ && flush_callback) {
                flush_callback();
            }
        }
    });
}

void FlushScheduler::Stop() {
    running_ = false;
    if (worker_.joinable()) {
        worker_.join();
    }
}

}  // namespace naviai::log_module
