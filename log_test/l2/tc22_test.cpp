#include "tc_support.hpp"

int main(int argc, char** argv) {
    constexpr int kCaseId = 22;
    auto root_dir = argc > 1 ? std::filesystem::path(argv[1])
                             : naviai::log::test::DefaultRootForCase(kCaseId);
    naviai::log::test::RemoveRoot(root_dir);
    std::error_code ec;
    std::filesystem::create_directories(root_dir / "l2_shutdown_bundle_legacy", ec);

    naviai::log::L2RecorderOptions options;
    options.root_dir = root_dir.string();
    options.session_id = "cleanup-session";
    options.topics = naviai::log::test::BuildStandardTopics(1024 * 1024, false);
    naviai::log::l2_log::InitRecorder(options);
    naviai::log::l2_log::ShutdownAll();

    const auto inspection = naviai::log::test::InspectRoot(root_dir);
    const bool passed =
        !std::filesystem::exists(root_dir / "l2_shutdown_bundle_legacy", ec);
    return naviai::log::test::ReportResult(
        kCaseId, passed, "legacy shutdown bundle cleaned", inspection);
}
