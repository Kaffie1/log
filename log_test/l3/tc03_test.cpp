#include "l3_support.hpp"

int main() {
    constexpr int kCaseId = 3;
    auto root = naviai::log::test::l3::DefaultRootForCase(kCaseId);
    naviai::log::test::l3::RemoveRoot(root);

    auto options = naviai::log::test::l3::BuildOptions(root);
    options.level = naviai::log::LogLevel::Warn;
    options.enable_runtime_agent = false;
    naviai::log::l3::L3::Init(options);
    naviai::log::l3::L3::Write({naviai::log::l3::LoggerModule::Navigation,
                                naviai::log::LogLevel::Info,
                                "info should be filtered",
                                1781764689338040});
    naviai::log::l3::L3::Write({naviai::log::l3::LoggerModule::Navigation,
                                naviai::log::LogLevel::Error,
                                "error should remain",
                                1781764689407759});
    naviai::log::l3::L3::Shutdown();

    const auto inspection = naviai::log::test::l3::InspectRoot(root);
    const bool passed =
        inspection.combined_text.find("info should be filtered") == std::string::npos &&
        inspection.combined_text.find("error should remain") != std::string::npos;
    return naviai::log::test::l3::ReportResult(
        kCaseId, passed, "global level filtering", inspection,
        passed ? "" : "global level filtering mismatch");
}
