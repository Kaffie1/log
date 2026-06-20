#include "log_service.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <limits>
#include <mutex>
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

bool HasMatchingSuffix(const std::filesystem::path& path,
                       const std::string& suffix) {
    if (suffix.empty()) {
        return true;
    }
    return path.extension() == ("." + TrimLeadingDot(suffix));
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

}  // namespace

LogService::LogService(std::filesystem::path root_dir, LogServicePolicy policy)
    : root_dir_(std::move(root_dir)), policy_(policy) {}

OperationResult LogService::CreateActiveFile(const std::string& file_type,
                                             const std::string& file_suffix) {
    std::lock_guard<std::mutex> lock(mutex_);
    return CreateActiveFileLocked(NormalizeFileType(file_type), file_suffix);
}

OperationResult LogService::CreateActiveFileAndActivateWriter(
    const std::string& file_type,
    const std::string& file_suffix,
    const LoggerConfig& base_config) {
    std::unique_lock<std::mutex> lock(mutex_);
    const auto normalized_file_type = NormalizeFileType(file_type);

    auto activation_guard = PrepareWriterActivationLocked(normalized_file_type);
    if (!activation_guard.success) {
        return activation_guard;
    }

    auto create_result =
        CreateActiveFileLocked(normalized_file_type, file_suffix);
    if (!create_result.success) {
        return create_result;
    }

    auto config = BuildWriterConfigLocked(base_config);
    if (!config.has_value()) {
        return {false, "active session not found", {}};
    }
    writer_base_config_ = base_config;
    auto* session = FindActiveSessionLocked();
    if (session != nullptr) {
        session->switching = true;
    }

    lock.unlock();
    OperationResult activate_result;
    try {
        LogManager::Init(*config);
        activate_result = {true,
                           "writer activated",
                           std::filesystem::path(config->root_dir) /
                               config->file_name};
    } catch (const std::exception& e) {
        activate_result = {false, e.what(), config->root_dir};
    } catch (...) {
        activate_result = {false, "failed to activate writer", config->root_dir};
    }
    lock.lock();
    if (!activate_result.success) {
        session = FindActiveSessionLocked();
        if (session != nullptr) {
            session->switching = false;
        }
        switch_cv_.notify_all();
        return activate_result;
    }
    auto flush_result = FlushBufferedLogsLocked();
    if (!flush_result.success) {
        flush_result.message += ", writer remains in switching state";
        return flush_result;
    }
    writer_activated_ = true;
    session = FindActiveSessionLocked();
    if (session != nullptr) {
        session->switching = false;
    }
    switch_cv_.notify_all();
    return activate_result;
}

OperationResult LogService::CreateActiveFileLocked(const std::string& file_type,
                                                   const std::string& file_suffix) {
    if (file_type.empty() || file_suffix.empty()) {
        return {false, "file_type and file_suffix must not be empty", {}};
    }
    if (!IsFileTypeSafe(file_type)) {
        return {false, "file_type contains unsupported path pattern", {}};
    }
    if (active_session_.has_value()) {
        return {false, "active session already exists for this service instance", {}};
    }

    const auto normalized_file_type = NormalizeFileType(file_type);
    const auto start_time_us = NowMicroseconds();
    const auto path =
        BuildActivePath(normalized_file_type, file_suffix, start_time_us);
    EnsureDirectory(path.parent_path());

    std::ofstream stream(path, std::ios::app);
    if (!stream.is_open()) {
        return {false, "failed to create active file", path};
    }

    active_session_ =
        ActiveFileSession{path, normalized_file_type, file_suffix, start_time_us, false, 0};
    return {true, "active file created", path};
}

OperationResult LogService::SwitchSegment() {
    std::unique_lock<std::mutex> lock(mutex_);

    if (writer_base_config_.has_value()) {
        LoggerConfig base_config = *writer_base_config_;
        lock.unlock();
        return SwitchSegmentAndActivateWriter(base_config);
    }

    auto* session = FindActiveSessionLocked();
    if (session == nullptr) {
        return {false, "active session not found", {}};
    }
    if (session->switching) {
        return {false, "session is already switching", session->active_path};
    }

    const auto file_suffix = session->file_suffix;
    const auto active_file_type = session->file_type;
    session->switching = true;
    auto seal_result = SealFileLocked(std::nullopt);
    if (!seal_result.success) {
        auto* rollback_session = FindActiveSessionLocked();
        if (rollback_session != nullptr) {
            rollback_session->switching = false;
        }
        switch_cv_.notify_all();
        return seal_result;
    }

    auto create_result = CreateActiveFileLocked(active_file_type, file_suffix);
    switch_cv_.notify_all();
    return create_result;
}

