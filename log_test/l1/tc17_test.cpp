#include "l1_support.hpp"

int main() {
    constexpr int kCaseId = 17;
    auto root = naviai::log::test::l1::DefaultRootForCase(kCaseId);
    naviai::log::test::l1::RemoveRoot(root);

    auto options = naviai::log::test::l1::BuildOptions(root);
    options.enable_runtime_agent = true;
    naviai::log::l1::L1::Init(options);
    naviai::log::l1::L1::Write(naviai::log::l1::MonitorModule::Resource,
                               naviai::log::LogLevel::Warn,
                               "disk alarm");
    naviai::log::l1::L1::Write(naviai::log::l1::MonitorModule::Health,
                               naviai::log::LogLevel::Info,
                               "heartbeat");
    naviai::log::l1::L1::Shutdown();

    int exit_code = 0;
    const auto output = naviai::log::test::l1::RunCommandCapture(
        "cd /workspaces/log && ./build_l1/log_cli/log_cli query --root " +
            root.string() + " --module RESOURCE --level WARN",
        &exit_code);

    const auto inspection = naviai::log::test::l1::InspectRoot(root);
    const bool passed =
        exit_code == 0 &&
        output.find("success=true") != std::string::npos &&
        output.find("total_files=1") != std::string::npos;
    return naviai::log::test::l1::ReportResult(
        kCaseId, passed, "cli query on gz by module and level", inspection,
        passed ? "" : "cli gz query filtering failed");
}
