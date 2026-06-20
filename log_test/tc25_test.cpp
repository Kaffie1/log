#include "tc_support.hpp"

int main(int argc, char** argv) {
    constexpr int kCaseId = 25;
    auto root_dir = argc > 1 ? std::filesystem::path(argv[1])
                             : naviai::log::test::DefaultRootForCase(kCaseId);
    naviai::log::test::ScenarioConfig config;
    config.root_dir = root_dir;
    const auto run = naviai::log::test::RunScenario(config);
    const bool passed =
        run.inspection.idx_records == 0 && run.inspection.counts.active_files == 0;
    return naviai::log::test::ReportResult(
        kCaseId, passed, "empty start-stop stayed clean", run.inspection);
}
