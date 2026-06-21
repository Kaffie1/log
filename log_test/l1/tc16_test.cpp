#include "l1_support.hpp"

int main() {
    constexpr int kCaseId = 16;
    auto root = naviai::log::test::l1::DefaultRootForCase(kCaseId);
    auto package_root = root / "packages";
    naviai::log::test::l1::RemoveRoot(root);

    naviai::log::test::l1::CreateManagedSealedFile(root,
                                                   "system.log",
                                                   "early heartbeat\n",
                                                   1781764689000000,
                                                   1781764689500000);
    naviai::log::test::l1::CreateManagedSealedFile(root,
                                                   "system.log",
                                                   "late heartbeat\n",
                                                   1781764696000000,
                                                   1781764696500000);

    int exit_code = 0;
    const auto output = naviai::log::test::l1::RunCommandCapture(
        "cd /workspaces/log && ./build_l1/log_cli/log_cli package --root " +
            root.string() + " --package-root " + package_root.string() +
            " --start 1781764688000000 --duration 5",
        &exit_code);

    const auto inspection = naviai::log::test::l1::InspectRoot(root);
    const bool passed =
        exit_code == 0 &&
        output.find("task_state=completed") != std::string::npos &&
        output.find("source_file_count=1") != std::string::npos;
    return naviai::log::test::l1::ReportResult(
        kCaseId, passed, "cli package with start and duration", inspection,
        passed ? "" : "cli package duration flow failed");
}
