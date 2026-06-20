#pragma once

#include "log_types.hpp"

#include <cstdint>
#include <string>

namespace naviai::log {

class LogManager {
  public:
    static void Init(LogLevel level = LogLevel::Info,
                     const std::string& root_dir = "/tmp/logs");
    static void Init(const LoggerConfig& config);
    static void SetLevel(LogLevel level);
    static void SetLevel(const std::string& module_name, LogLevel level);
    static void SetFlushInterval(std::uint32_t interval_seconds);
    static void Flush();
    static void Shutdown();
    static void WriteRaw(const std::string& module_name,
                         LogLevel level,
                         const std::string& payload);

    template <typename T>
    static void Trace(const std::string& module_name, const T& payload) {
        Write(module_name, LogLevel::Trace, payload);
    }

    template <typename T>
    static void Debug(const std::string& module_name, const T& payload) {
        Write(module_name, LogLevel::Debug, payload);
    }

    template <typename T>
    static void Info(const std::string& module_name, const T& payload) {
        Write(module_name, LogLevel::Info, payload);
    }

    template <typename T>
    static void Warn(const std::string& module_name, const T& payload) {
        Write(module_name, LogLevel::Warn, payload);
    }

    template <typename T>
    static void Error(const std::string& module_name, const T& payload) {
        Write(module_name, LogLevel::Error, payload);
    }

    template <typename T>
    static void Critical(const std::string& module_name, const T& payload) {
        Write(module_name, LogLevel::Critical, payload);
    }

  private:
    template <typename T>
    static void Write(const std::string& module_name,
                      LogLevel level,
                      const T& payload);
};

}  // namespace naviai::log

#include <fmt/format.h>

namespace naviai::log {

namespace detail {
void WriteImpl(std::string module_name, LogLevel level, std::string payload);
}

template <typename T>
void LogManager::Write(const std::string& module_name,
                       LogLevel level,
                       const T& payload) {
    detail::WriteImpl(module_name, level, fmt::format("{}", payload));
}

template <>
inline void LogManager::Write<std::string>(const std::string& module_name,
                                           LogLevel level,
                                           const std::string& payload) {
    detail::WriteImpl(module_name, level, payload);
}

template <>
inline void LogManager::Write<const char*>(const std::string& module_name,
                                           LogLevel level,
                                           const char* const& payload) {
    detail::WriteImpl(module_name, level, payload == nullptr ? "" : payload);
}

}  // namespace naviai::log