OperationResult LogService::SwitchSegmentAndActivateWriter(
    const LoggerConfig& base_config) {
    std::unique_lock<std::mutex> lock(mutex_);

    auto* session = FindActiveSessionLocked();
    if (session == nullptr) {
        return {false, "active session not found", {}};
    }
    if (session->switching) {
        return {false, "session is already switching", session->active_path};
    }

    const auto file_suffix = session->file_suffix;
    const auto active_file_type = session->file_type;
    session->switching = true;

    lock.unlock();
    FlushWriterSafely();
    lock.lock();

    auto seal_result = SealFileLocked(std::nullopt);
    if (!seal_result.success) {
        auto* rollback_session = FindActiveSessionLocked();
        if (rollback_session != nullptr) {
            rollback_session->switching = false;
        }
        switch_cv_.notify_all();
        return seal_result;
    }

    auto switch_result = CreateActiveFileLocked(active_file_type, file_suffix);
    if (!switch_result.success) {
        switch_cv_.notify_all();
        return switch_result;
    }

    auto* new_session = FindActiveSessionLocked();
    if (new_session == nullptr) {
        switch_cv_.notify_all();
        return {false, "active session not found after segment switch", {}};
    }
    new_session->switching = true;

    auto config = BuildWriterConfigLocked(base_config);
    if (!config.has_value()) {
        new_session->switching = false;
        switch_cv_.notify_all();
        return {false, "active session not found", {}};
    }
    writer_base_config_ = base_config;

    lock.unlock();
    OperationResult activate_result;
    try {
        LogManager::Init(*config);
        activate_result = {true,
                           "writer activated",
                           std::filesystem::path(config->root_dir) /
                               config->file_name};
    } catch (const std::exception& e) {
        activate_result = {false, e.what(), config->root_dir};
    } catch (...) {
        activate_result = {false, "failed to activate writer", config->root_dir};
    }
    lock.lock();

    auto* activated_session = FindActiveSessionLocked();
    if (!activate_result.success) {
        if (activated_session != nullptr) {
            activated_session->switching = false;
        }
        switch_cv_.notify_all();
        return activate_result;
    }

    auto flush_result = FlushBufferedLogsLocked();
    if (!flush_result.success) {
        flush_result.message += ", writer remains in switching state";
        return flush_result;
    }
    writer_activated_ = true;
    if (activated_session != nullptr) {
        activated_session->switching = false;
    }
    switch_cv_.notify_all();
    return activate_result;
}

OperationResult LogService::SealFile() {
    std::lock_guard<std::mutex> lock(mutex_);
    FlushWriterSafely();
    auto result = SealFileLocked(std::nullopt);
    if (result.success) {
        writer_activated_ = false;
        writer_base_config_.reset();
    }
    switch_cv_.notify_all();
    return result;
}

OperationResult LogService::ActivateWriter(const LoggerConfig& base_config) {
    std::unique_lock<std::mutex> lock(mutex_);
    auto* current_session = FindActiveSessionLocked();
    if (current_session == nullptr) {
        return {false, "active session not found", {}};
    }
    auto activation_guard = PrepareWriterActivationLocked(current_session->file_type);
    if (!activation_guard.success) {
        return activation_guard;
    }
    auto config = BuildWriterConfigLocked(base_config);
    if (!config.has_value()) {
        return {false, "active session not found", {}};
    }
    writer_base_config_ = base_config;
    auto* session = FindActiveSessionLocked();
    if (session != nullptr) {
        session->switching = true;
    }

    lock.unlock();
    OperationResult activate_result;
    try {
        LogManager::Init(*config);
        activate_result = {true,
                           "writer activated",
                           std::filesystem::path(config->root_dir) /
                               config->file_name};
    } catch (const std::exception& e) {
        activate_result = {false, e.what(), config->root_dir};
    } catch (...) {
        activate_result = {false, "failed to activate writer", config->root_dir};
    }
    lock.lock();

    session = FindActiveSessionLocked();
    if (!activate_result.success) {
        if (session != nullptr) {
            session->switching = false;
        }
        switch_cv_.notify_all();
        return activate_result;
    }

    auto flush_result = FlushBufferedLogsLocked();
    if (!flush_result.success) {
        flush_result.message += ", writer remains in switching state";
        return flush_result;
    }
    writer_activated_ = true;
    if (session != nullptr) {
        session->switching = false;
    }
    switch_cv_.notify_all();
    return activate_result;
}

