#pragma once

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace naviai::log_module {

class ThreadPoolManager {
  public:
    ThreadPoolManager(size_t worker_count, size_t max_queue_size);
    ~ThreadPoolManager();
    void Start();
    void Stop();
    void Post(std::function<void()> task);

  private:
    void WorkerLoop();

    size_t worker_count_;
    size_t max_queue_size_;
    bool running_{false};
    std::mutex mutex_;
    std::condition_variable cv_;
    std::queue<std::function<void()>> tasks_;
    std::vector<std::thread> workers_;
};

class AsyncDispatcher {
  public:
    AsyncDispatcher(size_t worker_count, size_t max_queue_size);
    void Start();
    void Stop();
    void Dispatch(std::function<void()> task);

  private:
    ThreadPoolManager thread_pool_;
};

class FlushScheduler {
  public:
    explicit FlushScheduler(size_t interval_seconds);
    ~FlushScheduler();
    void Start(std::function<void()> flush_callback);
    void Stop();

  private:
    size_t interval_seconds_;
    bool running_{false};
    std::thread worker_;
};

}  // namespace naviai::log_module
