#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace naviai::log {

struct ActiveFileSession {
    std::filesystem::path active_path;
    std::string file_type;
    std::string file_suffix;
    std::int64_t start_time_us{0};
    bool switching{false};
    std::size_t buffered_records{0};
};

struct LogServicePolicy {
    std::size_t switch_buffer_limit{1024};
    bool block_on_buffer_full{true};
    std::size_t upload_retry_limit{3};
};

struct QueryCondition {
    std::int64_t start_time_us{0};
    std::int64_t end_time_us{0};
    std::string file_type;
    std::string module_name;
    std::string log_level;
    std::string file_suffix;
};

struct OperationResult {
    bool success{false};
    std::string message;
    std::filesystem::path path;
};

struct QueryResult {
    bool success{false};
    std::string message;
    std::size_t total_files{0};
    std::vector<std::filesystem::path> files;
};

struct PackageTask {
    std::string task_id;
    QueryCondition condition;
    std::filesystem::path output_path;
    std::filesystem::path manifest_path;
    std::string message;
    std::string task_state;
    std::vector<std::filesystem::path> source_files;
};

struct DeliveryTask {
    std::string task_id;
    std::string task_type;
    std::filesystem::path source_path;
    std::filesystem::path target_path;
    std::string message;
    std::string task_state;
    std::size_t retry_count{0};
};

struct LogServiceState {
    std::optional<ActiveFileSession> active_session;
    LogServicePolicy policy;
    bool writer_activated{false};
};

}  // namespace naviai::log
