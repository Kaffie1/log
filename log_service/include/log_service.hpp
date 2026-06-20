#pragma once

#include "log_manager.hpp"
#include "log_service_types.hpp"
#include "log_types.hpp"

#include <condition_variable>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>

namespace naviai::log {

class LogService {
  public:
    explicit LogService(std::filesystem::path root_dir,
                        LogServicePolicy policy = {});

    OperationResult CreateActiveFile(const std::string& file_type,
                                     const std::string& file_suffix);
    OperationResult CreateActiveFileAndActivateWriter(
        const std::string& file_type,
        const std::string& file_suffix,
        const LoggerConfig& base_config = LoggerConfig());
    OperationResult SwitchSegment();
    OperationResult SwitchSegmentAndActivateWriter(
        const LoggerConfig& base_config = LoggerConfig());
    OperationResult SealFile();
    OperationResult ActivateWriter(const LoggerConfig& base_config = LoggerConfig());
    OperationResult WriteLog(const std::string& module_name,
                             LogLevel level,
                             const std::string& payload);

    QueryResult QueryLogs(const QueryCondition& condition);
    PackageTask PackageLogs(const QueryCondition& condition,
                            const std::filesystem::path& package_root_dir);
    DeliveryTask ExportPackage(const std::filesystem::path& package_path,
                               const std::filesystem::path& target_dir);
    DeliveryTask UploadPackage(const std::filesystem::path& package_path,
                               const std::string& target_uri);
    std::optional<LoggerConfig> BuildWriterConfig(
        const LoggerConfig& base_config = LoggerConfig());

    std::optional<ActiveFileSession> GetActiveSession() const;
    LogServiceState GetState() const;

  private:
    std::filesystem::path root_dir_;
    LogServicePolicy policy_;

    struct BufferedLogEntry {
        std::string module_name;
        LogLevel level{LogLevel::Info};
        std::string payload;
    };

    mutable std::mutex mutex_;
    mutable std::condition_variable switch_cv_;
    std::optional<ActiveFileSession> active_session_;
    std::vector<BufferedLogEntry> buffered_logs_;
    std::optional<LoggerConfig> writer_base_config_;
    bool writer_activated_{false};

    static std::string FormatTimestamp(std::int64_t time_us);
    static std::int64_t NowMicroseconds();
    static std::string BuildFileName(std::int64_t start_time_us,
                                     std::optional<std::int64_t> end_time_us,
                                     const std::string& suffix);
    static std::optional<std::pair<std::int64_t, std::int64_t>> ParseTimeRange(
        const std::filesystem::path& path);
    static std::string NormalizeFileType(const std::string& file_type);
    static bool IsFileTypeSafe(const std::string& file_type);

    std::filesystem::path BuildDirectoryPath(const std::string& file_type) const;
    std::filesystem::path BuildActivePath(const std::string& file_type,
                                          const std::string& suffix,
                                          std::int64_t start_time_us) const;
    std::filesystem::path BuildSealedPath(const std::string& file_type,
                                          const std::string& suffix,
                                          std::int64_t start_time_us,
                                          std::int64_t end_time_us) const;
    OperationResult CreateActiveFileLocked(const std::string& file_type,
                                           const std::string& file_suffix);
    std::optional<LoggerConfig> BuildWriterConfigLocked(
        const LoggerConfig& base_config);

    ActiveFileSession* FindActiveSessionLocked();
    const ActiveFileSession* FindActiveSessionLocked() const;
    void EnsureDirectory(const std::filesystem::path& path) const;
    bool ValidateQueryCondition(const QueryCondition& condition,
                                std::string* error_message) const;
    QueryResult QueryLogsLocked(const QueryCondition& condition) const;
    void FlushWriterSafely() const;
    OperationResult WriteLogLocked(const std::string& module_name,
                                   LogLevel level,
                                   const std::string& payload);
    OperationResult FlushBufferedLogsLocked();
    OperationResult ForceSealIntersectedSessions(
        std::unique_lock<std::mutex>& lock,
        const QueryCondition& condition);
    OperationResult PrepareWriterActivationLocked(const std::string& file_type);
    OperationResult SealFileLocked(std::optional<std::int64_t> explicit_end_time);
};

}  // namespace naviai::log
