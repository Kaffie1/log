#include "l1_support.hpp"

int main() {
    constexpr int kCaseId = 14;
    auto root = naviai::log::test::l1::DefaultRootForCase(kCaseId);
    naviai::log::test::l1::RemoveRoot(root);

    auto options = naviai::log::test::l1::BuildOptions(root);
    options.enable_runtime_agent = false;
    naviai::log::l1::L1::Init(options);
    naviai::log::l1::L1::Write({naviai::log::l1::MonitorModule::Process,
                                naviai::log::LogLevel::Error,
                                "planner exited unexpectedly",
                                "process_exit",
                                "restart_count",
                                "3",
                                "1",
                                "planner",
                                1234567890123456});
    naviai::log::l1::L1::Shutdown();

    const auto inspection = naviai::log::test::l1::InspectRoot(root);
    const bool passed =
        inspection.combined_text.find("\"process\":\"planner\"") !=
            std::string::npos &&
        inspection.combined_text.find("\"metric_name\":\"restart_count\"") !=
            std::string::npos &&
        inspection.combined_text.find("\"metric_value\":\"3\"") !=
            std::string::npos &&
        inspection.combined_text.find("\"timestamp_us\":1234567890123456") !=
            std::string::npos;
    return naviai::log::test::l1::ReportResult(
        kCaseId, passed, "custom metric and process fields", inspection,
        passed ? "" : "missing custom L1 fields");
}
