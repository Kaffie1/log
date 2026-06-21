#include "l1_support.hpp"

int main() {
    constexpr int kCaseId = 9;
    auto root = naviai::log::test::l1::DefaultRootForCase(kCaseId);
    naviai::log::test::l1::RemoveRoot(root);

    auto options = naviai::log::test::l1::BuildOptions(root);
    options.enable_source_location = false;
    options.enable_runtime_agent = false;
    naviai::log::l1::L1::Init(options);
    naviai::log::l1::L1::Write(naviai::log::l1::MonitorModule::Process,
                               naviai::log::LogLevel::Error,
                               "planner process exited");
    naviai::log::l1::L1::Shutdown();

    const auto inspection = naviai::log::test::l1::InspectRoot(root);
    const bool passed =
        inspection.combined_text.find("\"file\":") == std::string::npos &&
        inspection.combined_text.find("\"line\":") == std::string::npos &&
        inspection.combined_text.find("\"func\":") == std::string::npos;
    return naviai::log::test::l1::ReportResult(
        kCaseId, passed, "disable source location", inspection,
        passed ? "" : "source location fields still present");
}
