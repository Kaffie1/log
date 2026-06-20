#pragma once

#include "formatter.hpp"
#include "log_types.hpp"

#include <memory>
#include <unordered_map>
#include <vector>

#include <spdlog/async.h>
#include <spdlog/logger.h>

namespace naviai::log {

class LoggerRegistry {
  public:
    LoggerRegistry();
    ~LoggerRegistry();

    void Configure(const LoggerConfig& config,
                   std::shared_ptr<spdlog::details::thread_pool> thread_pool,
                   std::vector<spdlog::sink_ptr> sinks,
                   std::shared_ptr<FormatterSelector> formatter_selector);
    void Reset();
    void SetLevel(LogLevel level);
    void SetLevel(const std::string& module_name, LogLevel level);
    void Flush();
    void Write(const LogRecord& record, LogLevel logger_level);

  private:
    std::shared_ptr<spdlog::logger> GetOrCreateLogger(const std::string& module_name,
                                                      LogLevel logger_level);

    LoggerConfig config_;
    std::shared_ptr<spdlog::details::thread_pool> thread_pool_;
    std::vector<spdlog::sink_ptr> sinks_;
    std::shared_ptr<FormatterSelector> formatter_selector_;
    std::unordered_map<std::string, std::shared_ptr<spdlog::logger>> loggers_;
};

}  // namespace naviai::log
