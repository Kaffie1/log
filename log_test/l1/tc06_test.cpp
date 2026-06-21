#include "l1_support.hpp"

int main() {
    constexpr int kCaseId = 6;
    auto root = naviai::log::test::l1::DefaultRootForCase(kCaseId);
    naviai::log::test::l1::RemoveRoot(root);

    const auto lingering = naviai::log::test::l1::CreateManagedActiveFile(
        root, "system.log", "legacy heartbeat\n", 1781764689338040);

    auto options = naviai::log::test::l1::BuildOptions(root);
    options.enable_runtime_agent = false;
    options.recover_on_init = true;
    naviai::log::l1::L1::Init(options);
    naviai::log::l1::L1::Write(naviai::log::l1::MonitorModule::Health,
                               naviai::log::LogLevel::Info,
                               "new heartbeat");
    naviai::log::l1::L1::Shutdown();

    const auto inspection = naviai::log::test::l1::InspectRoot(root);
    const bool passed = !lingering.empty() &&
                        inspection.counts.active_files == 0 &&
                        inspection.counts.sealed_files == 2 &&
                        inspection.combined_text.find("legacy heartbeat") !=
                            std::string::npos &&
                        inspection.combined_text.find("new heartbeat") !=
                            std::string::npos;
    return naviai::log::test::l1::ReportResult(
        kCaseId, passed, "recover lingering active on init", inspection,
        passed ? "" : "failed to recover lingering active");
}
