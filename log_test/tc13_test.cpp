#include "tc_support.hpp"

int main(int argc, char** argv) {
    constexpr int kCaseId = 13;
    auto root_dir = argc > 1 ? std::filesystem::path(argv[1])
                             : naviai::log::test::DefaultRootForCase(kCaseId);
    naviai::log::test::RemoveRoot(root_dir);

    naviai::log::test::ScenarioConfig historical;
    historical.root_dir = root_dir;
    historical.business_messages = 6;
    naviai::log::test::RunScenario(historical);
    const auto before = naviai::log::test::InspectRoot(root_dir);

    naviai::log::test::ScenarioConfig active;
    active.root_dir = root_dir;
    active.business_messages = 6;
    active.static_messages = 2;
    active.large_messages = 1;
    active.shutdown_all = false;
    naviai::log::test::RunScenario(active);

    const bool recovered = naviai::log::test::RecoverActiveWithService(
        root_dir, naviai::log::test::kBaseTimeUs + 2000000LL);
    naviai::log::l2_log::ShutdownAll();
    const auto after = naviai::log::test::InspectRoot(root_dir);
    const bool passed = recovered && after.counts.active_files == 0 &&
                        after.counts.gz_idx + after.counts.raw_idx >=
                            before.counts.gz_idx + before.counts.raw_idx;
    return naviai::log::test::ReportResult(
        kCaseId, passed, "history remained intact during recovery", after);
}
