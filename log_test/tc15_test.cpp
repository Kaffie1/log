#include "tc_support.hpp"

int main(int argc, char** argv) {
    constexpr int kCaseId = 15;
    auto root_dir = argc > 1 ? std::filesystem::path(argv[1])
                             : naviai::log::test::DefaultRootForCase(kCaseId);
    naviai::log::test::RemoveRoot(root_dir);

    naviai::log::test::ScenarioConfig active;
    active.root_dir = root_dir;
    active.business_messages = 4;
    active.static_messages = 4;
    active.large_messages = 4;
    active.shutdown_all = false;
    naviai::log::test::RunScenario(active);

    naviai::log::RecoveryTask task;
    const bool recovered = naviai::log::test::RecoverActiveWithService(
        root_dir, naviai::log::test::kBaseTimeUs + 3000000LL, &task);
    naviai::log::l2_log::ShutdownAll();
    const auto inspection = naviai::log::test::InspectRoot(root_dir);
    const bool passed = recovered && task.recovered_paths.size() >= 2 &&
                        inspection.counts.active_files == 0;
    return naviai::log::test::ReportResult(
        kCaseId, passed, "multi-group recovery verified", inspection);
}
