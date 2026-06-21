#include "log_l1.hpp"

#include "level_mapper.hpp"
#include "log_agent.hpp"
#include "log_manager.hpp"
#include "log_service_naming.hpp"

#include <chrono>
#include <filesystem>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <vector>

namespace naviai::log::l1 {
namespace {

struct RuntimeState {
    std::mutex mutex;
    bool initialized{false};
    L1SdkOptions options;
    std::filesystem::path active_path;
    std::unique_ptr<LogAgent> agent;
};

RuntimeState& State() {
    static RuntimeState state;
    return state;
}

std::int64_t NowUs() {
    const auto now = std::chrono::time_point_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now());
    return now.time_since_epoch().count();
}

std::string EscapeJson(std::string_view input) {
    std::string output;
    output.reserve(input.size());
    for (char ch : input) {
        switch (ch) {
            case '\\':
                output += "\\\\";
                break;
            case '"':
                output += "\\\"";
                break;
            case '\n':
                output += "\\n";
                break;
            case '\r':
                output += "\\r";
                break;
            case '\t':
                output += "\\t";
                break;
            default:
                output += ch;
                break;
        }
    }
    return output;
}

std::string TrimLeadingDot(std::string value) {
    if (!value.empty() && value.front() == '.') {
        value.erase(value.begin());
    }
    return value;
}

bool EndsWith(const std::string& value, const std::string& suffix) {
    return value.size() >= suffix.size() &&
           value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

void AppendJsonField(std::string& json,
                     std::string_view key,
                     std::string_view value) {
    if (value.empty()) {
        return;
    }
    json += fmt::format(",\"{}\":\"{}\"", key, EscapeJson(value));
}

std::string BuildStructuredPayload(const L1SdkOptions& options,
                                   std::string_view module_name,
                                   LogLevel level,
                                   const L1LogInput& input,
                                   const char* file,
                                   int line,
                                   const char* func) {
    const auto event_time_us = input.timestamp_us > 0 ? input.timestamp_us : NowUs();
    const auto receive_time_us = NowUs();

    std::string json = fmt::format(
        "{{\"version\":\"1.0\",\"timestamp_us\":{},\"receive_timestamp_us\":{},"
        "\"host_name\":\"{}\",\"layer\":\"L1\",\"module\":\"{}\",\"level\":\"{}\","
        "\"message\":\"{}\"",
        event_time_us,
        receive_time_us,
        EscapeJson(options.host_name),
        EscapeJson(module_name),
        ToLevelName(level),
        EscapeJson(input.message));

    AppendJsonField(json, "event", input.event);
    AppendJsonField(json, "metric_name", input.metric_name);
    AppendJsonField(json, "metric_value", input.metric_value);
    AppendJsonField(json, "threshold", input.threshold);
    AppendJsonField(json, "process", input.process_name);
    if (options.enable_source_location && file != nullptr && func != nullptr) {
        json += fmt::format(",\"file\":\"{}\",\"line\":{},\"func\":\"{}\"",
                            EscapeJson(file),
                            line,
                            EscapeJson(func));
    }
    json += "}";
    return json;
}

LoggerConfig BuildLoggerConfig(const L1SdkOptions& options,
                               const std::filesystem::path& active_path) {
    LoggerConfig config;
    config.root_dir = active_path.parent_path().string();
    config.file_name = active_path.filename().string();
    config.level = options.level;
    config.output_format = OutputFormat::Text;
    config.max_file_size_bytes = options.max_file_size_bytes;
    config.max_files = options.max_files;
    config.async_queue_size = options.async_queue_size;
    config.async_worker_threads = options.async_worker_threads;
    config.flush_interval_seconds = options.flush_interval_seconds;
    config.async_overflow_policy = options.async_overflow_policy;
    config.async_mode = options.async_mode;
    config.enable_basic_file_sink = false;
    config.enable_rotating_file_sink = true;
    config.enable_console_sink = options.enable_console_output;
    return config;
}

void EnsureInitialized() {
    if (!State().initialized) {
        throw std::runtime_error("L1 is not initialized");
    }
}

LogService BuildLogService(const L1SdkOptions& options) {
    return LogService(std::filesystem::path(options.root_dir));
}

std::filesystem::path BuildActiveLogPath(const L1SdkOptions& options) {
    const auto plan = BuildLogService(options).BuildActiveFilePlan("", options.file_name);
    if (!plan.has_value()) {
        throw std::runtime_error("failed to build L1 active log plan");
    }
    return plan->path;
}

void RecoverActivePathIfNeeded(const L1SdkOptions& options,
                               const std::filesystem::path& active_path,
                               std::int64_t end_time_us) {
    if (active_path.empty()) {
        return;
    }
    std::error_code ec;
    if (!std::filesystem::exists(active_path, ec)) {
        return;
    }
    auto task = BuildLogService(options).RecoverActiveFiles({active_path}, end_time_us);
    if (!task.success) {
        throw std::runtime_error("failed to recover active file: " + task.message);
    }
}

void RecoverLingeringActiveFiles(const L1SdkOptions& options) {
    const std::filesystem::path root(options.root_dir);
    std::error_code ec;
    if (!std::filesystem::exists(root, ec)) {
        return;
    }

    const auto expected_suffix = "." + TrimLeadingDot(options.file_name);
    std::vector<std::filesystem::path> active_paths;
    auto service = BuildLogService(options);
    for (const auto& entry : std::filesystem::directory_iterator(root, ec)) {
        if (ec) {
            break;
        }
        if (!entry.is_regular_file()) {
            continue;
        }
        const auto path = entry.path();
        if (!service.IsActiveFilePath(path)) {
            continue;
        }
        if (!EndsWith(path.filename().string(), expected_suffix)) {
            continue;
        }
        active_paths.push_back(path);
    }
    if (active_paths.empty()) {
        return;
    }
    auto task = service.RecoverActiveFiles(active_paths, NowUs());
    if (!task.success) {
        throw std::runtime_error("failed to recover lingering active files: " +
                                 task.message);
    }
}

void RunDrainPass(const L1SdkOptions& options) {
    LogAgent agent(std::filesystem::path(options.root_dir), options.agent_policy);
    auto result = agent.DrainNow();
    if (!result.success) {
        throw std::runtime_error("failed to drain L1 files: " + result.message);
    }
}

}  // namespace

void L1::Init(LogLevel level, const std::string& root_dir) {
    L1SdkOptions options;
    options.level = level;
    options.root_dir = root_dir;
    Init(options);
}

void L1::Init(const L1SdkOptions& options) {
    auto& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);
    if (state.initialized) {
        const auto previous_options = state.options;
        const auto previous_active_path = state.active_path;
        auto agent = std::move(state.agent);
        if (agent) {
            agent->Stop(false);
        }
        LogManager::Flush();
        LogManager::Shutdown();
        state.initialized = false;
        if (previous_options.recover_on_shutdown) {
            RecoverActivePathIfNeeded(previous_options, previous_active_path, NowUs());
        }
        if (previous_options.enable_runtime_agent) {
            RunDrainPass(previous_options);
        }
    }
    if (options.recover_on_init) {
        RecoverLingeringActiveFiles(options);
    }
    if (options.enable_runtime_agent) {
        RunDrainPass(options);
    }
    const auto active_path = BuildActiveLogPath(options);
    LogManager::Init(BuildLoggerConfig(options, active_path));
    state.options = options;
    state.active_path = active_path;
    if (options.enable_runtime_agent) {
        state.agent = std::make_unique<LogAgent>(
            std::filesystem::path(options.root_dir), options.agent_policy);
        auto result = state.agent->Start();
        if (!result.success) {
            state.agent.reset();
            throw std::runtime_error("failed to start L1 agent: " + result.message);
        }
    } else {
        state.agent.reset();
    }
    state.initialized = true;
}

