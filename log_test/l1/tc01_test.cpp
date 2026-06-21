#include "l1_support.hpp"

int main() {
    constexpr int kCaseId = 1;
    auto root = naviai::log::test::l1::DefaultRootForCase(kCaseId);
    naviai::log::test::l1::RemoveRoot(root);

    auto options = naviai::log::test::l1::BuildOptions(root);
    naviai::log::l1::L1::Init(options);
    naviai::log::l1::L1::Write({naviai::log::l1::MonitorModule::Health,
                                naviai::log::LogLevel::Info,
                                "watchdog heartbeat ok",
                                "heartbeat"});
    naviai::log::l1::L1::Flush();
    naviai::log::l1::L1::Shutdown();

    const auto inspection = naviai::log::test::l1::InspectRoot(root);
    const bool passed =
        inspection.combined_text.find("\"layer\":\"L1\"") != std::string::npos &&
        inspection.combined_text.find("\"module\":\"HEALTH\"") != std::string::npos &&
        inspection.combined_text.find("\"message\":\"watchdog heartbeat ok\"") !=
            std::string::npos &&
        inspection.combined_text.find("\"event\":\"heartbeat\"") != std::string::npos;
    return naviai::log::test::l1::ReportResult(
        kCaseId, passed, "basic system monitor write", inspection,
        passed ? "" : "missing structured l1 fields");
}
