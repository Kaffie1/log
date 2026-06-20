#include "query_service.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>

namespace naviai::log_module {

namespace {

bool ParseTextTimestampUs(const std::string& line, int64_t* timestamp_us) {
    if (line.size() < 29 || line.front() != '[') {
        return false;
    }
    const auto end = line.find(']');
    if (end == std::string::npos) {
        return false;
    }

    const std::string raw = line.substr(1, end - 1);
    if (raw.size() != 26) {
        return false;
    }

    std::tm tm_buf{};
    std::istringstream stream(raw.substr(0, 19));
    stream >> std::get_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
    if (stream.fail()) {
        return false;
    }

    const auto micro = std::stoll(raw.substr(20, 6));
    const auto seconds = std::chrono::system_clock::from_time_t(std::mktime(&tm_buf));
    *timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(
                        seconds.time_since_epoch())
                        .count() +
                    micro;
    return true;
}

bool ParseJsonTimestampUs(const std::string& line, int64_t* timestamp_us) {
    static constexpr char kField[] = "\"timestamp_us\":";
    const auto pos = line.find(kField);
    if (pos == std::string::npos) {
        return false;
    }

    const auto start = pos + sizeof(kField) - 1;
    size_t end = start;
    while (end < line.size() && std::isdigit(static_cast<unsigned char>(line[end]))) {
        ++end;
    }
    if (start == end) {
        return false;
    }

    *timestamp_us = std::stoll(line.substr(start, end - start));
    return true;
}

bool MatchesTimeRange(const std::string& line, const QueryFilter& filter) {
    if (filter.timestamp_from_us <= 0 && filter.timestamp_to_us <= 0) {
        return true;
    }

    int64_t timestamp_us = 0;
    if (!ParseJsonTimestampUs(line, &timestamp_us) &&
        !ParseTextTimestampUs(line, &timestamp_us)) {
        return false;
    }

    if (filter.timestamp_from_us > 0 && timestamp_us < filter.timestamp_from_us) {
        return false;
    }
    if (filter.timestamp_to_us > 0 && timestamp_us > filter.timestamp_to_us) {
        return false;
    }
    return true;
}

bool Matches(const std::string& line, const QueryFilter& filter) {
    if (!filter.module.empty() && line.find(filter.module) == std::string::npos) {
        return false;
    }
    if (!filter.layer.empty() && line.find(filter.layer) == std::string::npos) {
        return false;
    }
    if (!filter.trace_id.empty() &&
        line.find(filter.trace_id) == std::string::npos) {
        return false;
    }
    if (!filter.level.empty() && line.find(filter.level) == std::string::npos) {
        return false;
    }
    return MatchesTimeRange(line, filter);
}

}  // namespace

std::vector<std::string> LogQueryService::Query(const QueryFilter& filter) const {
    std::vector<std::string> result;
    if (filter.root_dir.empty()) {
        return result;
    }

    std::error_code ec;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(filter.root_dir, ec)) {
        if (ec || !entry.is_regular_file()) {
            continue;
        }
        std::ifstream input(entry.path());
        std::string line;
        while (std::getline(input, line)) {
            if (Matches(line, filter)) {
                result.push_back(line);
            }
        }
    }
    return result;
}

std::vector<std::string> TraceQueryService::QueryTrace(const QueryFilter& filter) const {
    return LogQueryService().Query(filter);
}

std::vector<std::string> LogStatsService::SummarizeByLayer(
    const QueryFilter& filter) const {
    std::map<std::string, size_t> counts;
    const auto lines = LogQueryService().Query(filter);
    for (const auto& line : lines) {
        if (line.find("L1") != std::string::npos) {
            ++counts["L1"];
        } else if (line.find("L2") != std::string::npos) {
            ++counts["L2"];
        } else if (line.find("L3") != std::string::npos) {
            ++counts["L3"];
        }
    }

    std::vector<std::string> result;
    for (const auto& item : counts) {
        result.push_back(item.first + ": " + std::to_string(item.second));
    }
    return result;
}

}  // namespace naviai::log_module
