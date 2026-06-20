#include "tc_support.hpp"

int main() {
    constexpr int kCaseId = 28;
    naviai::log::L2RecorderOptions options;
    options.root_dir = "/dev/null/log";
    options.session_id = "invalid-root";
    options.topics = naviai::log::test::BuildStandardTopics(1024, false);
    bool threw = false;
    try {
        naviai::log::l2_log::InitRecorder(options);
        naviai::log::l2_log::RecordBusinessTopic(
            naviai::log::test::BuildMessage(naviai::log::test::kBusinessTopic, 1, 128));
        naviai::log::l2_log::FlushAll();
        naviai::log::l2_log::ShutdownAll();
    } catch (...) {
        threw = true;
    }
    return naviai::log::test::ReportResult(
        kCaseId, threw, "invalid root should fail observably", {});
}
