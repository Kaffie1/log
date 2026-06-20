#include "level_mapper.hpp"

namespace naviai::log {

spdlog::level::level_enum ToSpdlogLevel(LogLevel level) {
    switch (level) {
        case LogLevel::Trace:
            return spdlog::level::trace;
        case LogLevel::Debug:
            return spdlog::level::debug;
        case LogLevel::Info:
            return spdlog::level::info;
        case LogLevel::Warn:
            return spdlog::level::warn;
        case LogLevel::Error:
            return spdlog::level::err;
        case LogLevel::Critical:
            return spdlog::level::critical;
    }
    return spdlog::level::info;
}

const char* ToLevelName(LogLevel level) {
    switch (level) {
        case LogLevel::Trace:
            return "TRACE";
        case LogLevel::Debug:
            return "DEBUG";
        case LogLevel::Info:
            return "INFO";
        case LogLevel::Warn:
            return "WARN";
        case LogLevel::Error:
            return "ERROR";
        case LogLevel::Critical:
            return "CRITICAL";
    }
    return "INFO";
}

spdlog::async_overflow_policy ToSpdlogOverflowPolicy(
    AsyncOverflowPolicy policy) {
    switch (policy) {
        case AsyncOverflowPolicy::Block:
            return spdlog::async_overflow_policy::block;
        case AsyncOverflowPolicy::OverrunOldest:
            return spdlog::async_overflow_policy::overrun_oldest;
    }
    return spdlog::async_overflow_policy::block;
}

}  // namespace naviai::log
