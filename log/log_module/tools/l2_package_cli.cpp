#include <chrono>
#include <cstdint>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "l2_log/include/log.hpp"

namespace {

void PrintUsage(const char* argv0) {
    std::cerr
        << "Usage:\n"
        << "  " << argv0
        << " --root-dir <dir> --start-time <YYYYMMDD_HHMMSS> --end-time <YYYYMMDD_HHMMSS> [--output <path.tar.xz>]\n"
        << "  " << argv0
        << " --root-dir <dir> --start-time <YYYYMMDD_HHMMSS> --duration-sec <seconds> [--output <path.tar.xz>]\n";
}

int64_t ParseTimeUs(const std::string& value) {
    std::tm tm_buf{};
    std::istringstream stream(value);
    stream >> std::get_time(&tm_buf, "%Y%m%d_%H%M%S");
    if (stream.fail()) {
        throw std::invalid_argument("invalid time format: " + value);
    }
    return static_cast<int64_t>(std::mktime(&tm_buf)) * 1000000LL;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        naviai::log_module::L2PackageOptions options;
        std::string start_time_text;
        std::string end_time_text;
        int64_t duration_sec = 0;

        for (int index = 1; index < argc; ++index) {
            const std::string arg = argv[index];
            auto require_value = [&](const char* name) -> std::string {
                if (index + 1 >= argc) {
                    throw std::invalid_argument(std::string("missing value for ") + name);
                }
                return argv[++index];
            };

            if (arg == "--root-dir") {
                options.root_dir = require_value("--root-dir");
            } else if (arg == "--start-time") {
                start_time_text = require_value("--start-time");
            } else if (arg == "--end-time") {
                end_time_text = require_value("--end-time");
            } else if (arg == "--duration-sec") {
                duration_sec = std::stoll(require_value("--duration-sec"));
            } else if (arg == "--output") {
                options.output_path = require_value("--output");
            } else if (arg == "--help" || arg == "-h") {
                PrintUsage(argv[0]);
                return 0;
            } else {
                throw std::invalid_argument("unknown argument: " + arg);
            }
        }

        if (start_time_text.empty()) {
            throw std::invalid_argument("--start-time is required");
        }
        options.start_time_us = ParseTimeUs(start_time_text);

        if (!end_time_text.empty() && duration_sec > 0) {
            throw std::invalid_argument("use either --end-time or --duration-sec");
        }
        if (end_time_text.empty() && duration_sec <= 0) {
            throw std::invalid_argument("either --end-time or --duration-sec is required");
        }

        if (!end_time_text.empty()) {
            options.end_time_us = ParseTimeUs(end_time_text);
        } else {
            options.end_time_us =
                options.start_time_us + duration_sec * 1000000LL;
        }

        const std::string output = naviai::log_module::L2::PackageRecords(options);
        std::cout << output << '\n';
        return 0;
    } catch (const std::exception& error) {
        PrintUsage(argv[0]);
        std::cerr << error.what() << '\n';
        return 1;
    }
}
