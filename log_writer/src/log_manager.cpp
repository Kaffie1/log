#include "log_manager.hpp"

#include "error_handler.hpp"
#include "formatter.hpp"
#include "module_registry.hpp"
#include "logger_registry.hpp"
#include "record_builder.hpp"
#include "sink_assembler.hpp"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>

#include <spdlog/async.h>
#include <spdlog/spdlog.h>

namespace naviai::log {
namespace {

struct RuntimeState {
    std::mutex mutex;
    bool initialized{false};
    LoggerConfig config;
    ModuleRegistry module_registry;
    LoggerRegistry logger_registry;
    std::shared_ptr<FormatterSelector> formatter_selector;
    std::shared_ptr<spdlog::details::thread_pool> thread_pool;
    std::atomic<bool> stop_flush_worker{false};
    std::thread flush_worker;
};

RuntimeState& State() {
    static RuntimeState state;
    return state;
}

void EnsureInitialized() {
    if (!State().initialized) {
        throw std::runtime_error("LogManager is not initialized");
    }
}

void BuildRuntime(const LoggerConfig& config) {
    auto& state = State();

    if (config.root_dir.empty()) {
        throw std::runtime_error("log root_dir must not be empty");
    }
    if ((config.enable_basic_file_sink || config.enable_rotating_file_sink) &&
        config.file_name.empty()) {
        throw std::runtime_error("log file_name must not be empty");
    }
    if (!config.enable_basic_file_sink && !config.enable_rotating_file_sink &&
        !config.enable_console_sink) {
        throw std::runtime_error("at least one sink must be enabled");
    }
    if (config.async_mode) {
        if (config.async_queue_size == 0) {
            throw std::runtime_error("async_queue_size must be greater than 0");
        }
        if (config.async_worker_threads == 0) {
            throw std::runtime_error("async_worker_threads must be greater than 0");
        }
    }

    std::error_code ec;
    std::filesystem::create_directories(config.root_dir, ec);
    if (ec) {
        throw std::runtime_error("failed to create log directory: " + ec.message());
    }

    state.config = config;
    state.formatter_selector =
        std::make_shared<FormatterSelector>(config.output_format);

    if (config.async_mode) {
        state.thread_pool = std::make_shared<spdlog::details::thread_pool>(
            config.async_queue_size, config.async_worker_threads);
    } else {
        state.thread_pool.reset();
    }

    state.logger_registry.Configure(
        state.config,
        state.thread_pool,
        state.formatter_selector);

    state.stop_flush_worker = false;
    if (config.flush_interval_seconds > 0) {
        state.flush_worker = std::thread([interval = config.flush_interval_seconds]() {
            while (!State().stop_flush_worker.load()) {
                std::this_thread::sleep_for(std::chrono::seconds(interval));
                if (State().stop_flush_worker.load()) {
                    break;
                }
                std::lock_guard<std::mutex> lock(State().mutex);
                if (State().initialized) {
                    State().logger_registry.Flush();
                }
            }
        });
    }
    state.initialized = true;
}

void StopFlushWorker(RuntimeState& state) {
    state.stop_flush_worker = true;
    if (state.flush_worker.joinable()) {
        state.flush_worker.join();
    }
}

}  // namespace

void LogManager::Init(LogLevel level, const std::string& root_dir) {
    LoggerConfig config;
    config.level = level;
    config.root_dir = root_dir;
    Init(config);
}

void LogManager::Init(const LoggerConfig& config) {
    auto& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);

    try {
        if (state.initialized) {
            if (state.config.root_dir == config.root_dir &&
                state.config.file_name == config.file_name &&
                state.config.async_mode == config.async_mode &&
                state.config.output_format == config.output_format) {
                state.config.level = config.level;
                state.module_registry.SetDefaultLevel(config.level);
                for (const auto& module_config : state.module_registry.List()) {
                    state.logger_registry.SetLevel(module_config.module_name,
                                                   module_config.level);
                }
                return;
            }

            StopFlushWorker(state);
            state.module_registry.Clear();
            state.logger_registry.Reset();
            state.thread_pool.reset();
            state.formatter_selector.reset();
            spdlog::shutdown();
            state.initialized = false;
        }

        BuildRuntime(config);
    } catch (const std::exception& e) {
        ReportInternalError("init", e.what());
        state.initialized = false;
        throw;
    } catch (...) {
        ReportInternalError("init", "unknown error");
        state.initialized = false;
        throw;
    }
}

void LogManager::SetLevel(LogLevel level) {
    auto& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);
    EnsureInitialized();

    state.config.level = level;
    state.module_registry.SetDefaultLevel(level);
    for (const auto& module_config : state.module_registry.List()) {
        state.logger_registry.SetLevel(module_config.module_name, module_config.level);
    }
}

void LogManager::SetLevel(const std::string& module_name, LogLevel level) {
    auto& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);
    EnsureInitialized();

    const auto& module_config =
        state.module_registry.SetModuleLevel(module_name, level, state.config.level);
    state.logger_registry.SetLevel(module_config.module_name, module_config.level);
}

void LogManager::SetFlushInterval(std::uint32_t interval_seconds) {
    auto& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);
    EnsureInitialized();

    StopFlushWorker(state);
    state.config.flush_interval_seconds = interval_seconds;
    state.stop_flush_worker = false;
    if (interval_seconds > 0) {
        state.flush_worker = std::thread([interval = interval_seconds]() {
            while (!State().stop_flush_worker.load()) {
                std::this_thread::sleep_for(std::chrono::seconds(interval));
                if (State().stop_flush_worker.load()) {
                    break;
                }
                std::lock_guard<std::mutex> lock(State().mutex);
                if (State().initialized) {
                    State().logger_registry.Flush();
                }
            }
        });
    }
}

void LogManager::Flush() {
    auto& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);
    if (!state.initialized) {
        return;
    }

    try {
        state.logger_registry.Flush();
    } catch (const std::exception& e) {
        ReportInternalError("flush", e.what());
    } catch (...) {
        ReportInternalError("flush", "unknown error");
    }
}

void LogManager::Shutdown() {
    auto& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);
    if (!state.initialized) {
        return;
    }

    try {
        StopFlushWorker(state);
        state.module_registry.Clear();
        state.logger_registry.Reset();
        state.thread_pool.reset();
        state.formatter_selector.reset();
        spdlog::shutdown();
        state.initialized = false;
    } catch (const std::exception& e) {
        ReportInternalError("shutdown", e.what());
        state.initialized = false;
    } catch (...) {
        ReportInternalError("shutdown", "unknown error");
        state.initialized = false;
    }
}

void LogManager::WriteRaw(const std::string& module_name,
                          LogLevel level,
                          const std::string& payload) {
    detail::WriteImpl(module_name, level, payload);
}

namespace detail {

void WriteImpl(std::string module_name, LogLevel level, std::string payload) {
    auto& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);
    try {
        EnsureInitialized();

        const auto& module_config =
            state.module_registry.GetOrCreate(module_name, state.config.level);
        state.logger_registry.Write(
            RecordBuilder::Build(module_config, level, std::move(payload)),
            module_config.level);
    } catch (const std::exception& e) {
        ReportInternalError("write_impl", e.what());
        FallbackWrite(module_name, level, payload);
    } catch (...) {
        ReportInternalError("write_impl", "unknown error");
        FallbackWrite(module_name, level, payload);
    }
}

}  // namespace detail
}  // namespace naviai::log
