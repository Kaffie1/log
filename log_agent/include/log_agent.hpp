#pragma once

#include "log_agent_types.hpp"

#include <condition_variable>
#include <filesystem>
#include <mutex>
#include <thread>
#include <unordered_map>

namespace naviai::log {

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
    struct ParsedFileName {
        std::int64_t start_time_us{0};
        std::int64_t end_time_us{0};
        bool has_end_time{false};
        std::string suffix;
        bool compressed{false};
    };

    void WorkerLoop();
    LogAgentResult RunScanLocked();
    LogAgentResult RunRecoverLocked();
    LogAgentResult RunCompressLocked();
    LogAgentResult RunCleanupLocked(bool dry_run);

    static std::int64_t NowMicroseconds();
    static std::int64_t ParseTimestamp(const std::string& text);
    static std::string FormatTimestamp(std::int64_t time_us);
    static std::string DetermineFileState(const ParsedFileName& parsed,
                                          bool abnormal_marker);
    static bool ParseFileName(const std::filesystem::path& path,
                              ParsedFileName* parsed);
    static std::string BuildSessionId(const std::filesystem::path& path,
                                      std::int64_t start_time_us);
    static bool ShouldCompressFile(const LogFileEntry& file);
    static bool CompressFileToGzip(const std::filesystem::path& source_path,
                                   bool delete_raw_after_compress);
    static bool ReadLastTimestampFromTextFile(const std::filesystem::path& path,
                                              std::int64_t* time_us);
    static bool ReadLastTimestampFromIndexFile(const std::filesystem::path& path,
                                               std::int64_t* time_us);
    static bool LooksLikeManagedFile(const std::filesystem::path& path);

    std::filesystem::path BuildRecoveredPath(const std::filesystem::path& path,
                                             std::int64_t start_time_us,
                                             std::int64_t end_time_us) const;

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
