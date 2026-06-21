#include "l1_support.hpp"

int main() {
    constexpr int kCaseId = 2;
    auto root = naviai::log::test::l1::DefaultRootForCase(kCaseId);
    naviai::log::test::l1::RemoveRoot(root);

    auto options = naviai::log::test::l1::BuildOptions(root);
    naviai::log::l1::L1::Init(options);
    naviai::log::l1::L1::Write(naviai::log::l1::MonitorModule::Resource,
                               naviai::log::LogLevel::Warn,
                               "disk usage exceeds threshold");
    naviai::log::l1::L1::Shutdown();

    const auto inspection = naviai::log::test::l1::InspectRoot(root);
    const bool passed = inspection.counts.total_files == 1 &&
                        inspection.counts.active_files == 0 &&
                        inspection.combined_text.find("\"module\":\"RESOURCE\"") !=
                            std::string::npos;
    return naviai::log::test::l1::ReportResult(
        kCaseId, passed, "single directory system log layout", inspection,
        passed ? "" : "unexpected l1 file layout");
}
