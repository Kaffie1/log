#include "l3_support.hpp"

int main() {
    constexpr int kCaseId = 8;
    auto root = naviai::log::test::l3::DefaultRootForCase(kCaseId);
    naviai::log::test::l3::RemoveRoot(root);

    auto first = naviai::log::test::l3::BuildOptions(root);
    first.enable_runtime_agent = false;
    naviai::log::l3::L3::Init(first);
    naviai::log::l3::L3::Write({naviai::log::l3::LoggerModule::Platform,
                                naviai::log::LogLevel::Info,
                                "sealed raw before agent",
                                1781764689338040});
    naviai::log::l3::L3::Shutdown();

    auto second = naviai::log::test::l3::BuildOptions(root);
    second.enable_runtime_agent = true;
    naviai::log::l3::L3::Init(second);
    naviai::log::l3::L3::Write({naviai::log::l3::LoggerModule::Platform,
                                naviai::log::LogLevel::Info,
                                "agent started",
                                1781764689407759});
    naviai::log::l3::L3::Shutdown();

    const auto inspection = naviai::log::test::l3::InspectRoot(root);
    const bool passed =
        inspection.counts.gz_files >= 1 &&
        inspection.combined_text.find("sealed raw before agent") != std::string::npos &&
        inspection.combined_text.find("agent started") != std::string::npos;
    return naviai::log::test::l3::ReportResult(
        kCaseId, passed, "agent drains existing sealed files", inspection,
        passed ? "" : "agent drain/compress mismatch");
}
