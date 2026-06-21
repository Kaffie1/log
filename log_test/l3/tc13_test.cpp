#include "l3_support.hpp"

int main() {
    constexpr int kCaseId = 13;
    auto root = naviai::log::test::l3::DefaultRootForCase(kCaseId);
    naviai::log::test::l3::RemoveRoot(root);

    auto options = naviai::log::test::l3::BuildOptions(root);
    options.enable_runtime_agent = false;
    options.recover_on_shutdown = true;
    naviai::log::l3::L3::Init(options);
    naviai::log::l3::L3::Write(
        {naviai::log::l3::LoggerModule::Scene,
         naviai::log::LogLevel::Warn,
         "sealed raw without agent",
         1781764689338040});
    naviai::log::l3::L3::Shutdown();

    const auto inspection = naviai::log::test::l3::InspectRoot(root);
    const bool passed = inspection.counts.sealed_files == 1 && inspection.counts.gz_files == 0;
    return naviai::log::test::l3::ReportResult(
        kCaseId, passed, "shutdown without runtime agent keeps raw sealed file", inspection,
        passed ? "" : "unexpected gzip artifact when agent disabled");
}
