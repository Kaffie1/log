#include "l1_support.hpp"

int main() {
    constexpr int kCaseId = 13;
    auto root = naviai::log::test::l1::DefaultRootForCase(kCaseId);
    naviai::log::test::l1::RemoveRoot(root);

    naviai::log::test::l1::CreateManagedActiveFile(root,
                                                   "system.log",
                                                   "stale sealed candidate\n",
                                                   1781764689338040);
    auto options = naviai::log::test::l1::BuildOptions(root);
    options.recover_on_init = true;
    options.enable_runtime_agent = true;
    naviai::log::l1::L1::Init(options);
    naviai::log::l1::L1::Write(naviai::log::l1::MonitorModule::System,
                               naviai::log::LogLevel::Info,
                               "boot complete");
    naviai::log::l1::L1::Shutdown();

    const auto inspection = naviai::log::test::l1::InspectRoot(root);
    const bool passed = inspection.counts.active_files == 0 &&
                        inspection.counts.gz_files >= 1 &&
                        inspection.combined_text.find("stale sealed candidate") !=
                            std::string::npos;
    return naviai::log::test::l1::ReportResult(
        kCaseId, passed, "agent drains recovered sealed files", inspection,
        passed ? "" : "recovered files were not drained");
}
