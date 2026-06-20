#include "log_agent.hpp"

#include <system_error>

namespace naviai::log {
namespace {

constexpr char kActiveState[] = "active";
constexpr char kAbnormalState[] = "abnormal";

}  // namespace

LogAgentResult LogAgent::RunCleanupLocked(bool dry_run) {
    const auto now_us = NowMicroseconds();
    const auto cutoff_us = now_us - policy_.retention_window_seconds * 1000000LL;
    std::size_t affected = 0;

    for (const auto& file : state_.files) {
        if (file.file_state == kActiveState || file.file_state == kAbnormalState ||
            file.end_time_us <= 0 || file.end_time_us > cutoff_us) {
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

}  // namespace naviai::log
