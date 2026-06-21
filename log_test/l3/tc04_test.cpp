#include "l3_support.hpp"

int main() {
    constexpr int kCaseId = 4;
    auto root = naviai::log::test::l3::DefaultRootForCase(kCaseId);
    naviai::log::test::l3::RemoveRoot(root);

    auto options = naviai::log::test::l3::BuildOptions(root);
    options.level = naviai::log::LogLevel::Error;
    options.enable_runtime_agent = false;
    naviai::log::l3::L3::Init(options);
    naviai::log::l3::L3::SetLevel(naviai::log::l3::LoggerModule::Navigation,
                                  naviai::log::LogLevel::Info);
    naviai::log::l3::L3::Write({naviai::log::l3::LoggerModule::Navigation,
                                naviai::log::LogLevel::Info,
                                "nav info visible",
                                1781764689338040});
    naviai::log::l3::L3::Write({naviai::log::l3::LoggerModule::Scheduler,
                                naviai::log::LogLevel::Info,
                                "scheduler info hidden",
                                1781764689407759});
    naviai::log::l3::L3::Shutdown();

    const auto inspection = naviai::log::test::l3::InspectRoot(root);
    const bool passed =
        inspection.combined_text.find("nav info visible") != std::string::npos &&
        inspection.combined_text.find("scheduler info hidden") == std::string::npos;
    return naviai::log::test::l3::ReportResult(
        kCaseId, passed, "module level override", inspection,
        passed ? "" : "module level override mismatch");
}
