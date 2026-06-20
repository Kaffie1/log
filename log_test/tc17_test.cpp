#include "tc_support.hpp"

#include <fstream>

int main(int argc, char** argv) {
    constexpr int kCaseId = 17;
    auto root_dir = argc > 1 ? std::filesystem::path(argv[1])
                             : naviai::log::test::DefaultRootForCase(kCaseId);
    naviai::log::test::RemoveRoot(root_dir);
    naviai::log::LogService service(root_dir);
    const auto plan = service.BuildActiveFilePlan("business_data", ".data");
    if (!plan.has_value()) {
        return naviai::log::test::ReportResult(
            kCaseId, false, "failed to build orphan file path", {});
    }
    {
        std::ofstream output(plan->path);
        output << "orphan";
    }

    naviai::log::RecoveryTask task;
    const bool recovered = naviai::log::test::RecoverActiveWithService(
        root_dir, naviai::log::test::kBaseTimeUs + 1000000LL, &task);
    const auto inspection = naviai::log::test::InspectRoot(root_dir);
    const bool passed = !recovered && std::filesystem::exists(plan->path);
    return naviai::log::test::ReportResult(
        kCaseId, passed, "broken orphan file stayed observable", inspection);
}
