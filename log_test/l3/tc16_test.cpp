#include "l3_support.hpp"

#include <exception>

int main() {
    constexpr int kCaseId = 16;
    naviai::log::test::l3::Inspection inspection;
    bool threw = false;
    std::string error;

    try {
        auto options = naviai::log::test::l3::BuildOptions("/dev/null/log");
        options.enable_runtime_agent = false;
        naviai::log::l3::L3::Init(options);
        naviai::log::l3::L3::Shutdown();
    } catch (const std::exception& ex) {
        threw = true;
        error = ex.what();
    }

    const bool passed = threw;
    return naviai::log::test::l3::ReportResult(
        kCaseId, passed, "invalid root path fails explicitly", inspection,
        passed ? "" : "invalid root unexpectedly succeeded");
}
