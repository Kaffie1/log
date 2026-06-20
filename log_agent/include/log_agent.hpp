#pragma once

#include <cstddef>
#include <cstdint>
#include <condition_variable>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace naviai::log {

struct LogFileEntry {
    std::filesystem::path path;
    std::string file_type;
    std::string file_state;
    std::int64_t start_time_us{0};
    std::int64_t end_time_us{0};
    std::size_t size_bytes{0};
};

struct FileGovernPolicy {
    std::int64_t retention_window_seconds{48LL * 60LL * 60LL};
    std::int64_t scan_interval_ms{5LL * 1000LL};
    std::int64_t cleanup_interval_ms{60LL * 1000LL};
    std::size_t compress_retry_limit{3};
    bool delete_raw_after_compress{true};
};

struct AbnormalLogSession {
    std::filesystem::path path;
    std::string session_id;
    std::string abnormal_type;
    std::string detected_reason;
    std::int64_t detected_time_us{0};
    bool recovered{false};
};

struct AgentScheduleState {
    bool running{false};
    std::int64_t last_scan_time_us{0};
    std::int64_t last_cleanup_time_us{0};
    std::int64_t last_recovery_time_us{0};
    std::size_t pending_compress_tasks{0};
    bool draining_before_exit{false};
};

struct LogAgentStats {
    std::size_t total_files{0};
    std::size_t active_files{0};
    std::size_t sealed_files{0};
    std::size_t compressed_files{0};
    std::size_t abnormal_files{0};
    std::uint64_t total_size_bytes{0};
};

struct LogAgentState {
    std::filesystem::path root_dir;
    FileGovernPolicy govern_policy;
    std::vector<LogFileEntry> files;
    std::vector<AbnormalLogSession> abnormal_sessions;
    AgentScheduleState schedule_state;
    LogAgentStats stats;
};

struct LogAgentResult {
    bool success{false};
    std::string message;
    std::size_t affected_files{0};
};

class LogAgent {
  public:
    explicit LogAgent(std::filesystem::path root_dir,
                      FileGovernPolicy policy = {});
    ~LogAgent();

    LogAgent(const LogAgent&) = delete;
    LogAgent& operator=(const LogAgent&) = delete;

    LogAgentResult Start();
    LogAgentResult Stop(bool drain = true);

    LogAgentResult ScanNow();
    LogAgentResult RecoverNow();
    LogAgentResult CompressNow();
    LogAgentResult DrainNow();
    LogAgentResult CleanupNow(bool dry_run = false);

    LogAgentState GetState() const;

  private:
    void WorkerLoop();
    LogAgentResult RunScanLocked();
    LogAgentResult RunRecoverLocked();
    LogAgentResult RunCompressLocked();
    LogAgentResult RunCleanupLocked(bool dry_run);

    std::filesystem::path root_dir_;
    FileGovernPolicy policy_;

    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::thread worker_;
    bool running_{false};
    bool stop_requested_{false};
    bool wake_requested_{false};

    LogAgentState state_;
    std::unordered_map<std::string, std::size_t> compress_failures_;
};

}  // namespace naviai::log
