#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace naviai::log {

struct QueryCondition {
    std::int64_t start_time_us{0};
    std::int64_t end_time_us{0};
    std::string file_type;
    std::string module_name;
    std::string log_level;
    std::string file_suffix;
};

struct QueryResult {
    bool success{false};
    std::string message;
    std::size_t total_files{0};
    std::vector<std::filesystem::path> files;
};

}  // namespace naviai::log
