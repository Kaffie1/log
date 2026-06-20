#include "log_service_naming.hpp"

#include <sstream>
#include <system_error>
#include <unordered_set>

namespace naviai::log {
namespace {

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

}  // namespace naviai::log
