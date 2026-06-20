#include "tc_support.hpp"

int main(int argc, char** argv) {
    constexpr int kCaseId = 5;
    auto root_dir = argc > 1 ? std::filesystem::path(argv[1])
                             : naviai::log::test::DefaultRootForCase(kCaseId);
    naviai::log::test::ScenarioConfig config;
    config.root_dir = root_dir;
    config.segment_size_bytes = 2048;
    config.payload_size = 900;
    config.business_messages = 5;
    const auto run = naviai::log::test::RunScenario(config);
    const bool passed =
        run.inspection.sequences.size() == 5 && run.inspection.counts.raw_idx >= 2;
    return naviai::log::test::ReportResult(
        kCaseId, passed, "segment boundary crossing verified", run.inspection);
}
