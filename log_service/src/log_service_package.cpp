#include "log_service_naming.hpp"

#include <algorithm>
#include <fstream>
#include <system_error>

#include <zlib.h>

namespace naviai::log {
namespace {

std::string BuildTaskId(const std::string& prefix, std::int64_t now_us) {
    return prefix + "_" + std::to_string(now_us);
}

std::string QuoteOrAny(const std::string& value) {
    return value.empty() ? "any" : value;
}

}  // namespace

PackagePreparationResult LogService::PrepareFilesForPackaging(
    const std::vector<std::filesystem::path>& files) const {
    PackagePreparationResult result;
    result.message = "package preparation failed";
    result.prepared_files = files;

    for (const auto& file : files) {
        if (IsActiveFilePath(file)) {
            result.has_active_files = true;
            result.message = "active files must be sealed before packaging";
            return result;
        }
    }

    for (auto& file : result.prepared_files) {
        if (file.extension() == ".gz") {
            continue;
        }
        if (!CompressFileToGzip(file)) {
            result.message = "failed to compress file before packaging: " +
                             file.string();
            return result;
        }
        file += ".gz";
    }

    result.success = true;
    result.message = "package preparation completed";
    return result;
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

bool LogService::CompressFileToGzip(const std::filesystem::path& source_path) {
    std::error_code ec;
    if (source_path.empty() || !std::filesystem::exists(source_path, ec) ||
        source_path.extension() == ".gz") {
        return false;
    }

    std::ifstream input(source_path, std::ios::binary);
    if (!input.is_open()) {
        return false;
    }

    const auto gzip_path = source_path.string() + ".gz";
    gzFile output = gzopen(gzip_path.c_str(), "wb");
    if (output == nullptr) {
        return false;
    }

    char buffer[8192];
    bool success = true;
    while (input.good()) {
        input.read(buffer, sizeof(buffer));
        const auto bytes = static_cast<unsigned int>(input.gcount());
        if (bytes > 0 && gzwrite(output, buffer, bytes) != static_cast<int>(bytes)) {
            success = false;
            break;
        }
    }

    gzclose(output);
    input.close();
    if (!success) {
        std::filesystem::remove(gzip_path, ec);
        return false;
    }
    std::filesystem::remove(source_path, ec);
    return !ec;
}

}  // namespace naviai::log
