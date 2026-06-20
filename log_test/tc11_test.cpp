#include "tc_support.hpp"

#include <fstream>

int main(int argc, char** argv) {
    constexpr int kCaseId = 11;
    auto root_dir = argc > 1 ? std::filesystem::path(argv[1])
                             : naviai::log::test::DefaultRootForCase(kCaseId);
    naviai::log::test::RemoveRoot(root_dir);
    const auto foreign_path = root_dir.parent_path() / "foreign.log";
    {
        std::ofstream output(foreign_path);
        output << "keep";
    }

    naviai::log::test::ScenarioConfig config;
    config.root_dir = root_dir;
    config.business_messages = 8;
    config.enable_agent = true;
    config.post_record_sleep_ms = 300;
    const auto run = naviai::log::test::RunScenario(config);
    const bool passed = naviai::log::test::ReadFile(foreign_path) == "keep";
    std::error_code ec;
    std::filesystem::remove(foreign_path, ec);
    return naviai::log::test::ReportResult(
        kCaseId, passed, "agent scope stayed inside root", run.inspection);
}
