#include "log_agent.hpp"

#include <chrono>

namespace naviai::log {

LogAgent::LogAgent(std::filesystem::path root_dir, FileGovernPolicy policy)
    : root_dir_(std::move(root_dir)), policy_(policy) {
    state_.root_dir = root_dir_;
    state_.govern_policy = policy_;
}

LogAgent::~LogAgent() {
    Stop(true);
}

LogAgentResult LogAgent::Start() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (running_) {
        return {true, "log agent is already running", 0};
    }

    stop_requested_ = false;
    wake_requested_ = false;
    running_ = true;
    state_.schedule_state.running = true;
    worker_ = std::thread(&LogAgent::WorkerLoop, this);
    return {true, "log agent started", 0};
}

LogAgentResult LogAgent::Stop(bool drain) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_ && !worker_.joinable()) {
            return {true, "log agent is already stopped", 0};
        }
        stop_requested_ = true;
        wake_requested_ = true;
        state_.schedule_state.draining_before_exit = drain;
        cv_.notify_all();
    }

    if (worker_.joinable()) {
        worker_.join();
    }

    std::lock_guard<std::mutex> lock(mutex_);
    running_ = false;
    state_.schedule_state.running = false;
    state_.schedule_state.draining_before_exit = false;
    return {true, "log agent stopped", 0};
}

LogAgentResult LogAgent::ScanNow() {
    std::lock_guard<std::mutex> lock(mutex_);
    return RunScanLocked();
}

LogAgentResult LogAgent::RecoverNow() {
    std::lock_guard<std::mutex> lock(mutex_);
    return RunRecoverLocked();
}

LogAgentResult LogAgent::CompressNow() {
    std::lock_guard<std::mutex> lock(mutex_);
    return RunCompressLocked();
}

LogAgentResult LogAgent::CleanupNow(bool dry_run) {
    std::lock_guard<std::mutex> lock(mutex_);
    return RunCleanupLocked(dry_run);
}

LogAgentState LogAgent::GetState() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_;
}

void LogAgent::WorkerLoop() {
    std::unique_lock<std::mutex> lock(mutex_);

    RunScanLocked();
    RunRecoverLocked();
    RunScanLocked();

    while (!stop_requested_) {
        const auto wait_duration =
            std::chrono::milliseconds(std::max<std::int64_t>(policy_.scan_interval_ms, 100LL));
        cv_.wait_for(lock, wait_duration, [&]() {
            return stop_requested_ || wake_requested_;
        });
        wake_requested_ = false;
        if (stop_requested_) {
            break;
        }

        RunScanLocked();
        RunCompressLocked();

        const auto now_us = NowMicroseconds();
        const auto last_cleanup_us = state_.schedule_state.last_cleanup_time_us;
        const auto cleanup_interval_us =
            std::max<std::int64_t>(policy_.cleanup_interval_ms, 1000LL) * 1000LL;
        if (last_cleanup_us == 0 || now_us - last_cleanup_us >= cleanup_interval_us) {
            RunCleanupLocked(false);
        }
    }

    if (state_.schedule_state.draining_before_exit) {
        RunScanLocked();
        RunCompressLocked();
    }
}

}  // namespace naviai::log
