#include "formatter.hpp"

#include "level_mapper.hpp"

#include <chrono>
#include <ctime>
#include <fmt/format.h>

namespace naviai::log {
namespace {

std::string EscapeJson(const std::string& input) {
    std::string output;
    output.reserve(input.size());
    for (char ch : input) {
        switch (ch) {
            case '\\':
                output += "\\\\";
                break;
            case '"':
                output += "\\\"";
                break;
            case '\n':
                output += "\\n";
                break;
            case '\r':
                output += "\\r";
                break;
            case '\t':
                output += "\\t";
                break;
            default:
                output += ch;
                break;
        }
    }
    return output;
}

std::string FormatTimestampUs(std::int64_t timestamp_us) {
    using namespace std::chrono;

    const auto time_point = system_clock::time_point{microseconds(timestamp_us)};
    const auto seconds_part = time_point_cast<seconds>(time_point);
    const auto micros_part =
        duration_cast<microseconds>(time_point - seconds_part).count();
    const std::time_t tt = system_clock::to_time_t(time_point);

    std::tm tm_buf{};
#if defined(_WIN32)
    localtime_s(&tm_buf, &tt);
#else
    localtime_r(&tt, &tm_buf);
#endif

    return fmt::format("{:04d}-{:02d}-{:02d} {:02d}:{:02d}:{:02d}.{:06d}",
                       tm_buf.tm_year + 1900,
                       tm_buf.tm_mon + 1,
                       tm_buf.tm_mday,
                       tm_buf.tm_hour,
                       tm_buf.tm_min,
                       tm_buf.tm_sec,
                       static_cast<int>(micros_part));
}

}  // namespace

std::string TextFormatter::Format(const LogRecord& record) const {
    return fmt::format("[{}] [{}] [{}] {}",
                       FormatTimestampUs(record.timestamp_us),
                       ToLevelName(record.level),
                       record.module_name,
                       record.payload);
}

std::string JsonFormatter::Format(const LogRecord& record) const {
    return fmt::format(
        "{{\"timestamp_us\":{},\"module\":\"{}\",\"output_group\":\"{}\","
        "\"level\":\"{}\",\"payload\":\"{}\"}}",
        record.timestamp_us,
        EscapeJson(record.module_name),
        EscapeJson(record.output_group),
        ToLevelName(record.level),
        EscapeJson(record.payload));
}

FormatterSelector::FormatterSelector(OutputFormat output_format)
    : output_format_(output_format) {}

const Formatter& FormatterSelector::Get() const {
    if (output_format_ == OutputFormat::Json) {
        return json_formatter_;
    }
    return text_formatter_;
}

OutputFormat FormatterSelector::GetOutputFormat() const {
    return output_format_;
}

}  // namespace naviai::log
