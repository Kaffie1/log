#include "l1_support.hpp"

int main() {
    constexpr int kCaseId = 3;
    auto root = naviai::log::test::l1::DefaultRootForCase(kCaseId);
    naviai::log::test::l1::RemoveRoot(root);

    auto options = naviai::log::test::l1::BuildOptions(root);
    options.level = naviai::log::LogLevel::Error;
    naviai::log::l1::L1::Init(options);
    naviai::log::l1::L1::Write(naviai::log::l1::MonitorModule::Health,
                               naviai::log::LogLevel::Info,
                               "heartbeat visible");
    naviai::log::l1::L1::Write(naviai::log::l1::MonitorModule::Process,
                               naviai::log::LogLevel::Error,
                               "planner exited unexpectedly");
    naviai::log::l1::L1::Shutdown();

    const auto inspection = naviai::log::test::l1::InspectRoot(root);
    const bool passed =
        inspection.combined_text.find("heartbeat visible") == std::string::npos &&
        inspection.combined_text.find("planner exited unexpectedly") !=
            std::string::npos;
    return naviai::log::test::l1::ReportResult(
        kCaseId, passed, "global level filtering", inspection,
        passed ? "" : "global level filter mismatch");
}
