#pragma once

#include <filesystem>

namespace naviai::log {

struct LogFileEntry;

namespace log_agent_detail {

bool ShouldCompressFile(const LogFileEntry& file);
bool CompressFileToGzip(const std::filesystem::path& source_path,
                        bool delete_raw_after_compress);

}  // namespace log_agent_detail

}  // namespace naviai::log
