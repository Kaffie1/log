#include "l1_support.hpp"

int main() {
    constexpr int kCaseId = 4;
    auto root = naviai::log::test::l1::DefaultRootForCase(kCaseId);
    naviai::log::test::l1::RemoveRoot(root);

    auto options = naviai::log::test::l1::BuildOptions(root);
    options.level = naviai::log::LogLevel::Error;
    naviai::log::l1::L1::Init(options);
    naviai::log::l1::L1::SetLevel(naviai::log::l1::MonitorModule::Resource,
                                  naviai::log::LogLevel::Warn);
    naviai::log::l1::L1::Write(naviai::log::l1::MonitorModule::Health,
                               naviai::log::LogLevel::Warn,
                               "heartbeat jitter");
    naviai::log::l1::L1::Write(naviai::log::l1::MonitorModule::Resource,
                               naviai::log::LogLevel::Warn,
                               "memory usage high");
    naviai::log::l1::L1::Shutdown();

    const auto inspection = naviai::log::test::l1::InspectRoot(root);
    const bool passed =
        inspection.combined_text.find("heartbeat jitter") == std::string::npos &&
        inspection.combined_text.find("memory usage high") != std::string::npos;
    return naviai::log::test::l1::ReportResult(
        kCaseId, passed, "module level override", inspection,
        passed ? "" : "module level override mismatch");
}
