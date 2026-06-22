#include "tc_support.hpp"

int main(int argc, char** argv) {
    constexpr int kCaseId = 21;
    auto root_dir = argc > 1 ? std::filesystem::path(argv[1])
                             : naviai::log::test::DefaultRootForCase(kCaseId);
    naviai::log::test::ScenarioConfig config;
    config.root_dir = root_dir;
    config.business_messages = 10;
    const auto run = naviai::log::test::RunScenario(config);

    naviai::log::FileGovernPolicy policy;
    naviai::log::LogAgent agent(root_dir, policy);
    agent.Start();
    const auto first = agent.CompressNow();
    const auto second = agent.CompressNow();
    agent.Stop(true);
    const auto inspection = naviai::log::test::InspectRoot(root_dir);
    const bool passed = first.affected_files > 0 && second.affected_files == 0;
    return naviai::log::test::ReportResult(
        kCaseId, passed, "repeat governance skipped processed files", inspection);
}
