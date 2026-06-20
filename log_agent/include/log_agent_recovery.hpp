#pragma once

#include <cstdint>
#include <filesystem>

namespace naviai::log::log_agent_detail {

bool ReadLastTimestampFromTextFile(const std::filesystem::path& path,
                                   std::int64_t* time_us);
bool ReadLastTimestampFromIndexFile(const std::filesystem::path& path,
                                    std::int64_t* time_us);
std::filesystem::path BuildRecoveredPath(const std::filesystem::path& path,
                                         std::int64_t start_time_us,
                                         std::int64_t end_time_us);

}  // namespace naviai::log::log_agent_detail
