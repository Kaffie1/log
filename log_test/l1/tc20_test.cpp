#include "l1_support.hpp"

int main() {
    constexpr int kCaseId = 20;
    auto root = naviai::log::test::l1::DefaultRootForCase(kCaseId);
    naviai::log::test::l1::RemoveRoot(root);

    int exit_code = 0;
    const auto output = naviai::log::test::l1::RunCommandCapture(
        "cd /workspaces/log && ./build_l1/log_cli/log_cli query --root " +
            root.string() + " --start 260101_000000 --duration 60",
        &exit_code);

    const auto inspection = naviai::log::test::l1::InspectRoot(root);
    const bool passed =
        exit_code == 0 &&
        output.find("success=true") != std::string::npos &&
        output.find("total_files=0") != std::string::npos;
    return naviai::log::test::l1::ReportResult(
        kCaseId, passed, "cli query supports yymmdd duration syntax", inspection,
        passed ? "" : "cli duration syntax did not execute as expected");
}
