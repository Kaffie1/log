#include "tc_support.hpp"

int main(int argc, char** argv) {
    constexpr int kCaseId = 26;
    auto root_dir = argc > 1 ? std::filesystem::path(argv[1])
                             : naviai::log::test::DefaultRootForCase(kCaseId);
    naviai::log::test::ScenarioConfig config;
    config.root_dir = root_dir;
    config.segment_size_bytes = 1024;
    config.payload_size = 4096;
    config.business_messages = 2;
    const auto run = naviai::log::test::RunScenario(config);
    const bool passed =
        run.inspection.sequences.size() == 2 && run.inspection.counts.active_files == 0;
    return naviai::log::test::ReportResult(
        kCaseId, passed, "large single message boundary verified", run.inspection);
}
