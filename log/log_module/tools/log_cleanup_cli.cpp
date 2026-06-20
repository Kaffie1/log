#include <chrono>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

constexpr int64_t kDefaultRetentionHours = 48;

void PrintUsage(const char* argv0) {
    std::cerr
        << "Usage:\n"
        << "  " << argv0 << " [--root-dir <dir>] [--retention-hours <hours>] [--dry-run]\n";
}

std::string StripCompressionAndExtension(std::string name) {
    if (name.size() > 3 && name.substr(name.size() - 3) == ".gz") {
        name.resize(name.size() - 3);
    }
    const auto dot_pos = name.rfind('.');
    if (dot_pos != std::string::npos) {
        name.resize(dot_pos);
    }
    return name;
}

bool TryParseStartTimeFromPath(const std::filesystem::path& path,
                               int64_t* start_time_us) {
    if (start_time_us == nullptr) {
        return false;
    }

    std::string stem = StripCompressionAndExtension(path.filename().string());
    const auto dash_pos = stem.find('-');
    if (dash_pos == std::string::npos) {
        return false;
    }

    std::string start_text = stem.substr(0, dash_pos);
    const auto underscore_pos = start_text.rfind('_');
    if (underscore_pos != std::string::npos && underscore_pos >= 15) {
        start_text = start_text.substr(underscore_pos - 15, 15);
    }

    std::tm tm_buf{};
    std::istringstream stream(start_text);
    stream >> std::get_time(&tm_buf, "%Y%m%d_%H%M%S");
    if (stream.fail()) {
        return false;
    }

    *start_time_us = static_cast<int64_t>(std::mktime(&tm_buf)) * 1000000LL;
    return *start_time_us > 0;
}

bool IsEpochYearStartTimeUs(int64_t start_time_us) {
    if (start_time_us <= 0) {
        return false;
    }
    const auto time = static_cast<std::time_t>(start_time_us / 1000000LL);
    std::tm tm_buf{};
    localtime_r(&time, &tm_buf);
    return (tm_buf.tm_year + 1900) == 1970;
}

bool RemoveIfExpired(const std::filesystem::path& path,
                     int64_t cutoff_time_us,
                     bool dry_run) {
    int64_t start_time_us = 0;
    if (!TryParseStartTimeFromPath(path, &start_time_us) ||
        IsEpochYearStartTimeUs(start_time_us) ||
        start_time_us > cutoff_time_us) {
        return false;
    }

    if (dry_run) {
        std::cout << "[dry-run] remove " << path << '\n';
        return true;
    }

    std::error_code ec;
    std::filesystem::remove(path, ec);
    if (ec) {
        std::cerr << "failed to remove " << path << ": " << ec.message() << '\n';
        return false;
    }
    std::cout << "removed " << path << '\n';
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        std::filesystem::path root_dir{"/var/log/robot"};
        int64_t retention_hours = kDefaultRetentionHours;
        bool dry_run = false;

        for (int index = 1; index < argc; ++index) {
            const std::string arg = argv[index];
            auto require_value = [&](const char* name) -> std::string {
                if (index + 1 >= argc) {
                    throw std::invalid_argument(std::string("missing value for ") + name);
                }
                return argv[++index];
            };

            if (arg == "--root-dir") {
                root_dir = require_value("--root-dir");
            } else if (arg == "--retention-hours") {
                retention_hours = std::stoll(require_value("--retention-hours"));
            } else if (arg == "--dry-run") {
                dry_run = true;
            } else if (arg == "--help" || arg == "-h") {
                PrintUsage(argv[0]);
                return 0;
            } else {
                throw std::invalid_argument("unknown argument: " + arg);
            }
        }

        if (retention_hours <= 0) {
            throw std::invalid_argument("--retention-hours must be > 0");
        }

        std::error_code ec;
        if (!std::filesystem::exists(root_dir, ec)) {
            throw std::runtime_error("root dir does not exist: " + root_dir.string());
        }

        const int64_t now_us = std::chrono::duration_cast<std::chrono::microseconds>(
                                   std::chrono::system_clock::now().time_since_epoch())
                                   .count();
        const int64_t cutoff_time_us = now_us - retention_hours * 60LL * 60LL * 1000000LL;

        size_t removed_count = 0;
        for (const auto& entry :
             std::filesystem::recursive_directory_iterator(root_dir, ec)) {
            if (ec) {
                throw std::runtime_error("failed to iterate root dir: " + ec.message());
            }
            std::error_code entry_ec;
            if (!entry.is_regular_file(entry_ec)) {
                continue;
            }
            if (RemoveIfExpired(entry.path(), cutoff_time_us, dry_run)) {
                removed_count += 1;
            }
        }

        std::cout << "cleanup complete, removed_files=" << removed_count
                  << ", root_dir=" << root_dir << '\n';
        return 0;
    } catch (const std::exception& error) {
        PrintUsage(argv[0]);
        std::cerr << error.what() << '\n';
        return 1;
    }
}
