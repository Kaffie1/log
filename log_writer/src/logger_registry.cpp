#include "logger_registry.hpp"

#include "error_handler.hpp"
#include "level_mapper.hpp"
#include "sink_assembler.hpp"

#include <stdexcept>
#include <utility>

#include <spdlog/async_logger.h>
#include <spdlog/spdlog.h>

namespace naviai::log {
LoggerRegistry::LoggerRegistry() = default;

LoggerRegistry::~LoggerRegistry() = default;

void LoggerRegistry::Configure(
    const LoggerConfig& config,
    std::shared_ptr<spdlog::details::thread_pool> thread_pool,
    std::shared_ptr<FormatterSelector> formatter_selector) {
    config_ = config;
    thread_pool_ = std::move(thread_pool);
    formatter_selector_ = std::move(formatter_selector);
    loggers_.clear();
}

void LoggerRegistry::Reset() {
    try {
        Flush();
    } catch (const std::exception& e) {
        ReportInternalError("logger_reset", e.what());
    } catch (...) {
        ReportInternalError("logger_reset", "unknown error");
    }
    loggers_.clear();
    formatter_selector_.reset();
    thread_pool_.reset();
}

void LoggerRegistry::SetLevel(LogLevel level) {
    config_.level = level;
    const auto spdlog_level = ToSpdlogLevel(level);
    for (auto& item : loggers_) {
        item.second->set_level(spdlog_level);
    }
}

void LoggerRegistry::SetLevel(const std::string& module_name, LogLevel level) {
    auto it = loggers_.find(module_name);
    if (it == loggers_.end()) {
        return;
    }
    it->second->set_level(ToSpdlogLevel(level));
}

void LoggerRegistry::Flush() {
    for (auto& item : loggers_) {
        try {
            item.second->flush();
        } catch (const std::exception& e) {
            ReportInternalError("flush", e.what());
        } catch (...) {
            ReportInternalError("flush", "unknown error");
        }
    }
}

void LoggerRegistry::Write(const LogRecord& record, LogLevel logger_level) {
    if (!formatter_selector_) {
        throw std::runtime_error("formatter selector is not configured");
    }

    auto logger = GetOrCreateLogger(record.module_name, logger_level);
    const auto level = ToSpdlogLevel(record.level);
    if (!logger->should_log(level)) {
        return;
    }
    try {
        logger->log(level, formatter_selector_->Get().Format(record));
    } catch (const std::exception& e) {
        ReportInternalError("write", e.what());
        FallbackWrite(record.module_name, record.level, record.payload);
    } catch (...) {
        ReportInternalError("write", "unknown error");
        FallbackWrite(record.module_name, record.level, record.payload);
    }
}

std::shared_ptr<spdlog::logger> LoggerRegistry::GetOrCreateLogger(
    const std::string& module_name, LogLevel logger_level) {
    auto it = loggers_.find(module_name);
    if (it != loggers_.end()) {
        return it->second;
    }

    auto sinks = SinkAssembler::Build(config_);
    if (sinks.empty()) {
        throw std::runtime_error("no sinks configured");
    }

    std::shared_ptr<spdlog::logger> logger;

    if (config_.async_mode) {
        if (!thread_pool_) {
            throw std::runtime_error("async thread pool is not configured");
        }
        logger = std::make_shared<spdlog::async_logger>(
            module_name,
            sinks.begin(),
            sinks.end(),
            thread_pool_,
            ToSpdlogOverflowPolicy(config_.async_overflow_policy));
    } else {
        logger =
            std::make_shared<spdlog::logger>(module_name, sinks.begin(), sinks.end());
    }

    const auto spdlog_level = ToSpdlogLevel(logger_level);
    logger->set_level(spdlog_level);
    logger->flush_on(spdlog::level::err);
    logger->set_error_handler([](const std::string& message) {
        ReportInternalError("logger", message);
    });
    loggers_.emplace(module_name, logger);
    return logger;
}

}  // namespace naviai::log
