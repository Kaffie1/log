#include "tc_support.hpp"

int main(int argc, char** argv) {
    constexpr int kCaseId = 3;
    auto root_dir = argc > 1 ? std::filesystem::path(argv[1])
                             : naviai::log::test::DefaultRootForCase(kCaseId);
    naviai::log::test::ScenarioConfig config;
    config.root_dir = root_dir;
    config.business_messages = 8;
    const auto run = naviai::log::test::RunScenario(config);
    const bool passed =
        !naviai::log::test::AnyFileNameContains(root_dir, "validation_idle");
    return naviai::log::test::ReportResult(
        kCaseId, passed, "idle topic did not create files", run.inspection);
}
