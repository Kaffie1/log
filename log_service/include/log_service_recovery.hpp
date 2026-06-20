#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace naviai::log {

struct RecoveryTask {
    bool success{false};
    std::string message;
    std::vector<std::filesystem::path> source_paths;
    std::vector<std::filesystem::path> recovered_paths;
};

}  // namespace naviai::log
