#pragma once

#include "log_agent.hpp"
#include "log_types.hpp"
#include "monitor_module.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include <fmt/format.h>

namespace naviai::log::l1 {

struct L1SdkOptions {
    std::string root_dir{"/tmp/logs/l1"};
    std::string file_name{"system.log"};
    std::string host_name;
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

struct L1LogInput {
    MonitorModule module{MonitorModule::Unknown};
    LogLevel level{LogLevel::Info};
    std::string message;
    std::string event;
    std::string metric_name;
    std::string metric_value;
    std::string threshold;
    std::string process_name;
    std::int64_t timestamp_us{0};
};

class L1 {
  public:
    static void Init(LogLevel level, const std::string& root_dir);
    static void Init(const L1SdkOptions& options);
    static void SetLevel(LogLevel level);
    static void SetLevel(MonitorModule module, LogLevel level);
    static void SetLevel(const std::string& module_name, LogLevel level);
    static void Flush();
    static void Shutdown();

    static void Write(MonitorModule module,
                      LogLevel level,
                      std::string_view message,
                      const char* file = nullptr,
                      int line = 0,
                      const char* func = nullptr,
                      std::int64_t timestamp_us = 0);
    static void Write(const std::string& module_name,
                      LogLevel level,
                      std::string_view message,
                      const char* file = nullptr,
                      int line = 0,
                      const char* func = nullptr,
                      std::int64_t timestamp_us = 0);
    static void Write(const L1LogInput& input,
                      const char* file = nullptr,
                      int line = 0,
                      const char* func = nullptr);

    template <typename T>
    static void Trace(MonitorModule module,
                      const T& message,
                      const char* file = nullptr,
                      int line = 0,
                      const char* func = nullptr,
                      std::int64_t timestamp_us = 0) {
        Write(module,
              LogLevel::Trace,
              fmt::format("{}", message),
              file,
              line,
              func,
              timestamp_us);
    }

    template <typename T>
    static void Debug(MonitorModule module,
                      const T& message,
                      const char* file = nullptr,
                      int line = 0,
                      const char* func = nullptr,
                      std::int64_t timestamp_us = 0) {
        Write(module,
              LogLevel::Debug,
              fmt::format("{}", message),
              file,
              line,
              func,
              timestamp_us);
    }

    template <typename T>
    static void Info(MonitorModule module,
                     const T& message,
                     const char* file = nullptr,
                     int line = 0,
                     const char* func = nullptr,
                     std::int64_t timestamp_us = 0) {
        Write(module,
              LogLevel::Info,
              fmt::format("{}", message),
              file,
              line,
              func,
              timestamp_us);
    }

    template <typename T>
    static void Warn(MonitorModule module,
                     const T& message,
                     const char* file = nullptr,
                     int line = 0,
                     const char* func = nullptr,
                     std::int64_t timestamp_us = 0) {
        Write(module,
              LogLevel::Warn,
              fmt::format("{}", message),
              file,
              line,
              func,
              timestamp_us);
    }

    template <typename T>
    static void Error(MonitorModule module,
                      const T& message,
                      const char* file = nullptr,
                      int line = 0,
                      const char* func = nullptr,
                      std::int64_t timestamp_us = 0) {
        Write(module,
              LogLevel::Error,
              fmt::format("{}", message),
              file,
              line,
              func,
              timestamp_us);
    }

    template <typename T>
    static void Critical(MonitorModule module,
                         const T& message,
                         const char* file = nullptr,
                         int line = 0,
                         const char* func = nullptr,
                         std::int64_t timestamp_us = 0) {
        Write(module,
              LogLevel::Critical,
              fmt::format("{}", message),
              file,
              line,
              func,
              timestamp_us);
    }
};

}  // namespace naviai::log::l1

#define L1_LOG_TRACE(module, message) \
    ::naviai::log::l1::L1::Trace(module, message, __FILE__, __LINE__, __func__)
#define L1_LOG_DEBUG(module, message) \
    ::naviai::log::l1::L1::Debug(module, message, __FILE__, __LINE__, __func__)
#define L1_LOG_INFO(module, message) \
    ::naviai::log::l1::L1::Info(module, message, __FILE__, __LINE__, __func__)
#define L1_LOG_WARN(module, message) \
    ::naviai::log::l1::L1::Warn(module, message, __FILE__, __LINE__, __func__)
#define L1_LOG_ERROR(module, message) \
    ::naviai::log::l1::L1::Error(module, message, __FILE__, __LINE__, __func__)
#define L1_LOG_CRITICAL(module, message) \
    ::naviai::log::l1::L1::Critical(module, message, __FILE__, __LINE__, __func__)
