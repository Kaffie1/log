#include "log_service_naming.hpp"

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <system_error>

namespace naviai::log {
namespace {

std::string TrimLeadingDot(std::string suffix) {
    if (!suffix.empty() && suffix.front() == '.') {
        suffix.erase(suffix.begin());
    }
    return suffix;
}

bool ContainsPathTraversal(const std::string& value) {
    return value.find("..") != std::string::npos;
}

}  // namespace

LogService::LogService(std::filesystem::path root_dir, LogServicePolicy policy)
    : root_dir_(std::move(root_dir)), policy_(policy) {}

std::optional<FileNamingPlan> LogService::BuildActiveFilePlan(
    const std::string& file_type,
    const std::string& file_suffix,
    std::optional<std::int64_t> start_time_us) const {
    const auto normalized_file_type = NormalizeFileType(file_type);
    if (!IsFileTypeSafe(normalized_file_type) || file_suffix.empty()) {
        return std::nullopt;
    }

    FileNamingPlan plan;
    plan.file_type = normalized_file_type;
    plan.file_suffix = TrimLeadingDot(file_suffix);
    plan.start_time_us = std::max<std::int64_t>(
        start_time_us.value_or(NowMicroseconds()), 1);
    plan.path = BuildActivePath(plan.file_type, plan.file_suffix, plan.start_time_us);
    plan.sealed = false;
    return plan;
}

std::optional<FileNamingPlan> LogService::BuildSealedFilePlan(
    const std::string& file_type,
    const std::string& file_suffix,
    std::int64_t start_time_us,
    std::optional<std::int64_t> end_time_us) const {
    const auto normalized_file_type = NormalizeFileType(file_type);
    if (!IsFileTypeSafe(normalized_file_type) || file_suffix.empty() ||
        start_time_us <= 0) {
        return std::nullopt;
    }

    FileNamingPlan plan;
    plan.file_type = normalized_file_type;
    plan.file_suffix = TrimLeadingDot(file_suffix);
    plan.start_time_us = start_time_us;
    plan.end_time_us =
        std::max(end_time_us.value_or(NowMicroseconds()), start_time_us);
    plan.path = BuildSealedPath(
        plan.file_type, plan.file_suffix, plan.start_time_us, plan.end_time_us);
    plan.sealed = true;
    return plan;
}

std::optional<FileNamingPlan> LogService::BuildSealedFilePlanFromActivePath(
    const std::filesystem::path& active_path,
    std::optional<std::int64_t> end_time_us) const {
    auto parsed = ParseFileNamingPlan(active_path);
    if (!parsed.has_value() || parsed->sealed) {
        return std::nullopt;
    }
    return BuildSealedFilePlan(parsed->file_type,
                               parsed->file_suffix,
                               parsed->start_time_us,
                               end_time_us);
}

bool LogService::IsActiveFilePath(const std::filesystem::path& path) const {
    const auto parsed = ParseFileNamingPlan(path);
    return parsed.has_value() && !parsed->sealed;
}

LogServiceState LogService::GetState() const {
    return {policy_};
}

std::string LogService::FormatTimestamp(std::int64_t time_us) {
    const auto time_t_value = static_cast<std::time_t>(time_us / 1000000);
    std::tm tm_value {};
#if defined(_WIN32)
    localtime_s(&tm_value, &time_t_value);
#else
    localtime_r(&time_t_value, &tm_value);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm_value, "%Y%m%d_%H%M%S");
    return oss.str();
}

std::int64_t LogService::NowMicroseconds() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

std::string LogService::BuildFileName(std::int64_t start_time_us,
                                      std::optional<std::int64_t> end_time_us,
                                      const std::string& suffix) {
    std::ostringstream oss;
    oss << FormatTimestamp(start_time_us) << "-";
    if (end_time_us.has_value()) {
        oss << FormatTimestamp(*end_time_us);
    }
    oss << "." << TrimLeadingDot(suffix);
    return oss.str();
}

std::string LogService::NormalizeFileType(const std::string& file_type) {
    std::filesystem::path normalized(file_type);
    return normalized.lexically_normal().generic_string();
}

