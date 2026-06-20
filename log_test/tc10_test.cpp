#include "tc_support.hpp"

int main(int argc, char** argv) {
    constexpr int kCaseId = 10;
    auto root_dir = argc > 1 ? std::filesystem::path(argv[1])
                             : naviai::log::test::DefaultRootForCase(kCaseId);
    naviai::log::test::ScenarioConfig config;
    config.root_dir = root_dir;
    config.business_messages = 12;
    config.enable_agent = true;
    config.post_record_sleep_ms = 500;
    const auto run = naviai::log::test::RunScenario(config);
    const bool passed =
        run.inspection.counts.active_files == 0 && run.inspection.counts.gz_idx > 0;
    return naviai::log::test::ReportResult(
        kCaseId, passed, "graceful shutdown governance verified", run.inspection);
}
