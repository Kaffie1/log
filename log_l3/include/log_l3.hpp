#pragma once

#include "log_types.hpp"
#include "logger_module.hpp"
#include "log_agent.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

#include <fmt/format.h>

namespace naviai::log::l3 {

struct L3SdkOptions {
    std::string root_dir{"/tmp/logs/l3"};
    std::string file_name{"l3.log"};
    std::string robot_sn;
    LogLevel level{LogLevel::Info};
    std::size_t max_file_size_bytes{20 * 1024 * 1024};
    std::size_t max_files{100};
    std::size_t async_queue_size{8192};
    std::size_t async_worker_threads{1};
    std::uint32_t flush_interval_seconds{1};
    AsyncOverflowPolicy async_overflow_policy{AsyncOverflowPolicy::Block};
    bool async_mode{true};
    bool enable_console_output{false};
    bool enable_source_location{true};
    bool recover_on_init{true};
    bool recover_on_shutdown{true};
    bool enable_runtime_agent{true};
    FileGovernPolicy agent_policy{};
};

struct L3LogInput {
    LoggerModule module{LoggerModule::Unknown};
    LogLevel level{LogLevel::Info};
    std::string payload;
    std::int64_t timestamp_us{0};
};

class L3 {
  public:
    static void Init(LogLevel level, const std::string& root_dir);
    static void Init(const L3SdkOptions& options);
    static void SetLevel(LogLevel level);
    static void SetLevel(LoggerModule module, LogLevel level);
    static void SetLevel(const std::string& module_name, LogLevel level);
    static void Flush();
    static void Shutdown();

    static void Write(LoggerModule module,
                      LogLevel level,
                      std::string_view payload,
                      const char* file = nullptr,
                      int line = 0,
                      const char* func = nullptr,
                      std::int64_t timestamp_us = 0);

    static void Write(const std::string& module_name,
                      LogLevel level,
                      std::string_view payload,
                      const char* file = nullptr,
                      int line = 0,
                      const char* func = nullptr,
                      std::int64_t timestamp_us = 0);
    static void Write(const L3LogInput& input,
                      const char* file = nullptr,
                      int line = 0,
                      const char* func = nullptr);

    template <typename T>
    static void Trace(LoggerModule module,
                      const T& payload,
                      const char* file = nullptr,
                      int line = 0,
                      const char* func = nullptr,
                      std::int64_t timestamp_us = 0) {
        Write(module,
              LogLevel::Trace,
              fmt::format("{}", payload),
              file,
              line,
              func,
              timestamp_us);
    }

    template <typename T>
    static void Debug(LoggerModule module,
                      const T& payload,
                      const char* file = nullptr,
                      int line = 0,
                      const char* func = nullptr,
                      std::int64_t timestamp_us = 0) {
        Write(module,
              LogLevel::Debug,
              fmt::format("{}", payload),
              file,
              line,
              func,
              timestamp_us);
    }

    template <typename T>
    static void Info(LoggerModule module,
                     const T& payload,
                     const char* file = nullptr,
                     int line = 0,
                     const char* func = nullptr,
                     std::int64_t timestamp_us = 0) {
        Write(module,
              LogLevel::Info,
              fmt::format("{}", payload),
              file,
              line,
              func,
              timestamp_us);
    }

    template <typename T>
    static void Warn(LoggerModule module,
                     const T& payload,
                     const char* file = nullptr,
                     int line = 0,
                     const char* func = nullptr,
                     std::int64_t timestamp_us = 0) {
        Write(module,
              LogLevel::Warn,
              fmt::format("{}", payload),
              file,
              line,
              func,
              timestamp_us);
    }

    template <typename T>
    static void Error(LoggerModule module,
                      const T& payload,
                      const char* file = nullptr,
                      int line = 0,
                      const char* func = nullptr,
                      std::int64_t timestamp_us = 0) {
        Write(module,
              LogLevel::Error,
              fmt::format("{}", payload),
              file,
              line,
              func,
              timestamp_us);
    }

    template <typename T>
    static void Critical(LoggerModule module,
                         const T& payload,
                         const char* file = nullptr,
                         int line = 0,
                         const char* func = nullptr,
                         std::int64_t timestamp_us = 0) {
        Write(module,
              LogLevel::Critical,
              fmt::format("{}", payload),
              file,
              line,
              func,
              timestamp_us);
    }
};

}  // namespace naviai::log::l3

#define L3_LOG_TRACE(module, payload) \
    ::naviai::log::l3::L3::Trace(module, payload, __FILE__, __LINE__, __func__)
#define L3_LOG_DEBUG(module, payload) \
    ::naviai::log::l3::L3::Debug(module, payload, __FILE__, __LINE__, __func__)
#define L3_LOG_INFO(module, payload) \
    ::naviai::log::l3::L3::Info(module, payload, __FILE__, __LINE__, __func__)
#define L3_LOG_WARN(module, payload) \
    ::naviai::log::l3::L3::Warn(module, payload, __FILE__, __LINE__, __func__)
#define L3_LOG_ERROR(module, payload) \
    ::naviai::log::l3::L3::Error(module, payload, __FILE__, __LINE__, __func__)
#define L3_LOG_CRITICAL(module, payload) \
    ::naviai::log::l3::L3::Critical(module, payload, __FILE__, __LINE__, __func__)
