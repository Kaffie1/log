#include "l3_support.hpp"

int main() {
    constexpr int kCaseId = 5;
    auto root = naviai::log::test::l3::DefaultRootForCase(kCaseId);
    naviai::log::test::l3::RemoveRoot(root);

    auto options = naviai::log::test::l3::BuildOptions(root);
    options.enable_runtime_agent = false;
    options.enable_source_location = true;
    naviai::log::l3::L3::Init(options);
    L3_LOG_INFO(naviai::log::l3::LoggerModule::Scene, "scene with source");
    naviai::log::l3::L3::Shutdown();

    const auto inspection = naviai::log::test::l3::InspectRoot(root);
    const bool passed =
        inspection.combined_text.find("\"file\":\"") != std::string::npos &&
        inspection.combined_text.find("\"line\":") != std::string::npos &&
        inspection.combined_text.find("\"func\":\"main\"") != std::string::npos;
    return naviai::log::test::l3::ReportResult(
        kCaseId, passed, "source location fields", inspection,
        passed ? "" : "source location fields missing");
}
