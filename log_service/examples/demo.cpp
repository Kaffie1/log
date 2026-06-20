#include "log_service.hpp"

#include <fstream>
#include <iostream>

int main() {
    naviai::log::LogService service("/tmp/log_service_demo");

    const auto active_plan =
        service.BuildActiveFilePlan("navigation", "log", 1760880000000000LL);
    const auto sealed_plan =
        service.BuildSealedFilePlan("navigation",
                                    "log",
                                    1760880000000000LL,
                                    1760880060000000LL);
    std::cout << "active_plan: " << static_cast<bool>(active_plan) << " "
              << (active_plan ? active_plan->path.string() : "") << '\n';
    std::cout << "sealed_plan: " << static_cast<bool>(sealed_plan) << " "
              << (sealed_plan ? sealed_plan->path.string() : "") << '\n';

    if (sealed_plan.has_value()) {
        std::filesystem::create_directories(sealed_plan->path.parent_path());
        std::ofstream output(sealed_plan->path, std::ios::out | std::ios::trunc);
        output << "[2026-06-20 10:00:00.000000] [INFO] [LOCALIZATION] planner start\n";
        output << "[2026-06-20 10:00:05.000000] [WARN] [LOCALIZATION] planner tick\n";
    }

    naviai::log::QueryCondition condition;
    condition.module_name = "LOCALIZATION";
    condition.log_level = "WARN";
    condition.file_suffix = "log";
    auto query_result = service.QueryLogs(condition);
    std::cout << "query: " << query_result.success
              << " total=" << query_result.total_files << '\n';

    auto package_task = service.PackageLogs(condition, "/tmp/log_service_packages");
    std::cout << "package: " << package_task.task_state << " "
              << package_task.message << '\n';
    std::cout << "package_output: " << package_task.output_path.string() << '\n';
    std::cout << "manifest: " << package_task.manifest_path.string() << '\n';

    return 0;
}
