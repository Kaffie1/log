#include "log_agent.hpp"

#include <fstream>
#include <sstream>
#include <system_error>

namespace naviai::log {
namespace {

constexpr char kCompressedSuffix[] = ".gz";

bool IsIndexLike(const std::filesystem::path& path) {
    const auto name = path.filename().string();
    return name.size() >= 4 &&
           (name.rfind(".idx") != std::string::npos ||
            name.rfind(".idx.gz") != std::string::npos);
}

}  // namespace

LogAgentResult LogAgent::RunRecoverLocked() {
    std::size_t recovered = 0;

    for (auto& abnormal : state_.abnormal_sessions) {
        if (abnormal.abnormal_type != "unsealed_file") {
            continue;
        }

        ParsedFileName parsed;
        if (!ParseFileName(abnormal.path, &parsed) || parsed.has_end_time) {
            continue;
        }

        std::int64_t end_time_us = 0;
        bool resolved = false;
        if (IsIndexLike(abnormal.path)) {
            resolved = ReadLastTimestampFromIndexFile(abnormal.path, &end_time_us);
        } else {
            resolved = ReadLastTimestampFromTextFile(abnormal.path, &end_time_us);
        }
        if (!resolved) {
            end_time_us = parsed.start_time_us;
        }
        if (end_time_us < parsed.start_time_us) {
            end_time_us = parsed.start_time_us;
        }

        const auto recovered_path =
            BuildRecoveredPath(abnormal.path, parsed.start_time_us, end_time_us);
        std::error_code ec;
        std::filesystem::rename(abnormal.path, recovered_path, ec);
        if (ec) {
            abnormal.detected_reason = "failed to rename abnormal file: " + ec.message();
            abnormal.recovered = false;
            continue;
        }

        abnormal.path = recovered_path;
        abnormal.detected_reason = "recovered by filling end time";
        abnormal.recovered = true;
        ++recovered;
    }

    state_.schedule_state.last_recovery_time_us = NowMicroseconds();
    RunScanLocked();
    return {true, "recovery completed", recovered};
}

bool LogAgent::ReadLastTimestampFromTextFile(const std::filesystem::path& path,
                                             std::int64_t* time_us) {
    if (time_us == nullptr) {
        return false;
    }

    std::ifstream input(path);
    if (!input.is_open()) {
        return false;
    }

    std::string line;
    std::string last_non_empty;
    while (std::getline(input, line)) {
        if (!line.empty()) {
            last_non_empty = line;
        }
    }
    if (last_non_empty.empty()) {
        return false;
    }

    std::int64_t candidate = 0;
    for (std::size_t i = 0; i + 17 <= last_non_empty.size(); ++i) {
        const auto token = last_non_empty.substr(i, 17);
        candidate = ParseTimestamp(token);
        if (candidate > 0) {
            *time_us = candidate;
            return true;
        }
    }
    return false;
}

bool LogAgent::ReadLastTimestampFromIndexFile(const std::filesystem::path& path,
                                              std::int64_t* time_us) {
    if (time_us == nullptr) {
        return false;
    }

    std::ifstream input(path);
    if (!input.is_open()) {
        return false;
    }

    std::string line;
    std::string last_non_empty;
    while (std::getline(input, line)) {
        if (!line.empty()) {
            last_non_empty = line;
        }
    }
    if (last_non_empty.empty()) {
        return false;
    }

    std::istringstream iss(last_non_empty);
    std::vector<std::string> fields;
    std::string field;
    while (std::getline(iss, field, '\t')) {
        fields.push_back(field);
    }
    if (fields.size() < 3) {
        return false;
    }

    try {
        *time_us = std::stoll(fields[2]);
        return *time_us > 0;
    } catch (...) {
        return false;
    }
}

std::filesystem::path LogAgent::BuildRecoveredPath(
    const std::filesystem::path& path,
    std::int64_t start_time_us,
    std::int64_t end_time_us) const {
    std::string name = path.filename().string();
    const bool compressed =
        name.size() > 3 && name.substr(name.size() - 3) == kCompressedSuffix;
    if (compressed) {
        name.resize(name.size() - 3);
    }

    const auto dot_pos = name.rfind('.');
    const auto extension =
        dot_pos == std::string::npos ? std::string() : name.substr(dot_pos);
    std::string recovered =
        FormatTimestamp(start_time_us) + "-" + FormatTimestamp(end_time_us) + extension;
    if (compressed) {
        recovered += kCompressedSuffix;
    }
    return path.parent_path() / recovered;
}

}  // namespace naviai::log
