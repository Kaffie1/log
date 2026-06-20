#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace naviai::log_module {

struct QueryFilter {
    std::string root_dir;
    std::string module;
    std::string layer;
    std::string trace_id;
    std::string level;
    int64_t timestamp_from_us{0};
    int64_t timestamp_to_us{0};
};

class LogQueryService {
  public:
    std::vector<std::string> Query(const QueryFilter& filter) const;
};

class LogStatsService {
  public:
    std::vector<std::string> SummarizeByLayer(const QueryFilter& filter) const;
};

class TraceQueryService {
  public:
    std::vector<std::string> QueryTrace(const QueryFilter& filter) const;
};

}  // namespace naviai::log_module
