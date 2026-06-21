#include "l3_support.hpp"

int main() {
    constexpr int kCaseId = 14;
    auto root = naviai::log::test::l3::DefaultRootForCase(kCaseId);
    naviai::log::test::l3::RemoveRoot(root);

    auto lingering = naviai::log::test::l3::CreateManagedActiveFile(
        root, "l3_test.log", "old lingering active", 1760918400000000LL);
    if (lingering.empty()) {
        naviai::log::test::l3::Inspection empty;
        return naviai::log::test::l3::ReportResult(
            kCaseId, false, "recover_on_init disabled leaves old active", empty,
            "failed to create managed active file");
    }

    auto options = naviai::log::test::l3::BuildOptions(root);
    options.enable_runtime_agent = false;
    options.recover_on_init = false;
    options.recover_on_shutdown = true;
    naviai::log::l3::L3::Init(options);
    naviai::log::l3::L3::Write(
        {naviai::log::l3::LoggerModule::Application,
         naviai::log::LogLevel::Info,
         "new session after skipped recovery",
         1781764689338040});
    naviai::log::l3::L3::Shutdown();

    const auto inspection = naviai::log::test::l3::InspectRoot(root);
    const bool passed =
        inspection.counts.active_files == 1 &&
        inspection.counts.sealed_files == 1 &&
        inspection.combined_text.find("old lingering active") != std::string::npos;
    return naviai::log::test::l3::ReportResult(
        kCaseId, passed, "recover_on_init disabled preserves old active", inspection,
        passed ? "" : "recover_on_init disabled behavior mismatch");
}
