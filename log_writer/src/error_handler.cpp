#include "error_handler.hpp"

#include "level_mapper.hpp"

#include <chrono>
#include <cstdio>
#include <ctime>

namespace naviai::log {
namespace {

std::string BuildTimestamp() {
    const auto now = std::chrono::system_clock::now();
    const auto micros =
        std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch())
            .count() %
        1000000;
    const std::time_t tt = std::chrono::system_clock::to_time_t(now);

    std::tm tm_buf{};
#if defined(_WIN32)
    localtime_s(&tm_buf, &tt);
#else
    localtime_r(&tt, &tm_buf);
#endif

    char buffer[64];
    std::snprintf(buffer,
                  sizeof(buffer),
                  "%04d-%02d-%02d %02d:%02d:%02d.%06d",
                  tm_buf.tm_year + 1900,
                  tm_buf.tm_mon + 1,
                  tm_buf.tm_mday,
                  tm_buf.tm_hour,
                  tm_buf.tm_min,
                  tm_buf.tm_sec,
                  static_cast<int>(micros));
    return buffer;
}

}  // namespace

void ReportInternalError(const std::string& stage, const std::string& message) {
    std::fprintf(stderr,
                 "[log_api][%s][%s] %s\n",
                 BuildTimestamp().c_str(),
                 stage.c_str(),
                 message.c_str());
}

void FallbackWrite(const std::string& module_name,
                   LogLevel level,
                   const std::string& payload) {
    std::fprintf(stderr,
                 "[%s] [%s] [%s] %s\n",
                 BuildTimestamp().c_str(),
                 ToLevelName(level),
                 module_name.c_str(),
                 payload.c_str());
}

}  // namespace naviai::log
