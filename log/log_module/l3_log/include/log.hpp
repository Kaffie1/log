#pragma once

#include <fmt/format.h>

#include "log_types.hpp"

namespace naviai::log_module {

class L3 {
  public:
    static void Init(LogLevel level = LogLevel::Info,
                     const std::string& root_dir = "/var/log/robot");
    static void RegisterModule(const std::string& module_name,
                               const ModuleOptions& options = {});
    static void Write(const std::string& module_name,
                      LogLevel level,
                      const std::string& message,
                      const PublicLogContext& context = {},
                      const PublicLogExtra& extra = {});
    static void SetLevel(const std::string& module_name, LogLevel level);
    static void SetLevelForAll(LogLevel level);
    static void Flush();
    static void Shutdown();
};

}  // namespace naviai::log_module

#define LOG_L3_TRACE(module, ...) \
    ::naviai::log_module::L3::Write(module, ::naviai::log_module::LogLevel::Trace, fmt::format(__VA_ARGS__))
#define LOG_L3_DEBUG(module, ...) \
    ::naviai::log_module::L3::Write(module, ::naviai::log_module::LogLevel::Debug, fmt::format(__VA_ARGS__))
#define LOG_L3_INFO(module, ...) \
    ::naviai::log_module::L3::Write(module, ::naviai::log_module::LogLevel::Info, fmt::format(__VA_ARGS__))
#define LOG_L3_WARN(module, ...) \
    ::naviai::log_module::L3::Write(module, ::naviai::log_module::LogLevel::Warn, fmt::format(__VA_ARGS__))
#define LOG_L3_ERROR(module, ...) \
    ::naviai::log_module::L3::Write(module, ::naviai::log_module::LogLevel::Error, fmt::format(__VA_ARGS__))
#define LOG_L3_CRITICAL(module, ...) \
    ::naviai::log_module::L3::Write(module, ::naviai::log_module::LogLevel::Critical, fmt::format(__VA_ARGS__))
