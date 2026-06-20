#include "tc_support.hpp"

int main(int argc, char** argv) {
    constexpr int kCaseId = 23;
    auto root_dir = argc > 1 ? std::filesystem::path(argv[1])
                             : naviai::log::test::DefaultRootForCase(kCaseId);
    naviai::log::test::RemoveRoot(root_dir);

    naviai::log::test::ScenarioConfig active;
    active.root_dir = root_dir;
    active.business_messages = 6;
    active.static_messages = 2;
    active.large_messages = 1;
    active.shutdown_all = false;
    naviai::log::test::RunScenario(active);

    naviai::log::FileGovernPolicy policy;
    naviai::log::LogAgent agent(root_dir, policy);
    agent.Start();
    const bool recovered = agent.RecoverNow().success;
    agent.Stop(true);
    naviai::log::l2_log::ShutdownAll();
    const auto inspection = naviai::log::test::InspectRoot(root_dir);
    const bool passed =
        recovered && inspection.counts.active_files == 0 && inspection.idx_records > 0;
    return naviai::log::test::ReportResult(
        kCaseId, passed, "startup recovery interface verified", inspection);
}
