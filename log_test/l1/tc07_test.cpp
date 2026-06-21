#include "l1_support.hpp"

int main() {
    constexpr int kCaseId = 7;
    auto root = naviai::log::test::l1::DefaultRootForCase(kCaseId);
    naviai::log::test::l1::RemoveRoot(root);

    auto options = naviai::log::test::l1::BuildOptions(root);
    options.enable_runtime_agent = true;
    options.recover_on_shutdown = false;
    naviai::log::l1::L1::Init(options);
    naviai::log::l1::L1::Write({naviai::log::l1::MonitorModule::Storage,
                                naviai::log::LogLevel::Warn,
                                "disk usage high",
                                "disk_threshold",
                                "disk_usage",
                                "92",
                                "85"});
    naviai::log::l1::L1::Shutdown();

    const auto inspection = naviai::log::test::l1::InspectRoot(root);
    const bool passed = inspection.counts.active_files == 1 &&
                        inspection.counts.gz_files == 0;
    return naviai::log::test::l1::ReportResult(
        kCaseId, passed, "shutdown without recovery keeps active file", inspection,
        passed ? "" : "unexpected shutdown state when recovery disabled");
}
