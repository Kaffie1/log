#include "l3_support.hpp"

int main() {
    constexpr int kCaseId = 7;
    auto root = naviai::log::test::l3::DefaultRootForCase(kCaseId);
    naviai::log::test::l3::RemoveRoot(root);

    auto first = naviai::log::test::l3::BuildOptions(root);
    first.enable_runtime_agent = false;
    first.recover_on_shutdown = false;
    naviai::log::l3::L3::Init(first);
    naviai::log::l3::L3::Write({naviai::log::l3::LoggerModule::Navigation,
                                naviai::log::LogLevel::Info,
                                "left active before restart",
                                1781764689338040});
    naviai::log::l3::L3::Shutdown();

    auto second = naviai::log::test::l3::BuildOptions(root);
    second.enable_runtime_agent = false;
    second.recover_on_init = true;
    second.recover_on_shutdown = true;
    naviai::log::l3::L3::Init(second);
    naviai::log::l3::L3::Write({naviai::log::l3::LoggerModule::Navigation,
                                naviai::log::LogLevel::Info,
                                "after recovery restart",
                                1781764689407759});
    naviai::log::l3::L3::Shutdown();

    const auto inspection = naviai::log::test::l3::InspectRoot(root);
    const bool passed =
        inspection.counts.active_files == 0 &&
        inspection.counts.sealed_files == 2 &&
        inspection.combined_text.find("left active before restart") != std::string::npos &&
        inspection.combined_text.find("after recovery restart") != std::string::npos;
    return naviai::log::test::l3::ReportResult(
        kCaseId, passed, "recover lingering active on init", inspection,
        passed ? "" : "init recovery mismatch");
}
