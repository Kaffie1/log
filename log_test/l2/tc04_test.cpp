#include "tc_support.hpp"

int main(int argc, char** argv) {
    constexpr int kCaseId = 4;
    auto root_dir = argc > 1 ? std::filesystem::path(argv[1])
                             : naviai::log::test::DefaultRootForCase(kCaseId);
    naviai::log::test::ScenarioConfig config;
    config.root_dir = root_dir;
    config.segment_size_bytes = 1024;
    config.payload_size = 700;
    config.business_messages = 20;
    config.enable_agent = true;
    config.post_record_sleep_ms = 600;
    const auto run = naviai::log::test::RunScenario(config);
    const bool passed =
        run.inspection.sequences.size() == 20 && run.inspection.counts.gz_idx >= 2;
    return naviai::log::test::ReportResult(
        kCaseId, passed, "natural segmentation verified", run.inspection);
}
