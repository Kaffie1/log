#include "l1_support.hpp"

#include <filesystem>

int main() {
    constexpr int kCaseId = 19;
    auto root = naviai::log::test::l1::DefaultRootForCase(kCaseId);
    auto package_root = root / "packages";
    naviai::log::test::l1::RemoveRoot(root);

    auto options = naviai::log::test::l1::BuildOptions(root);
    options.enable_runtime_agent = false;
    naviai::log::l1::L1::Init(options);
    naviai::log::l1::L1::Write(naviai::log::l1::MonitorModule::Process,
                               naviai::log::LogLevel::Error,
                               "planner exit");
    naviai::log::l1::L1::Shutdown();

    int exit_code = 0;
    const auto output = naviai::log::test::l1::RunCommandCapture(
        "cd /workspaces/log && ./build_l1/log_cli/log_cli package --root " +
            root.string() + " --package-root " + package_root.string() +
            " --start 1 --end 9999999999999999",
        &exit_code);

    bool packaged_gz = false;
    std::error_code ec;
    if (std::filesystem::exists(package_root, ec)) {
        for (const auto& entry :
             std::filesystem::recursive_directory_iterator(package_root, ec)) {
            if (entry.is_regular_file() && entry.path().extension() == ".gz") {
                packaged_gz = true;
                break;
            }
        }
    }

    const auto inspection = naviai::log::test::l1::InspectRoot(root);
    const bool passed =
        exit_code == 0 &&
        output.find("task_state=completed") != std::string::npos &&
        packaged_gz;
    return naviai::log::test::l1::ReportResult(
        kCaseId, passed, "package compresses raw sealed file", inspection,
        passed ? "" : "raw sealed file was not packaged as gz");
}