void L1::SetLevel(LogLevel level) {
    LogManager::SetLevel(level);
}

void L1::SetLevel(MonitorModule module, LogLevel level) {
    SetLevel(std::string(ToString(module)), level);
}

void L1::SetLevel(const std::string& module_name, LogLevel level) {
    LogManager::SetLevel(module_name, level);
}

void L1::Flush() {
    LogManager::Flush();
}

void L1::Shutdown() {
    auto& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);
    if (!state.initialized) {
        return;
    }
    const auto options = state.options;
    const auto active_path = state.active_path;
    auto agent = std::move(state.agent);
    if (agent) {
        agent->Stop(false);
    }
    LogManager::Flush();
    LogManager::Shutdown();
    if (options.recover_on_shutdown) {
        RecoverActivePathIfNeeded(options, active_path, NowUs());
    }
    if (options.enable_runtime_agent) {
        RunDrainPass(options);
    }
    state.active_path.clear();
    state.initialized = false;
}

void L1::Write(MonitorModule module,
               LogLevel level,
               std::string_view message,
               const char* file,
               int line,
               const char* func,
               std::int64_t timestamp_us) {
    Write(std::string(ToString(module)),
          level,
          message,
          file,
          line,
          func,
          timestamp_us);
}

void L1::Write(const std::string& module_name,
               LogLevel level,
               std::string_view message,
               const char* file,
               int line,
               const char* func,
               std::int64_t timestamp_us) {
    L1LogInput input;
    input.message = std::string(message);
    input.timestamp_us = timestamp_us;
    auto& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);
    EnsureInitialized();
    LogManager::WriteRaw(module_name,
                         level,
                         BuildStructuredPayload(state.options,
                                                module_name,
                                                level,
                                                input,
                                                file,
                                                line,
                                                func));
}

void L1::Write(const L1LogInput& input,
               const char* file,
               int line,
               const char* func) {
    auto& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);
    EnsureInitialized();
    const auto module_name = std::string(ToString(input.module));
    LogManager::WriteRaw(module_name,
                         input.level,
                         BuildStructuredPayload(state.options,
                                                module_name,
                                                input.level,
                                                input,
                                                file,
                                                line,
                                                func));
}

}  // namespace naviai::log::l1
