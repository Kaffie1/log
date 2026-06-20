#include "log_agent.hpp"
#include "log_agent_scan.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <system_error>

namespace naviai::log {
namespace {

constexpr char kAbnormalState[] = "abnormal";
constexpr char kActiveState[] = "active";
constexpr char kSealedState[] = "sealed";
constexpr char kCompressedState[] = "compressed";
constexpr char kCompressedSuffix[] = ".gz";

}  // namespace

using log_agent_detail::BuildSessionId;
using log_agent_detail::DetermineFileState;
using log_agent_detail::LooksLikeManagedFile;
using log_agent_detail::NowMicroseconds;
using log_agent_detail::ParseFileName;
using log_agent_detail::ParsedFileName;

LogAgentResult LogAgent::RunScanLocked() {
    state_.files.clear();
    state_.abnormal_sessions.clear();
    state_.stats = {};

    std::error_code ec;
    if (!std::filesystem::exists(root_dir_, ec)) {
        state_.schedule_state.last_scan_time_us = NowMicroseconds();
        return {true, "root directory does not exist yet", 0};
    }

    std::size_t abnormal_count = 0;
    for (const auto& entry :
         std::filesystem::recursive_directory_iterator(root_dir_, ec)) {
        if (ec) {
            break;
        }
        std::error_code entry_ec;
        if (!entry.is_regular_file(entry_ec)) {
            continue;
        }

        const auto path = entry.path();
        if (!LooksLikeManagedFile(path)) {
            continue;
        }

        ParsedFileName parsed;
        if (!ParseFileName(path, &parsed)) {
            state_.abnormal_sessions.push_back(
                {path, BuildSessionId(path, 0), "invalid_name",
                 "failed to parse managed file name",
                 NowMicroseconds(), false});
            ++abnormal_count;
            continue;
        }

        bool abnormal_marker = !parsed.has_end_time;
        std::uintmax_t size_bytes = 0;
        size_bytes = std::filesystem::file_size(path, entry_ec);
        if (entry_ec) {
            size_bytes = 0;
        }

        const auto relative =
            std::filesystem::relative(path.parent_path(), root_dir_, entry_ec);
        std::string file_type;
        if (!entry_ec && !relative.empty() && relative.native() != ".") {
            file_type = relative.generic_string();
        } else {
            file_type = path.parent_path().filename().string();
        }

        std::string file_state = DetermineFileState(parsed, abnormal_marker);
        const auto failure_it = compress_failures_.find(path.string());
        if (failure_it != compress_failures_.end() &&
            failure_it->second >= policy_.compress_retry_limit) {
            abnormal_marker = true;
            file_state = kAbnormalState;
            state_.abnormal_sessions.push_back(
                {path,
                 BuildSessionId(path, parsed.start_time_us),
                 "compress_failed",
                 "compression retry limit reached",
                 NowMicroseconds(),
                 false});
            ++abnormal_count;
        }

        state_.files.push_back(
            {path,
             file_type,
             file_state,
             parsed.start_time_us,
             parsed.has_end_time ? parsed.end_time_us : 0,
             static_cast<std::size_t>(size_bytes)});

        ++state_.stats.total_files;
        state_.stats.total_size_bytes += static_cast<std::uint64_t>(size_bytes);
        if (file_state == kActiveState) {
            ++state_.stats.active_files;
        } else if (file_state == kSealedState) {
            ++state_.stats.sealed_files;
        } else if (file_state == kCompressedState) {
            ++state_.stats.compressed_files;
        } else if (file_state == kAbnormalState) {
            ++state_.stats.abnormal_files;
        }

        if (abnormal_marker) {
            state_.abnormal_sessions.push_back(
                {path,
                 BuildSessionId(path, parsed.start_time_us),
                 "unsealed_file",
                 "file name only contains start time",
                 NowMicroseconds(),
                 false});
            ++abnormal_count;
        }
    }

    state_.schedule_state.last_scan_time_us = NowMicroseconds();
    std::sort(state_.files.begin(),
              state_.files.end(),
              [](const LogFileEntry& lhs, const LogFileEntry& rhs) {
                  return lhs.path < rhs.path;
              });
    return {true, abnormal_count == 0 ? "scan completed"
                                      : "scan completed with abnormal files",
            state_.files.size()};
}

std::int64_t log_agent_detail::NowMicroseconds() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

std::int64_t log_agent_detail::ParseTimestamp(const std::string& text) {
    std::tm tm_buf{};
    std::istringstream stream(text);
    stream >> std::get_time(&tm_buf, "%Y%m%d_%H%M%S");
    if (stream.fail()) {
        return 0;
    }
    return static_cast<std::int64_t>(std::mktime(&tm_buf)) * 1000000LL;
}

std::string log_agent_detail::FormatTimestamp(std::int64_t time_us) {
    const auto time = static_cast<std::time_t>(time_us / 1000000LL);
    std::tm tm_buf{};
    localtime_r(&time, &tm_buf);
    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%Y%m%d_%H%M%S");
    return oss.str();
}

std::string log_agent_detail::DetermineFileState(const ParsedFileName& parsed,
                                                 bool abnormal_marker) {
    if (abnormal_marker) {
        return kAbnormalState;
    }
    if (parsed.compressed) {
        return kCompressedState;
    }
    return kSealedState;
}

bool log_agent_detail::ParseFileName(const std::filesystem::path& path,
                                     ParsedFileName* parsed) {
    if (parsed == nullptr) {
        return false;
    }

    std::string name = path.filename().string();
    parsed->compressed = false;
    if (name.size() > 3 && name.substr(name.size() - 3) == kCompressedSuffix) {
        parsed->compressed = true;
        name.resize(name.size() - 3);
    }

    const auto dot_pos = name.rfind('.');
    if (dot_pos == std::string::npos) {
        return false;
    }
    parsed->suffix = name.substr(dot_pos + 1);
    name.resize(dot_pos);

    const auto dash_pos = name.find('-');
    if (dash_pos == std::string::npos) {
        return false;
    }

    const auto start_text = name.substr(0, dash_pos);
    const auto end_text = name.substr(dash_pos + 1);
    parsed->start_time_us = ParseTimestamp(start_text);
    if (parsed->start_time_us <= 0) {
        return false;
    }

    if (end_text.empty()) {
        parsed->has_end_time = false;
        parsed->end_time_us = 0;
        return true;
    }

    parsed->end_time_us = ParseTimestamp(end_text);
    parsed->has_end_time = parsed->end_time_us > 0;
    return parsed->has_end_time;
}

std::string log_agent_detail::BuildSessionId(const std::filesystem::path& path,
                                             std::int64_t start_time_us) {
    if (start_time_us > 0) {
        return "session_" + FormatTimestamp(start_time_us);
    }
    return "session_" + path.parent_path().filename().string();
}

bool log_agent_detail::LooksLikeManagedFile(const std::filesystem::path& path) {
    const auto name = path.filename().string();
    return name.find('-') != std::string::npos && name.find('.') != std::string::npos;
}

}  // namespace naviai::log
