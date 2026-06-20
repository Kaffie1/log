#include "log_agent.hpp"
#include "log_agent_cleanup.hpp"
#include "log_agent_scan.hpp"

#include <system_error>

namespace naviai::log {
namespace {

}  // namespace

using log_agent_detail::NowMicroseconds;
using log_agent_detail::ShouldCleanupFile;

LogAgentResult LogAgent::RunCleanupLocked(bool dry_run) {
    const auto now_us = NowMicroseconds();
    const auto cutoff_us = now_us - policy_.retention_window_seconds * 1000000LL;
    std::size_t affected = 0;

    for (const auto& file : state_.files) {
        if (!ShouldCleanupFile(file, cutoff_us)) {
            continue;
        }

        if (!dry_run) {
            std::error_code ec;
            std::filesystem::remove(file.path, ec);
            if (ec) {
                continue;
            }
        }
        ++affected;
    }

    state_.schedule_state.last_cleanup_time_us = now_us;
    if (!dry_run) {
        RunScanLocked();
    }
    return {true, dry_run ? "cleanup dry-run completed" : "cleanup completed", affected};
}

bool log_agent_detail::ShouldCleanupFile(const LogFileEntry& file, std::int64_t cutoff_us) {
    return file.file_state != "active" && file.file_state != "abnormal" &&
           file.end_time_us > 0 && file.end_time_us <= cutoff_us;
}

}  // namespace naviai::log
