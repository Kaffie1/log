#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace naviai::log {

struct LogFileEntry;

namespace log_agent_detail {

struct ParsedFileName {
    std::int64_t start_time_us{0};
    std::int64_t end_time_us{0};
    bool has_end_time{false};
    std::string suffix;
    bool compressed{false};
};

std::int64_t NowMicroseconds();
std::int64_t ParseTimestamp(const std::string& text);
std::string FormatTimestamp(std::int64_t time_us);
std::string DetermineFileState(const ParsedFileName& parsed, bool abnormal_marker);
bool ParseFileName(const std::filesystem::path& path, ParsedFileName* parsed);
std::string BuildSessionId(const std::filesystem::path& path,
                           std::int64_t start_time_us);
bool LooksLikeManagedFile(const std::filesystem::path& path);

}  // namespace log_agent_detail

}  // namespace naviai::log
