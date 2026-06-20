#include "log_service.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <unordered_set>

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

std::string BuildTaskId(const std::string& prefix, std::int64_t now_us) {
    std::ostringstream oss;
    oss << prefix << "_" << now_us;
    return oss.str();
}

bool ContainsPathTraversal(const std::string& value) {
    return value.find("..") != std::string::npos;
}

std::string QuoteOrAny(const std::string& value) {
    return value.empty() ? "any" : value;
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
    std::ifstream stream(path);
    if (!stream.is_open()) {
        return false;
    }

    const auto text_token = "[" + level_name + "]";
    const auto json_token = "\"level\":\"" + level_name + "\"";

    std::string line;
    while (std::getline(stream, line)) {
        if (line.find(text_token) != std::string::npos ||
            line.find(json_token) != std::string::npos) {
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

    std::ifstream stream(path);
    if (!stream.is_open()) {
        return false;
    }

    const auto text_token = "] [" + module_name + "]";
    const auto json_token = "\"module\":\"" + module_name + "\"";

    std::string line;
    while (std::getline(stream, line)) {
        if (line.find(text_token) != std::string::npos ||
            line.find(json_token) != std::string::npos) {
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

std::filesystem::path BuildRecoveredVariantPath(const std::filesystem::path& path,
                                                std::uint32_t suffix) {
    const auto extension = path.extension().string();
    const auto stem = path.stem().string();
    std::ostringstream oss;
    oss << stem << "_recovered_" << suffix << extension;
    return path.parent_path() / oss.str();
}

bool ArePathsAvailable(const std::vector<std::filesystem::path>& paths) {
    std::error_code ec;
    std::unordered_set<std::string> unique_paths;
    for (const auto& path : paths) {
        const auto normalized = path.lexically_normal().string();
        if (!unique_paths.insert(normalized).second) {
            return false;
        }
        if (std::filesystem::exists(path, ec)) {
            return false;
        }
        ec.clear();
    }
    return true;
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

RecoveryTask LogService::RecoverActiveFiles(
    const std::vector<std::filesystem::path>& active_paths,
    std::int64_t end_time_us) const {
    RecoveryTask task;
    task.source_paths = active_paths;
    task.message = "recovery failed";

    if (active_paths.empty() || end_time_us <= 0) {
        task.message = "active_paths must not be empty and end_time_us must be > 0";
        return task;
    }

    std::vector<std::filesystem::path> target_paths;
    target_paths.reserve(active_paths.size());
    for (const auto& active_path : active_paths) {
        auto plan = BuildSealedFilePlanFromActivePath(active_path, end_time_us);
        if (!plan.has_value()) {
            task.message = "path is not a managed active file: " + active_path.string();
            return task;
        }
        target_paths.push_back(plan->path);
    }

    if (!ArePathsAvailable(target_paths)) {
        bool found_variant = false;
        for (std::uint32_t suffix = 1; suffix < 1000000; ++suffix) {
            std::vector<std::filesystem::path> candidate_paths;
            candidate_paths.reserve(target_paths.size());
            for (const auto& target_path : target_paths) {
                candidate_paths.push_back(BuildRecoveredVariantPath(target_path, suffix));
            }
            if (ArePathsAvailable(candidate_paths)) {
                target_paths = std::move(candidate_paths);
                found_variant = true;
                break;
            }
        }
        if (!found_variant) {
            task.message = "failed to find available recovered target paths";
            return task;
        }
    }

    std::vector<std::pair<std::filesystem::path, std::filesystem::path>> renamed_paths;
    std::error_code ec;
    for (std::size_t index = 0; index < active_paths.size(); ++index) {
        const auto& source_path = active_paths[index];
        const auto& target_path = target_paths[index];
        std::filesystem::rename(source_path, target_path, ec);
        if (ec) {
            for (auto rollback_it = renamed_paths.rbegin();
                 rollback_it != renamed_paths.rend();
                 ++rollback_it) {
                std::error_code rollback_ec;
                std::filesystem::rename(rollback_it->second, rollback_it->first, rollback_ec);
            }
            task.message = "failed to recover active file: " + ec.message();
            return task;
        }
        renamed_paths.push_back({source_path, target_path});
        task.recovered_paths.push_back(target_path);
    }

    task.success = true;
    task.message = "recovery completed";
    return task;
}

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

PackageTask LogService::PackageLogs(const QueryCondition& condition,
                                    const std::filesystem::path& package_root_dir) {
    QueryCondition normalized_condition = condition;
    if (!normalized_condition.file_type.empty()) {
        normalized_condition.file_type =
            NormalizeFileType(normalized_condition.file_type);
    }

    PackageTask task;
    task.task_id = BuildTaskId("package", NowMicroseconds());
    task.condition = normalized_condition;
    task.task_state = "failed";
    task.message = "package task failed";

    if (package_root_dir.empty()) {
        task.message = "package_root_dir must not be empty";
        return task;
    }
    if (!ValidateQueryCondition(normalized_condition, &task.message)) {
        return task;
    }

    auto query_result = QueryLogsImpl(normalized_condition);
    if (!query_result.success) {
        task.message = query_result.message;
        return task;
    }
    if (query_result.files.empty()) {
        task.task_state = "completed";
        task.message = "no files matched query condition";
        return task;
    }

    task.output_path = package_root_dir / task.task_id;
    task.manifest_path = task.output_path / "manifest.txt";
    EnsureDirectory(task.output_path);

    std::ofstream manifest(task.manifest_path, std::ios::trunc);
    if (!manifest.is_open()) {
        task.message = "failed to create manifest";
        return task;
    }

    manifest << "task_id=" << task.task_id << '\n';
    manifest << "file_count=" << query_result.total_files << '\n';
    manifest << "package_root=" << task.output_path.string() << '\n';
    manifest << "query_file_type=" << QuoteOrAny(normalized_condition.file_type)
             << '\n';
    manifest << "query_module_name=" << QuoteOrAny(normalized_condition.module_name)
             << '\n';
    manifest << "query_log_level=" << QuoteOrAny(normalized_condition.log_level)
             << '\n';
    manifest << "query_file_suffix=" << QuoteOrAny(normalized_condition.file_suffix)
             << '\n';
    manifest << "query_start_time_us=" << normalized_condition.start_time_us
             << '\n';
    manifest << "query_end_time_us=" << normalized_condition.end_time_us << '\n';

    for (const auto& file : query_result.files) {
        std::error_code relative_ec;
        auto relative_path =
            std::filesystem::relative(file, root_dir_, relative_ec);
        if (relative_ec || relative_path.empty()) {
            relative_path = file.filename();
        }

        std::error_code ec;
        const auto target = task.output_path / relative_path;
        if (!target.parent_path().empty()) {
            std::filesystem::create_directories(target.parent_path(), ec);
            if (ec) {
                task.message = ec.message();
                return task;
            }
        }
        std::filesystem::copy_file(
            file, target, std::filesystem::copy_options::overwrite_existing, ec);
        if (ec) {
            task.message = ec.message();
            return task;
        }
        task.source_files.push_back(file);
        manifest << relative_path.generic_string() << "=" << file.string() << '\n';
    }

    task.task_state = "completed";
    task.message = "package completed";
    return task;
}

DeliveryTask LogService::ExportPackage(const std::filesystem::path& package_path,
                                       const std::filesystem::path& target_dir) {
    DeliveryTask task;
    task.task_id = BuildTaskId("export", NowMicroseconds());
    task.task_type = "export";
    task.source_path = package_path;
    task.target_path = target_dir;
    task.task_state = "failed";
    task.message = "export task failed";

    if (package_path.empty() || target_dir.empty()) {
        task.message = "package_path and target_dir must not be empty";
        return task;
    }
    if (!std::filesystem::exists(package_path)) {
        task.message = "package_path does not exist";
        task.target_path = package_path;
        return task;
    }

    EnsureDirectory(target_dir);
    const auto destination = target_dir / package_path.filename();
    task.target_path = destination;

    std::error_code ec;
    std::filesystem::remove_all(destination, ec);
    ec.clear();
    std::filesystem::copy(package_path,
                          destination,
                          std::filesystem::copy_options::recursive |
                              std::filesystem::copy_options::overwrite_existing,
                          ec);
    if (ec) {
        std::error_code rollback_ec;
        std::filesystem::remove_all(destination, rollback_ec);
        task.message = ec.message();
        return task;
    }
    task.task_state = "completed";
    task.message = "package exported";
    return task;
}

DeliveryTask LogService::UploadPackage(const std::filesystem::path& package_path,
                                       const std::string& target_uri) {
    DeliveryTask task;
    task.task_id = BuildTaskId("upload", NowMicroseconds());
    task.task_type = "upload";
    task.source_path = package_path;
    task.task_state = "failed";
    task.message = "upload task failed";

    if (package_path.empty() || target_uri.empty()) {
        task.message = "package_path and target_uri must not be empty";
        return task;
    }

    std::filesystem::path target_path;
    if (target_uri.rfind("file://", 0) == 0) {
        target_path = target_uri.substr(7);
    } else if (!target_uri.empty() && target_uri.front() == '/') {
        target_path = target_uri;
    } else {
        task.message =
            "only file:// and absolute local upload targets are supported";
        return task;
    }
    task.target_path = target_path;

    const auto max_attempts = std::max<std::size_t>(1, policy_.upload_retry_limit + 1);
    for (std::size_t attempt = 1; attempt <= max_attempts; ++attempt) {
        auto export_task = ExportPackage(package_path, target_path);
        task.retry_count = attempt;
        if (export_task.task_state == "completed") {
            task.task_state = "completed";
            task.message = "package uploaded in attempt " + std::to_string(attempt);
            task.target_path = export_task.target_path;
            return task;
        }
        task.message = export_task.message;
    }

    task.message +=
        ", retries exhausted after " + std::to_string(max_attempts) + " attempts";
    return task;
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

std::filesystem::path LogService::BuildDirectoryPath(
    const std::string& file_type) const {
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
