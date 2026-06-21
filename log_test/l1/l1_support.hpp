#pragma once

#include "log_l1.hpp"
#include "log_service/include/log_service_naming.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace naviai::log::test::l1 {

struct Counts {
    std::size_t active_files{0};
    std::size_t sealed_files{0};
    std::size_t gz_files{0};
    std::size_t total_files{0};
};

struct Inspection {
    Counts counts;
    std::vector<std::filesystem::path> files;
    std::string combined_text;
};

std::filesystem::path DefaultRootForCase(int case_id);
void RemoveRoot(const std::filesystem::path& root_dir);
std::string ReadMaybeGzipFile(const std::filesystem::path& path);
std::string ReadFileText(const std::filesystem::path& path);
Inspection InspectRoot(const std::filesystem::path& root_dir);
naviai::log::l1::L1SdkOptions BuildOptions(const std::filesystem::path& root_dir,
                                           const std::string& file_name = "system.log");
std::size_t CountSubstring(const std::string& text, const std::string& token);
std::filesystem::path CreateManagedActiveFile(const std::filesystem::path& root_dir,
                                              const std::string& file_name,
                                              const std::string& content,
                                              std::int64_t start_time_us);
std::filesystem::path CreateManagedSealedFile(const std::filesystem::path& root_dir,
                                              const std::string& file_name,
                                              const std::string& content,
                                              std::int64_t start_time_us,
                                              std::int64_t end_time_us);
std::string RunCommandCapture(const std::string& command, int* exit_code = nullptr);
int ReportResult(int case_id,
                 bool passed,
                 const std::string& summary,
                 const Inspection& inspection,
                 const std::string& failure_reason = {});

}  // namespace naviai::log::test::l1
