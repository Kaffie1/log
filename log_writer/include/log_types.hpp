#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace naviai::log {

enum class LogLevel {
    Trace,
    Debug,
    Info,
    Warn,
    Error,
    Critical,
};

enum class OutputFormat {
    Text,
    Json,
};

enum class AsyncOverflowPolicy {
    Block,
    OverrunOldest,
};

struct LoggerConfig {
    std::string root_dir{"/tmp/logs"};
    std::string file_name{"naviai.log"};
    LogLevel level{LogLevel::Info};
    OutputFormat output_format{OutputFormat::Text};
    std::size_t max_file_size_bytes{20 * 1024 * 1024};
    std::size_t max_files{100};
    std::size_t async_queue_size{8192};
    std::size_t async_worker_threads{1};
    std::uint32_t flush_interval_seconds{0};
    AsyncOverflowPolicy async_overflow_policy{AsyncOverflowPolicy::Block};
    bool async_mode{true};
    bool enable_basic_file_sink{false};
    bool enable_rotating_file_sink{true};
    bool enable_console_sink{false};
};

struct LogRecord {
    std::int64_t timestamp_us{0};
    std::string module_name;
    std::string output_group;
    LogLevel level;
    std::string payload;
};

}  // namespace naviai::log
