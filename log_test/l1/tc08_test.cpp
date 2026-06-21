#include "l1_support.hpp"

int main() {
    constexpr int kCaseId = 8;
    auto root = naviai::log::test::l1::DefaultRootForCase(kCaseId);
    naviai::log::test::l1::RemoveRoot(root);

    auto options = naviai::log::test::l1::BuildOptions(root);
    options.enable_runtime_agent = true;
    options.recover_on_shutdown = true;
    options.async_mode = false;
    naviai::log::l1::L1::Init(options);
    for (int i = 0; i < 500; ++i) {
        naviai::log::l1::L1::Write(naviai::log::l1::MonitorModule::Heartbeat,
                                   naviai::log::LogLevel::Info,
                                   "heartbeat sample");
    }
    naviai::log::l1::L1::Shutdown();

    const auto inspection = naviai::log::test::l1::InspectRoot(root);
    const bool passed = naviai::log::test::l1::CountSubstring(
                            inspection.combined_text, "heartbeat sample") == 500;
    return naviai::log::test::l1::ReportResult(
        kCaseId, passed, "high frequency sync writes", inspection,
        passed ? "" : "missing heartbeat samples");
}
