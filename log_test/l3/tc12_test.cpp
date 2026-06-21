#include "l3_support.hpp"

int main() {
    constexpr int kCaseId = 12;
    auto root = naviai::log::test::l3::DefaultRootForCase(kCaseId);
    naviai::log::test::l3::RemoveRoot(root);

    constexpr std::int64_t kCustomTs = 1234567890123456LL;
    auto options = naviai::log::test::l3::BuildOptions(root);
    options.enable_runtime_agent = false;
    naviai::log::l3::L3::Init(options);
    naviai::log::l3::L3::Write(
        {naviai::log::l3::LoggerModule::Algorithm,
         naviai::log::LogLevel::Info,
         "custom timestamp",
         kCustomTs});
    naviai::log::l3::L3::Shutdown();

    const auto inspection = naviai::log::test::l3::InspectRoot(root);
    const bool passed =
        inspection.combined_text.find("\"timestamp_us\":1234567890123456") !=
        std::string::npos;
    return naviai::log::test::l3::ReportResult(
        kCaseId, passed, "custom timestamp passthrough", inspection,
        passed ? "" : "custom timestamp was not preserved");
}
