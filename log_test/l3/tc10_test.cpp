#include "l3_support.hpp"

#include <string>

int main() {
    constexpr int kCaseId = 10;
    auto root = naviai::log::test::l3::DefaultRootForCase(kCaseId);
    naviai::log::test::l3::RemoveRoot(root);

    auto options = naviai::log::test::l3::BuildOptions(root);
    options.async_mode = true;
    options.enable_runtime_agent = false;
    options.async_queue_size = 4096;
    naviai::log::l3::L3::Init(options);
    for (int i = 0; i < 1000; ++i) {
        naviai::log::l3::L3::Write(
            {naviai::log::l3::LoggerModule::Application,
             naviai::log::LogLevel::Info,
             "hf_async_" + std::to_string(i),
             1781764689338040LL + i});
    }
    naviai::log::l3::L3::Shutdown();

    const auto inspection = naviai::log::test::l3::InspectRoot(root);
    const bool passed =
        naviai::log::test::l3::CountSubstring(inspection.combined_text, "hf_async_") == 1000;
    return naviai::log::test::l3::ReportResult(
        kCaseId, passed, "high frequency async writes", inspection,
        passed ? "" : "record count mismatch under async writes");
}
