#include "l1_support.hpp"

#include <chrono>
#include <thread>

int main() {
    constexpr int kCaseId = 12;
    auto root = naviai::log::test::l1::DefaultRootForCase(kCaseId);
    naviai::log::test::l1::RemoveRoot(root);

    auto lingering =
        naviai::log::test::l1::CreateManagedActiveFile(root,
                                                       "system.log",
                                                       "preserved active\n",
                                                       1781764689338040);

    auto options = naviai::log::test::l1::BuildOptions(root);
    options.enable_runtime_agent = false;
    options.recover_on_init = false;
    naviai::log::l1::L1::Init(options);
    naviai::log::l1::L1::Write(naviai::log::l1::MonitorModule::Health,
                               naviai::log::LogLevel::Info,
                               "new monitor line");
    naviai::log::l1::L1::Shutdown();

    const auto inspection = naviai::log::test::l1::InspectRoot(root);
    const bool passed = !lingering.empty() &&
                        inspection.counts.active_files == 1 &&
                        inspection.counts.sealed_files == 1 &&
                        inspection.combined_text.find("preserved active") !=
                            std::string::npos;
    return naviai::log::test::l1::ReportResult(
        kCaseId, passed, "recover_on_init disabled preserves old active", inspection,
        passed ? "" : "old active was unexpectedly recovered");
}
