#include "tc_support.hpp"

int main(int argc, char** argv) {
    constexpr int kCaseId = 2;
    auto root_dir = argc > 1 ? std::filesystem::path(argv[1])
                             : naviai::log::test::DefaultRootForCase(kCaseId);
    naviai::log::test::ScenarioConfig config;
    config.root_dir = root_dir;
    config.business_messages = 10;
    config.static_messages = 3;
    config.large_messages = 4;
    const auto run = naviai::log::test::RunScenario(config);
    const bool passed = run.inspection.counts.business_files > 0 &&
                        run.inspection.counts.static_files > 0 &&
                        run.inspection.counts.large_files > 0 &&
                        run.inspection.sequences.size() == 17;
    return naviai::log::test::ReportResult(
        kCaseId, passed, "multi-topic directory split verified", run.inspection);
}
