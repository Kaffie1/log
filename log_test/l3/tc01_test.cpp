#include "l3_support.hpp"

int main() {
    constexpr int kCaseId = 1;
    auto root = naviai::log::test::l3::DefaultRootForCase(kCaseId);
    naviai::log::test::l3::RemoveRoot(root);

    auto options = naviai::log::test::l3::BuildOptions(root);
    naviai::log::l3::L3::Init(options);
    naviai::log::l3::L3::Write({naviai::log::l3::LoggerModule::Navigation,
                                naviai::log::LogLevel::Info,
                                "task started",
                                1781764689338040});
    naviai::log::l3::L3::Flush();
    naviai::log::l3::L3::Shutdown();

    const auto inspection = naviai::log::test::l3::InspectRoot(root);
    const bool passed =
        inspection.combined_text.find("\"layer\":\"L3\"") != std::string::npos &&
        inspection.combined_text.find("\"module\":\"NAVIGATION\"") != std::string::npos &&
        inspection.combined_text.find("\"payload\":\"task started\"") != std::string::npos;
    return naviai::log::test::l3::ReportResult(
        kCaseId, passed, "basic structured write", inspection,
        passed ? "" : "missing structured fields");
}
