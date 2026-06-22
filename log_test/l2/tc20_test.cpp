#include "tc_support.hpp"

int main(int argc, char** argv) {
    constexpr int kCaseId = 20;
    auto root_dir = argc > 1 ? std::filesystem::path(argv[1])
                             : naviai::log::test::DefaultRootForCase(kCaseId);
    naviai::log::test::ScenarioConfig config;
    config.root_dir = root_dir;
    config.business_messages = 5;
    config.enable_agent = true;
    config.post_record_sleep_ms = 400;
    config.shutdown_all = false;
    auto run = naviai::log::test::RunScenario(config);
    const bool passed =
        run.inspection.counts.active_files > 0 && run.inspection.counts.gz_idx == 0;
    if (run.agent != nullptr) {
        run.agent->Stop(false);
    }
    naviai::log::l2_log::ShutdownAll();
    return naviai::log::test::ReportResult(
        kCaseId, passed, "active files stayed uncompressed", run.inspection);
}
