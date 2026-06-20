#pragma once

#include "log_service_package.hpp"
#include "log_service_query.hpp"
#include "log_service_recovery.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace naviai::log {

struct FileNamingPlan {
    std::filesystem::path path;
    std::string file_type;
    std::string file_suffix;
    std::int64_t start_time_us{0};
    std::int64_t end_time_us{0};
    bool sealed{false};
};

struct LogServicePolicy {
    std::size_t switch_buffer_limit{1024};
    bool block_on_buffer_full{true};
    std::size_t upload_retry_limit{3};
};

struct OperationResult {
    bool success{false};
    std::string message;
    std::filesystem::path path;
};

struct LogServiceState {
    LogServicePolicy policy;
};

class LogService {
  public:
    explicit LogService(std::filesystem::path root_dir,
                        LogServicePolicy policy = {});

    std::optional<FileNamingPlan> BuildActiveFilePlan(
        const std::string& file_type,
        const std::string& file_suffix,
        std::optional<std::int64_t> start_time_us = std::nullopt) const;
    std::optional<FileNamingPlan> BuildSealedFilePlan(
        const std::string& file_type,
        const std::string& file_suffix,
        std::int64_t start_time_us,
        std::optional<std::int64_t> end_time_us = std::nullopt) const;
    std::optional<FileNamingPlan> BuildSealedFilePlanFromActivePath(
        const std::filesystem::path& active_path,
        std::optional<std::int64_t> end_time_us = std::nullopt) const;
    bool IsActiveFilePath(const std::filesystem::path& path) const;
    PackagePreparationResult PrepareFilesForPackaging(
        const std::vector<std::filesystem::path>& files) const;
    RecoveryTask RecoverActiveFiles(
        const std::vector<std::filesystem::path>& active_paths,
        std::int64_t end_time_us) const;

    QueryResult QueryLogs(const QueryCondition& condition);
    PackageTask PackageLogs(const QueryCondition& condition,
                            const std::filesystem::path& package_root_dir);
    DeliveryTask ExportPackage(const std::filesystem::path& package_path,
                               const std::filesystem::path& target_dir);
    DeliveryTask UploadPackage(const std::filesystem::path& package_path,
                               const std::string& target_uri);
    LogServiceState GetState() const;

  private:
    std::filesystem::path root_dir_;
    LogServicePolicy policy_;

    static std::string FormatTimestamp(std::int64_t time_us);
    static std::int64_t NowMicroseconds();
    static std::string BuildFileName(std::int64_t start_time_us,
                                     std::optional<std::int64_t> end_time_us,
                                     const std::string& suffix);
    static std::optional<std::pair<std::int64_t, std::int64_t>> ParseTimeRange(
        const std::filesystem::path& path);
    static std::string NormalizeFileType(const std::string& file_type);
    static bool IsFileTypeSafe(const std::string& file_type);

    static std::string NormalizeSuffix(const std::filesystem::path& path);
    static bool CompressFileToGzip(const std::filesystem::path& source_path);
    std::filesystem::path BuildDirectoryPath(const std::string& file_type) const;
    std::filesystem::path BuildActivePath(const std::string& file_type,
                                          const std::string& suffix,
                                          std::int64_t start_time_us) const;
    std::filesystem::path BuildSealedPath(const std::string& file_type,
                                          const std::string& suffix,
                                          std::int64_t start_time_us,
                                          std::int64_t end_time_us) const;
    std::optional<FileNamingPlan> ParseFileNamingPlan(
        const std::filesystem::path& path) const;
    void EnsureDirectory(const std::filesystem::path& path) const;
    bool ValidateQueryCondition(const QueryCondition& condition,
                                std::string* error_message) const;
    QueryResult QueryLogsImpl(const QueryCondition& condition) const;
};

}  // namespace naviai::log
