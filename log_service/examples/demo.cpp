#include "log_service.hpp"

#include <iostream>

int main() {
    naviai::log::LogService service("/tmp/log_service_demo");

    naviai::log::LoggerConfig base_config;
    base_config.level = naviai::log::LogLevel::Debug;
    base_config.enable_console_sink = true;

    auto create_result =
        service.CreateActiveFileAndActivateWriter("navigation", "log", base_config);
    std::cout << "create_and_activate: " << create_result.success << " "
              << create_result.path.string() << '\n';

    auto write_result =
        service.WriteLog("LOCALIZATION", naviai::log::LogLevel::Info, "planner start");
    std::cout << "write: " << write_result.success << " "
              << write_result.message << '\n';

    auto writer_config = service.BuildWriterConfig();
    std::cout << "writer_config: " << static_cast<bool>(writer_config) << '\n';
    if (writer_config.has_value()) {
        std::cout << "writer_root: " << writer_config->root_dir
                  << " file: " << writer_config->file_name << '\n';
    }

    auto switch_result = service.SwitchSegmentAndActivateWriter(base_config);
    std::cout << "switch_and_activate: " << switch_result.success << " "
              << switch_result.path.string() << '\n';

    auto write_result_after_switch =
        service.WriteLog("LOCALIZATION", naviai::log::LogLevel::Warn, "planner tick");
    std::cout << "write_after_switch: " << write_result_after_switch.success << " "
              << write_result_after_switch.message << '\n';

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
