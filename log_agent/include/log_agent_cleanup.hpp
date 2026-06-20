#pragma once

#include <cstdint>

namespace naviai::log {

struct LogFileEntry;

namespace log_agent_detail {

bool ShouldCleanupFile(const LogFileEntry& file, std::int64_t cutoff_us);

}  // namespace log_agent_detail

}  // namespace naviai::log
