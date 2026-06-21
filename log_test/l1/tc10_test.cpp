#include "l1_support.hpp"

#include <exception>
#include <string>

int main() {
    constexpr int kCaseId = 10;
    auto root = naviai::log::test::l1::DefaultRootForCase(kCaseId);
    naviai::log::test::l1::RemoveRoot(root);

    bool failed_as_expected = false;
    std::string failure_reason;
    try {
        naviai::log::l1::L1::Init(naviai::log::LogLevel::Info, "/dev/null/log");
    } catch (const std::exception& e) {
        failed_as_expected = true;
        failure_reason = e.what();
    }

    const auto inspection = naviai::log::test::l1::InspectRoot(root);
    return naviai::log::test::l1::ReportResult(
        kCaseId, failed_as_expected, "invalid root path explicit failure", inspection,
        failed_as_expected ? "" : "init unexpectedly succeeded");
}
