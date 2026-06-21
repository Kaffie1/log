#include "l3_support.hpp"

#include <fstream>
#include <iostream>
#include <sstream>

#include <zlib.h>

namespace fs = std::filesystem;

namespace naviai::log::test::l3 {

std::filesystem::path DefaultRootForCase(int case_id) {
    std::ostringstream oss;
    oss << "/tmp/l3_tc";
    if (case_id < 10) {
        oss << '0';
    }
    oss << case_id << "_test";
    return std::filesystem::path(oss.str());
}

void RemoveRoot(const std::filesystem::path& root_dir) {
    std::error_code ec;
    std::filesystem::remove_all(root_dir, ec);
    std::filesystem::create_directories(root_dir, ec);
}

std::string ReadMaybeGzipFile(const std::filesystem::path& path) {
    if (path.extension() != ".gz") {
        std::ifstream input(path);
        std::stringstream buffer;
        buffer << input.rdbuf();
        return buffer.str();
    }

    gzFile file = gzopen(path.string().c_str(), "rb");
    if (file == nullptr) {
        return {};
    }

    std::string output;
    char buffer[4096];
    int count = 0;
    while ((count = gzread(file, buffer, sizeof(buffer))) > 0) {
        output.append(buffer, static_cast<std::size_t>(count));
    }
    gzclose(file);
    return output;
}

Inspection InspectRoot(const std::filesystem::path& root_dir) {
    Inspection inspection;
    LogService service(root_dir);
    std::error_code ec;
    if (!std::filesystem::exists(root_dir, ec)) {
        return inspection;
    }

    for (const auto& entry : std::filesystem::directory_iterator(root_dir, ec)) {
        if (ec || !entry.is_regular_file()) {
            continue;
        }
        const auto path = entry.path();
        inspection.files.push_back(path);
        ++inspection.counts.total_files;
        if (service.IsActiveFilePath(path)) {
            ++inspection.counts.active_files;
        } else {
            ++inspection.counts.sealed_files;
        }
        if (path.extension() == ".gz") {
            ++inspection.counts.gz_files;
        }
        inspection.combined_text += ReadMaybeGzipFile(path);
    }
    return inspection;
}

naviai::log::l3::L3SdkOptions BuildOptions(const std::filesystem::path& root_dir,
                                           const std::string& file_name) {
    naviai::log::l3::L3SdkOptions options;
    options.root_dir = root_dir.string();
    options.file_name = file_name;
    options.robot_sn = "robot-ut";
    options.level = LogLevel::Info;
    options.async_mode = false;
    options.enable_console_output = false;
    options.agent_policy.scan_interval_ms = 100;
    options.agent_policy.cleanup_interval_ms = 60 * 1000;
    return options;
}

std::size_t CountSubstring(const std::string& text, const std::string& token) {
    if (token.empty()) {
        return 0;
    }
    std::size_t count = 0;
    std::size_t pos = 0;
    while ((pos = text.find(token, pos)) != std::string::npos) {
        ++count;
        pos += token.size();
    }
    return count;
}

std::filesystem::path CreateManagedActiveFile(const std::filesystem::path& root_dir,
                                              const std::string& file_name,
                                              const std::string& content,
                                              std::int64_t start_time_us) {
    LogService service(root_dir);
    const auto plan = service.BuildActiveFilePlan("", file_name, start_time_us);
    if (!plan.has_value()) {
        return {};
    }
    std::ofstream output(plan->path);
    output << content;
    return plan->path;
}

std::filesystem::path CreateManagedSealedFile(const std::filesystem::path& root_dir,
                                              const std::string& file_name,
                                              const std::string& content,
                                              std::int64_t start_time_us,
                                              std::int64_t end_time_us) {
    LogService service(root_dir);
    const auto plan =
        service.BuildSealedFilePlan("", file_name, start_time_us, end_time_us);
    if (!plan.has_value()) {
        return {};
    }
    std::ofstream output(plan->path);
    output << content;
    return plan->path;
}

int ReportResult(int case_id,
                 bool passed,
                 const std::string& summary,
                 const Inspection& inspection,
                 const std::string& failure_reason) {
    std::cout << "TC";
    if (case_id < 10) {
        std::cout << '0';
    }
    std::cout << case_id
              << " result=" << (passed ? "PASS" : "FAIL")
              << " summary=" << summary
              << " active_files=" << inspection.counts.active_files
              << " sealed_files=" << inspection.counts.sealed_files
              << " gz_files=" << inspection.counts.gz_files
              << " total_files=" << inspection.counts.total_files;
    if (!failure_reason.empty()) {
        std::cout << " failure_reason=" << failure_reason;
    }
    std::cout << std::endl;
    return passed ? 0 : 1;
}

}  // namespace naviai::log::test::l3
