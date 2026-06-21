#include "l3_support.hpp"

int main() {
    constexpr int kCaseId = 6;
    auto root = naviai::log::test::l3::DefaultRootForCase(kCaseId);
    naviai::log::test::l3::RemoveRoot(root);

    auto options = naviai::log::test::l3::BuildOptions(root);
    options.enable_runtime_agent = true;
    naviai::log::l3::L3::Init(options);
    naviai::log::l3::L3::Write({naviai::log::l3::LoggerModule::Application,
                                naviai::log::LogLevel::Info,
                                "shutdown compression",
                                1781764689338040});
    naviai::log::l3::L3::Shutdown();

    const auto inspection = naviai::log::test::l3::InspectRoot(root);
    const bool passed =
        inspection.counts.active_files == 0 &&
        inspection.counts.gz_files == 1 &&
        inspection.combined_text.find("shutdown compression") != std::string::npos;
    return naviai::log::test::l3::ReportResult(
        kCaseId, passed, "shutdown seals and compresses", inspection,
        passed ? "" : "shutdown artifact state mismatch");
}
