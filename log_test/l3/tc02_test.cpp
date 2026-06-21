#include "l3_support.hpp"

int main() {
    constexpr int kCaseId = 2;
    auto root = naviai::log::test::l3::DefaultRootForCase(kCaseId);
    naviai::log::test::l3::RemoveRoot(root);

    auto options = naviai::log::test::l3::BuildOptions(root);
    options.enable_runtime_agent = false;
    options.recover_on_shutdown = true;
    naviai::log::l3::L3::Init(options);
    naviai::log::l3::L3::Write({naviai::log::l3::LoggerModule::Scheduler,
                                naviai::log::LogLevel::Warn,
                                "single dir layout",
                                1781764689407759});
    naviai::log::l3::L3::Shutdown();

    const auto inspection = naviai::log::test::l3::InspectRoot(root);
    bool has_subdir = false;
    for (const auto& entry : std::filesystem::directory_iterator(root)) {
        if (entry.is_directory()) {
            has_subdir = true;
            break;
        }
    }
    const bool passed = !has_subdir && inspection.counts.total_files == 1;
    return naviai::log::test::l3::ReportResult(
        kCaseId, passed, "init dir keeps single-level layout", inspection,
        passed ? "" : "unexpected subdirectory or file count");
}
