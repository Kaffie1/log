#include "l1_support.hpp"

int main() {
    constexpr int kCaseId = 5;
    auto root = naviai::log::test::l1::DefaultRootForCase(kCaseId);
    naviai::log::test::l1::RemoveRoot(root);

    auto options = naviai::log::test::l1::BuildOptions(root);
    options.enable_runtime_agent = true;
    options.recover_on_shutdown = true;
    naviai::log::l1::L1::Init(options);
    naviai::log::l1::L1::Write({naviai::log::l1::MonitorModule::Storage,
                                naviai::log::LogLevel::Warn,
                                "disk usage exceeds threshold",
                                "disk_threshold",
                                "disk_usage",
                                "91",
                                "85"});
    naviai::log::l1::L1::Shutdown();

    const auto inspection = naviai::log::test::l1::InspectRoot(root);
    const bool passed = inspection.counts.active_files == 0 &&
                        inspection.counts.gz_files == 1 &&
                        inspection.combined_text.find("\"metric_name\":\"disk_usage\"") !=
                            std::string::npos &&
                        inspection.combined_text.find("\"threshold\":\"85\"") !=
                            std::string::npos;
    return naviai::log::test::l1::ReportResult(
        kCaseId, passed, "shutdown seal and compress", inspection,
        passed ? "" : "shutdown compress flow mismatch");
}
