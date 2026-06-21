#include "l1_support.hpp"

int main() {
    constexpr int kCaseId = 18;
    auto root = naviai::log::test::l1::DefaultRootForCase(kCaseId);
    naviai::log::test::l1::RemoveRoot(root);

    auto options = naviai::log::test::l1::BuildOptions(root);
    options.enable_runtime_agent = false;
    naviai::log::l1::L1::Init(options);
    naviai::log::l1::L1::Write({naviai::log::l1::MonitorModule::Resource,
                                naviai::log::LogLevel::Warn,
                                "disk usage high",
                                "disk_alarm",
                                "disk_usage",
                                "91",
                                "85"});
    naviai::log::l1::L1::Write({naviai::log::l1::MonitorModule::Resource,
                                naviai::log::LogLevel::Warn,
                                "cpu usage high",
                                "cpu_alarm",
                                "cpu_usage",
                                "88",
                                "80"});
    naviai::log::l1::L1::Write({naviai::log::l1::MonitorModule::Resource,
                                naviai::log::LogLevel::Warn,
                                "memory usage high",
                                "mem_alarm",
                                "memory_usage",
                                "93",
                                "85"});
    naviai::log::l1::L1::Shutdown();

    const auto inspection = naviai::log::test::l1::InspectRoot(root);
    const bool passed =
        inspection.combined_text.find("\"metric_name\":\"disk_usage\"") !=
            std::string::npos &&
        inspection.combined_text.find("\"metric_name\":\"cpu_usage\"") !=
            std::string::npos &&
        inspection.combined_text.find("\"metric_name\":\"memory_usage\"") !=
            std::string::npos;
    return naviai::log::test::l1::ReportResult(
        kCaseId, passed, "disk cpu memory threshold fields", inspection,
        passed ? "" : "resource alarm fields missing");
}