OperationResult LogService::WriteLog(const std::string& module_name,
                                     LogLevel level,
                                     const std::string& payload) {
    std::unique_lock<std::mutex> lock(mutex_);

    if (!writer_activated_) {
        return {false,
                "writer is not activated for current active session, activate writer before writing",
                {}};
    }

    while (true) {
        auto* session = FindActiveSessionLocked();
        if (session == nullptr) {
            return {false, "active session not found", {}};
        }

        if (!session->switching) {
            return WriteLogLocked(module_name, level, payload);
        }

        if (session->buffered_records < policy_.switch_buffer_limit) {
            buffered_logs_.push_back({module_name, level, payload});
            ++session->buffered_records;
            return {true, "log buffered during segment switch", session->active_path};
        }

        if (!policy_.block_on_buffer_full) {
            return {false, "switch buffer is full", session->active_path};
        }

        switch_cv_.wait(lock, [&]() {
            auto* pending = FindActiveSessionLocked();
            return pending == nullptr || !pending->switching;
        });
    }
}

QueryResult LogService::QueryLogs(const QueryCondition& condition) {
    std::unique_lock<std::mutex> lock(mutex_);
    QueryCondition normalized_condition = condition;
    if (!normalized_condition.file_type.empty()) {
        normalized_condition.file_type =
            NormalizeFileType(normalized_condition.file_type);
    }

    std::string error_message;
    if (!ValidateQueryCondition(normalized_condition, &error_message)) {
        return {false, error_message, 0, {}};
    }
    FlushWriterSafely();
    auto segment_result = ForceSealIntersectedSessions(lock, normalized_condition);
    if (!segment_result.success) {
        return {false, segment_result.message, 0, {}};
    }
    return QueryLogsLocked(normalized_condition);
}

PackageTask LogService::PackageLogs(const QueryCondition& condition,
                                    const std::filesystem::path& package_root_dir) {
    std::unique_lock<std::mutex> lock(mutex_);
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

    FlushWriterSafely();
    auto segment_result = ForceSealIntersectedSessions(lock, normalized_condition);
    if (!segment_result.success) {
        task.message = segment_result.message;
        return task;
    }
    auto query_result = QueryLogsLocked(normalized_condition);
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

std::optional<LoggerConfig> LogService::BuildWriterConfig(
    const LoggerConfig& base_config) {
    std::lock_guard<std::mutex> lock(mutex_);
    return BuildWriterConfigLocked(base_config);
}

std::optional<LoggerConfig> LogService::BuildWriterConfigLocked(
    const LoggerConfig& base_config) {
    const auto* session = FindActiveSessionLocked();
    if (session == nullptr) {
        return std::nullopt;
    }

    LoggerConfig config = base_config;
    config.root_dir = session->active_path.parent_path().string();
    config.file_name = session->active_path.filename().string();
    writer_base_config_ = base_config;
    return config;
}

std::optional<ActiveFileSession> LogService::GetActiveSession() const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto* session = FindActiveSessionLocked();
    if (session == nullptr) {
        return std::nullopt;
    }
    return *session;
}

LogServiceState LogService::GetState() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return {active_session_, policy_, writer_activated_};
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
        if (value.empty()) {
            return std::nullopt;
        }
        std::tm tm_value {};
        std::istringstream iss(value);
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

ActiveFileSession* LogService::FindActiveSessionLocked() {
    if (!active_session_.has_value()) {
        return nullptr;
    }
    return &(*active_session_);
}

