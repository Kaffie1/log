#include "tc_support.hpp"

#include <set>

int main(int argc, char** argv) {
    constexpr int kCaseId = 8;
    auto root_dir = argc > 1 ? std::filesystem::path(argv[1])
                             : naviai::log::test::DefaultRootForCase(kCaseId);
    naviai::log::test::ScenarioConfig config;
    config.root_dir = root_dir;
    config.segment_size_bytes = 1024;
    config.payload_size = 700;
    config.business_messages = 20;
    const auto run = naviai::log::test::RunScenario(config);

    std::set<std::string> names;
    std::error_code ec;
    for (const auto& entry :
         std::filesystem::recursive_directory_iterator(root_dir, ec)) {
        if (!ec && entry.is_regular_file()) {
            names.insert(entry.path().filename().string());
        }
    }
    const bool passed = names.size() ==
                            run.inspection.counts.raw_data +
                                run.inspection.counts.raw_idx &&
                        run.inspection.counts.active_files == 0;
    return naviai::log::test::ReportResult(
        kCaseId, passed, "sealed file naming uniqueness verified",
        run.inspection);
}
