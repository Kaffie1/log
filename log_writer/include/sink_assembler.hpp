#pragma once

#include "log_types.hpp"

#include <memory>
#include <vector>

#include <spdlog/logger.h>
#include <spdlog/sinks/sink.h>

namespace naviai::log {

class SinkAssembler {
  public:
    static std::vector<spdlog::sink_ptr> Build(const LoggerConfig& config);
    static void ApplyErrorHandler(const std::vector<spdlog::sink_ptr>& sinks);

  private:
    static std::string BuildBasicFilePath(const LoggerConfig& config);
    static std::string BuildRotatingFilePath(const LoggerConfig& config);
};

}  // namespace naviai::log
