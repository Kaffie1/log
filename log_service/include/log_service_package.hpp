#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

#include "log_service_query.hpp"

namespace naviai::log {

struct PackagePreparationResult {
    bool success{false};
    bool has_active_files{false};
    std::string message;
    std::vector<std::filesystem::path> prepared_files;
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

}  // namespace naviai::log
