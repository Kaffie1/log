#include "tc_support.hpp"

#include <chrono>
#include <thread>

int main(int argc, char** argv) {
    constexpr int kCaseId = 29;
    auto root_dir = argc > 1 ? std::filesystem::path(argv[1])
                             : naviai::log::test::DefaultRootForCase(kCaseId);
    naviai::log::test::RemoveRoot(root_dir);

    naviai::log::FileGovernPolicy policy;
    policy.scan_interval_ms = 100;
    policy.cleanup_interval_ms = 60 * 1000;
    naviai::log::LogAgent agent(root_dir, policy);
    const auto started = agent.Start();

    naviai::log::L2RecorderOptions options;
    options.root_dir = root_dir.string();
    options.session_id = "runtime-compress";
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
    std::this_thread::sleep_for(std::chrono::milliseconds(800));
    const auto state = agent.GetState();
    const auto inspection = naviai::log::test::InspectRoot(root_dir);
    naviai::log::l2_log::ShutdownAll();
    agent.Stop(true);
    const bool passed = started.success && state.stats.compressed_files > 0 &&
                        inspection.counts.active_files > 0 &&
                        inspection.counts.gz_idx > 0;
    return naviai::log::test::ReportResult(
        kCaseId, passed, "agent compressed sealed files during runtime",
        inspection);
}
