#include "tc_support.hpp"

#include <chrono>
#include <thread>

int main(int argc, char** argv) {
    constexpr int kCaseId = 30;
    auto root_dir = argc > 1 ? std::filesystem::path(argv[1])
                             : naviai::log::test::DefaultRootForCase(kCaseId);
    naviai::log::test::RemoveRoot(root_dir);

    naviai::log::FileGovernPolicy policy;
    policy.scan_interval_ms = 100;
    policy.cleanup_interval_ms = 60 * 1000;
    naviai::log::LogAgent agent(root_dir, policy);
    agent.Start();

    naviai::log::L2RecorderOptions options;
    options.root_dir = root_dir.string();
    options.session_id = "package-active";
    options.sample_mode = naviai::log::L2SampleMode::Full;
    options.topics = {naviai::log::test::BuildTopicDescriptor(
        naviai::log::test::kBusinessTopic, "validation/Business", "validator",
        "business", 1024)};
    naviai::log::l2_log::InitRecorder(options);
    for (int index = 0; index < 12; ++index) {
        naviai::log::l2_log::RecordBusinessTopic(naviai::log::test::BuildMessage(
            naviai::log::test::kBusinessTopic, index + 1, 512));
    }
    naviai::log::l2_log::FlushAll();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    naviai::log::L2PackageOptions package_options;
    package_options.root_dir = root_dir.string();
    const auto now_us = std::chrono::duration_cast<std::chrono::microseconds>(
                            std::chrono::system_clock::now().time_since_epoch())
                            .count();
    package_options.start_time_us = now_us - 5LL * 1000000LL;
    package_options.end_time_us = now_us + 5LL * 1000000LL;
    package_options.output_path = (root_dir / "tc30_package.tar.xz").string();
    const auto archive_path = naviai::log::l2_log::PackageRecords(package_options);
    const bool archive_valid =
        naviai::log::test::ArchiveContainsOnlyCompressedSegments(archive_path);

    naviai::log::l2_log::ShutdownAll();
    agent.Stop(true);
    const auto inspection = naviai::log::test::InspectRoot(root_dir);
    const bool passed = archive_valid && inspection.counts.gz_idx > 0;
    return naviai::log::test::ReportResult(
        kCaseId, passed,
        "package hit active files and archived only gz segments", inspection);
}
