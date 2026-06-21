#include "l1_support.hpp"

int main() {
    constexpr int kCaseId = 11;
    auto root = naviai::log::test::l1::DefaultRootForCase(kCaseId);
    naviai::log::test::l1::RemoveRoot(root);

    auto options = naviai::log::test::l1::BuildOptions(root);
    options.async_mode = true;
    options.async_queue_size = 4096;
    options.enable_runtime_agent = false;
    naviai::log::l1::L1::Init(options);
    for (int i = 0; i < 1000; ++i) {
        naviai::log::l1::L1::Write(naviai::log::l1::MonitorModule::Heartbeat,
                                   naviai::log::LogLevel::Info,
                                   "async heartbeat");
    }
    naviai::log::l1::L1::Shutdown();

    const auto inspection = naviai::log::test::l1::InspectRoot(root);
    const bool passed =
        naviai::log::test::l1::CountSubstring(inspection.combined_text,
                                              "async heartbeat") == 1000;
    return naviai::log::test::l1::ReportResult(
        kCaseId, passed, "high frequency async writes", inspection,
        passed ? "" : "missing async heartbeat samples");
}