const ActiveFileSession* LogService::FindActiveSessionLocked() const {
    if (!active_session_.has_value()) {
        return nullptr;
    }
    return &(*active_session_);
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

void LogService::FlushWriterSafely() const {
    try {
        LogManager::Flush();
    } catch (...) {
    }
}

OperationResult LogService::WriteLogLocked(const std::string& module_name,
                                           LogLevel level,
                                           const std::string& payload) {
    const auto* session = FindActiveSessionLocked();
    if (session == nullptr) {
        return {false, "active session not found", {}};
    }

    try {
        LogManager::WriteRaw(module_name, level, payload);
    } catch (const std::exception& e) {
        return {false, e.what(), session->active_path};
    } catch (...) {
        return {false, "failed to write log", session->active_path};
    }
    return {true, "log written", session->active_path};
}

OperationResult LogService::FlushBufferedLogsLocked() {
    auto* session = FindActiveSessionLocked();
    if (session == nullptr) {
        return {false, "active session not found", {}};
    }

    for (const auto& entry : buffered_logs_) {
        try {
            LogManager::WriteRaw(entry.module_name, entry.level, entry.payload);
        } catch (const std::exception& e) {
            return {false, e.what(), session->active_path};
        } catch (...) {
            return {false, "failed to flush buffered logs", session->active_path};
        }
    }

    buffered_logs_.clear();
    session->buffered_records = 0;
    return {true, "buffered logs flushed", session->active_path};
}

QueryResult LogService::QueryLogsLocked(const QueryCondition& condition) const {
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

OperationResult LogService::ForceSealIntersectedSessions(
    std::unique_lock<std::mutex>& lock, const QueryCondition& condition) {
    const auto effective_end = condition.end_time_us == 0
                                   ? std::numeric_limits<std::int64_t>::max()
                                   : condition.end_time_us;
    if (!active_session_.has_value()) {
        return {true, "intersected sessions processed", {}};
    }
    if (!condition.file_type.empty() && active_session_->file_type != condition.file_type) {
        return {true, "intersected sessions processed", {}};
    }
    if (!Intersects(active_session_->start_time_us,
                    std::numeric_limits<std::int64_t>::max(),
                    condition.start_time_us,
                    effective_end)) {
        return {true, "intersected sessions processed", {}};
    }
    if (writer_base_config_.has_value()) {
        LoggerConfig base_config = *writer_base_config_;
        lock.unlock();
        auto switch_result = SwitchSegmentAndActivateWriter(base_config);
        lock.lock();
        if (!switch_result.success) {
            return switch_result;
        }
        return {true, "intersected sessions processed", {}};
    }

    auto seal_result = SealFileLocked(std::nullopt);
    if (!seal_result.success) {
        return seal_result;
    }
    return {true, "intersected sessions processed", {}};
}

OperationResult LogService::PrepareWriterActivationLocked(
    const std::string& file_type) {
    if (!active_session_.has_value()) {
        return {true, "writer activation allowed", {}};
    }
    if (active_session_->file_type != file_type) {
        return {false,
                "service instance is bound to file_type '" + active_session_->file_type +
                    "', create another LogService process or instance for a different file_type",
                {}};
    }
    if (!writer_activated_) {
        return {true, "writer activation allowed", {}};
    }

    if (active_session_->file_type == file_type) {
        return {true, "writer activation allowed", {}};
    }

    return {false, "writer activation rejected", {}};
}

OperationResult LogService::SealFileLocked(
    std::optional<std::int64_t> explicit_end_time) {
    auto* session = FindActiveSessionLocked();
    if (session == nullptr) {
        return {false, "active session not found", {}};
    }

    const auto end_time_us =
        std::max(explicit_end_time.value_or(NowMicroseconds()), session->start_time_us);
    const auto sealed_path = BuildSealedPath(
        session->file_type, session->file_suffix, session->start_time_us, end_time_us);

    std::error_code ec;
    std::filesystem::rename(session->active_path, sealed_path, ec);
    if (ec) {
        return {false, ec.message(), sealed_path};
    }

    active_session_.reset();

    return {true, "file sealed", sealed_path};
}

}  // namespace naviai::log
