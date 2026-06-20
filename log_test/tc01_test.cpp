#include "tc_support.hpp"

int main(int argc, char** argv) {
    constexpr int kCaseId = 1;
    auto root_dir = argc > 1 ? std::filesystem::path(argv[1])
                             : naviai::log::test::DefaultRootForCase(kCaseId);
    naviai::log::test::ScenarioConfig config;
    config.root_dir = root_dir;
    config.business_messages = 20;
    config.static_messages = 2;
    config.large_messages = 6;
    const auto run = naviai::log::test::RunScenario(config);
    const bool passed = run.inspection.sequences.size() == 28 &&
                        run.inspection.counts.business_files > 0;
    return naviai::log::test::ReportResult(
        kCaseId, passed, "basic record count and directory layout verified",
        run.inspection);
}
