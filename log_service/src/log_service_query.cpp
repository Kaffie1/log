#include "log_service_naming.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>

#include <zlib.h>

namespace naviai::log {
namespace {

std::string TrimLeadingDot(std::string suffix) {
    if (!suffix.empty() && suffix.front() == '.') {
        suffix.erase(suffix.begin());
    }
    return suffix;
}

bool HasMatchingSuffix(const std::filesystem::path& path,
                       const std::string& suffix) {
    if (suffix.empty()) {
        return true;
    }
    const auto name = path.filename().string();
    const auto dot_pos = name.find('.');
    if (dot_pos == std::string::npos || dot_pos + 1 >= name.size()) {
        return false;
    }
    return name.substr(dot_pos + 1) == TrimLeadingDot(suffix);
}

bool Intersects(std::int64_t lhs_start,
                std::int64_t lhs_end,
                std::int64_t rhs_start,
                std::int64_t rhs_end) {
    return lhs_start <= rhs_end && rhs_start <= lhs_end;
}

std::string NormalizeLevelName(std::string value) {
    std::transform(value.begin(),
                   value.end(),
                   value.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
    if (value == "ERR") {
        return "ERROR";
    }
    if (value == "WARNING") {
        return "WARN";
    }
    return value;
}

bool MatchesLogLevel(const std::filesystem::path& path,
                     const std::string& raw_level_name) {
    if (raw_level_name.empty()) {
        return true;
    }

    const auto level_name = NormalizeLevelName(raw_level_name);
    const auto text_token = "[" + level_name + "]";
    const auto json_token = "\"level\":\"" + level_name + "\"";
    auto matches_line = [&](const std::string& line) {
        return line.find(text_token) != std::string::npos ||
               line.find(json_token) != std::string::npos;
    };

    if (path.extension() == ".gz") {
        gzFile stream = gzopen(path.string().c_str(), "rb");
        if (stream == nullptr) {
            return false;
        }
        char buffer[4096];
        while (gzgets(stream, buffer, sizeof(buffer)) != nullptr) {
            if (matches_line(buffer)) {
                gzclose(stream);
                return true;
            }
        }
        gzclose(stream);
        return false;
    }

    std::ifstream stream(path);
    if (!stream.is_open()) {
        return false;
    }

    std::string line;
    while (std::getline(stream, line)) {
        if (matches_line(line)) {
            return true;
        }
    }
    return false;
}

bool MatchesModuleName(const std::filesystem::path& path,
                       const std::string& module_name) {
    if (module_name.empty()) {
        return true;
    }

    const auto text_token = "] [" + module_name + "]";
    const auto json_token = "\"module\":\"" + module_name + "\"";
    auto matches_line = [&](const std::string& line) {
        return line.find(text_token) != std::string::npos ||
               line.find(json_token) != std::string::npos;
    };

    if (path.extension() == ".gz") {
        gzFile stream = gzopen(path.string().c_str(), "rb");
        if (stream == nullptr) {
            return false;
        }
        char buffer[4096];
        while (gzgets(stream, buffer, sizeof(buffer)) != nullptr) {
            if (matches_line(buffer)) {
                gzclose(stream);
                return true;
            }
        }
        gzclose(stream);
        return false;
    }

    std::ifstream stream(path);
    if (!stream.is_open()) {
        return false;
    }

    std::string line;
    while (std::getline(stream, line)) {
        if (matches_line(line)) {
            return true;
        }
    }
    return false;
}

bool IsPackagedArtifact(const std::filesystem::path& file_path,
                        const std::filesystem::path& root_dir) {
    std::error_code ec;
    auto current = file_path.parent_path();
    const auto normalized_root = root_dir.lexically_normal();

    while (!current.empty()) {
        if (std::filesystem::exists(current / "manifest.txt", ec)) {
            return true;
        }
        if (ec) {
            return false;
        }
        if (current.lexically_normal() == normalized_root) {
            break;
        }
        auto parent = current.parent_path();
        if (parent == current) {
            break;
        }
        current = parent;
    }
    return false;
}

}  // namespace

QueryResult LogService::QueryLogs(const QueryCondition& condition) {
    QueryCondition normalized_condition = condition;
    if (!normalized_condition.file_type.empty()) {
        normalized_condition.file_type =
            NormalizeFileType(normalized_condition.file_type);
    }

    std::string error_message;
    if (!ValidateQueryCondition(normalized_condition, &error_message)) {
        return {false, error_message, 0, {}};
    }
    return QueryLogsImpl(normalized_condition);
}

bool LogService::ValidateQueryCondition(const QueryCondition& condition,
                                        std::string* error_message) const {
    if (condition.start_time_us < 0 || condition.end_time_us < 0) {
        if (error_message != nullptr) {
            *error_message = "query time must not be negative";
        }
        return false;
    }
    if (condition.end_time_us != 0 &&
        condition.start_time_us > condition.end_time_us) {
        if (error_message != nullptr) {
            *error_message = "start_time_us must not be greater than end_time_us";
        }
        return false;
    }
    if (!condition.file_type.empty() && !IsFileTypeSafe(condition.file_type)) {
        if (error_message != nullptr) {
            *error_message = "file_type contains unsupported path pattern";
        }
        return false;
    }
    return true;
}

QueryResult LogService::QueryLogsImpl(const QueryCondition& condition) const {
    QueryResult result;
    result.success = true;

    std::error_code ec;
    if (!std::filesystem::exists(root_dir_, ec)) {
        result.success = false;
        result.message = "root directory does not exist";
        return result;
    }

    const auto effective_end = condition.end_time_us == 0
                                   ? std::numeric_limits<std::int64_t>::max()
                                   : condition.end_time_us;

    for (const auto& entry :
         std::filesystem::recursive_directory_iterator(root_dir_, ec)) {
        if (ec) {
            result.success = false;
            result.message = ec.message();
            return result;
        }
        if (!entry.is_regular_file()) {
            continue;
        }
        if (IsPackagedArtifact(entry.path(), root_dir_)) {
            continue;
        }
        if (!condition.file_type.empty()) {
            const auto expected_dir = BuildDirectoryPath(condition.file_type);
            const auto parent_dir = entry.path().parent_path().lexically_normal();
            if (parent_dir != expected_dir.lexically_normal()) {
                continue;
            }
        }
        if (!HasMatchingSuffix(entry.path(), condition.file_suffix)) {
            continue;
        }
        if (!MatchesLogLevel(entry.path(), condition.log_level)) {
            continue;
        }
        if (!MatchesModuleName(entry.path(), condition.module_name)) {
            continue;
        }

        const auto time_range = ParseTimeRange(entry.path());
        if (!time_range.has_value()) {
            continue;
        }
        if (!Intersects(time_range->first,
                        time_range->second,
                        condition.start_time_us,
                        effective_end)) {
            continue;
        }
        result.files.push_back(entry.path());
    }

    std::sort(result.files.begin(), result.files.end());
    result.total_files = result.files.size();
    result.message = "query completed";
    return result;
}

}  // namespace naviai::log