bool LogService::IsFileTypeSafe(const std::string& file_type) {
    if (file_type.empty()) {
        return false;
    }

    const std::filesystem::path value(file_type);
    if (value.is_absolute()) {
        return false;
    }

    const auto normalized = NormalizeFileType(file_type);
    if (normalized.empty() || normalized == "." || normalized.front() == '/') {
        return false;
    }

    return !ContainsPathTraversal(normalized);
}

std::optional<std::pair<std::int64_t, std::int64_t>> LogService::ParseTimeRange(
    const std::filesystem::path& path) {
    std::string name = path.filename().string();
    while (true) {
        const auto dot_pos = name.rfind('.');
        if (dot_pos == std::string::npos) {
            break;
        }
        name.resize(dot_pos);
    }
    const auto delimiter = name.find('-');
    if (delimiter == std::string::npos) {
        return std::nullopt;
    }

    auto parse_part = [](const std::string& value) -> std::optional<std::int64_t> {
        if (value.size() < 15) {
            return std::nullopt;
        }
        const auto candidate = value.substr(0, 15);
        std::tm tm_value {};
        std::istringstream iss(candidate);
        iss >> std::get_time(&tm_value, "%Y%m%d_%H%M%S");
        if (iss.fail()) {
            return std::nullopt;
        }
        return static_cast<std::int64_t>(std::mktime(&tm_value)) * 1000000;
    };

    const auto start = parse_part(name.substr(0, delimiter));
    if (!start.has_value()) {
        return std::nullopt;
    }
    const auto end_string = name.substr(delimiter + 1);
    if (end_string.empty()) {
        return std::make_pair(*start, std::numeric_limits<std::int64_t>::max());
    }
    const auto end = parse_part(end_string);
    if (!end.has_value()) {
        return std::nullopt;
    }
    return std::make_pair(*start, *end);
}

std::string LogService::NormalizeSuffix(const std::filesystem::path& path) {
    const auto name = path.filename().string();
    const auto dot_pos = name.find('.');
    if (dot_pos == std::string::npos || dot_pos + 1 >= name.size()) {
        return {};
    }
    return name.substr(dot_pos + 1);
}

std::filesystem::path LogService::BuildDirectoryPath(const std::string& file_type) const {
    return root_dir_ / NormalizeFileType(file_type);
}

std::filesystem::path LogService::BuildActivePath(const std::string& file_type,
                                                  const std::string& suffix,
                                                  std::int64_t start_time_us) const {
    return BuildDirectoryPath(file_type) /
           BuildFileName(start_time_us, std::nullopt, suffix);
}

std::filesystem::path LogService::BuildSealedPath(const std::string& file_type,
                                                  const std::string& suffix,
                                                  std::int64_t start_time_us,
                                                  std::int64_t end_time_us) const {
    return BuildDirectoryPath(file_type) /
           BuildFileName(start_time_us, end_time_us, suffix);
}

std::optional<FileNamingPlan> LogService::ParseFileNamingPlan(
    const std::filesystem::path& path) const {
    const auto time_range = ParseTimeRange(path);
    if (!time_range.has_value()) {
        return std::nullopt;
    }

    std::error_code ec;
    const auto relative_parent = std::filesystem::relative(path.parent_path(), root_dir_, ec);
    if (ec) {
        return std::nullopt;
    }

    FileNamingPlan plan;
    plan.path = path;
    plan.file_type = relative_parent.empty() || relative_parent == "."
                         ? std::string()
                         : relative_parent.generic_string();
    plan.file_suffix = NormalizeSuffix(path);
    plan.start_time_us = time_range->first;
    plan.sealed = time_range->second != std::numeric_limits<std::int64_t>::max();
    plan.end_time_us = plan.sealed ? time_range->second : 0;
    return plan;
}

void LogService::EnsureDirectory(const std::filesystem::path& path) const {
    std::error_code ec;
    std::filesystem::create_directories(path, ec);
    if (ec) {
        throw std::runtime_error("failed to create directory: " + ec.message());
    }
}

}  // namespace naviai::log
