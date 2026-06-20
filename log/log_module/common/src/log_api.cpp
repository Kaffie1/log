#include "log_core.hpp"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <stdexcept>

namespace naviai::log_module {

namespace {

int64_t NowTimestampUs() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

spdlog::level::level_enum ParseLevel(const std::string& level_name) {
    return spdlog::level::from_str(level_name);
}

std::string FormatTimestampUs(int64_t timestamp_us) {
    const auto sec = static_cast<std::time_t>(timestamp_us / 1000000);
    const auto micro = timestamp_us % 1000000;
    std::tm tm_buf{};
    localtime_r(&sec, &tm_buf);

    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S") << '.'
        << std::setw(6) << std::setfill('0') << micro;
    return oss.str();
}

const char* LevelName(spdlog::level::level_enum level) {
    return spdlog::level::to_string_view(level).data();
}

std::string EscapeJson(const std::string& value) {
    std::string output;
    output.reserve(value.size() + 8);
    for (const char ch : value) {
        switch (ch) {
            case '"':
                output += "\\\"";
                break;
            case '\\':
                output += "\\\\";
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
                output.push_back(ch);
                break;
        }
    }
    return output;
}

struct GlobalState {
    LogConfig config;
    LayerMetadataRegistry registry;
    LoggerRegistry logger_registry;
    std::shared_ptr<Formatter> text_formatter;
    std::shared_ptr<Formatter> json_formatter;
    std::shared_ptr<SinkAdapter> sink;
    std::unique_ptr<AsyncDispatcher> dispatcher;
    std::unique_ptr<FlushScheduler> flush_scheduler;
    std::unique_ptr<LayerValidator> validator;
    bool initialized{false};
    std::mutex mutex;
};

GlobalState& State() {
    static GlobalState state;
    return state;
}

}  // namespace

spdlog::level::level_enum ToSpdLogLevel(LogLevel level) {
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
        case LogLevel::Off:
        default:
            return spdlog::level::off;
    }
}

const char* ToLevelName(LogLevel level) {
    switch (level) {
        case LogLevel::Trace:
            return "trace";
        case LogLevel::Debug:
            return "debug";
        case LogLevel::Info:
            return "info";
        case LogLevel::Warn:
            return "warn";
        case LogLevel::Error:
            return "error";
        case LogLevel::Critical:
            return "critical";
        case LogLevel::Off:
        default:
            return "off";
    }
}

ModuleRegistrationOptions ToInternalOptions(const ModuleOptions& options) {
    ModuleRegistrationOptions internal;
    internal.category = options.category;
    internal.subtype = options.subtype;
    internal.event_type = options.event_type;
    internal.output_group = options.output_group;
    return internal;
}

LogContext ToInternalContext(const PublicLogContext& context) {
    LogContext internal;
    internal.trace_id = context.trace_id;
    internal.source_id = context.source_id;
    internal.host = context.host;
    internal.process_name = context.process_name;
    internal.pid = context.pid;
    return internal;
}

const char* ToString(LogLayer layer) {
    switch (layer) {
        case LogLayer::L1:
            return "L1";
        case LogLayer::L2:
            return "L2";
        case LogLayer::L3:
            return "L3";
        default:
            return "L3";
    }
}

void LayerMetadataRegistry::RegisterModule(const ModuleMetadata& metadata) {
    modules_[metadata.module_name] = metadata;
}

const ModuleMetadata& LayerMetadataRegistry::Get(std::string_view module_name) const {
    const auto it = modules_.find(std::string(module_name));
    if (it == modules_.end()) {
        throw std::runtime_error("module metadata not found: " +
                                 std::string(module_name));
    }
    return it->second;
}

bool LayerMetadataRegistry::Has(std::string_view module_name) const {
    return modules_.count(std::string(module_name)) > 0;
}

void LayerMetadataRegistry::Clear() {
    modules_.clear();
}

LayerValidator::LayerValidator(const LayerMetadataRegistry* registry)
    : registry_(registry) {}

LogRecord LayerValidator::BuildRecord(const std::string& module_name,
                                      spdlog::level::level_enum level,
                                      const std::string& message,
                                      const LogContext& context,
                                      const LogExtra& extra) const {
    if (registry_ == nullptr) {
        throw std::runtime_error("layer registry is not initialized");
    }

    const auto& metadata = registry_->Get(module_name);
    LogRecord record;
    record.timestamp_us = NowTimestampUs();
    record.module = module_name;
    record.output_group = metadata.output_group;
    record.level = level;
    record.message = message;
    record.layer_metadata = metadata.layer_metadata;
    record.category = metadata.layer_metadata.category;
    record.subtype = metadata.layer_metadata.subtype;
    record.event_type = metadata.layer_metadata.event_type;
    record.context = context;
    record.extra = extra;
    return record;
}

LogRecord LayerValidator::NormalizeRecord(const LogRecord& record) const {
    if (registry_ == nullptr) {
        throw std::runtime_error("layer registry is not initialized");
    }
    if (record.module.empty()) {
        throw std::invalid_argument("record.module must not be empty");
    }

    const auto& metadata = registry_->Get(record.module);
    LogRecord normalized = record;
    if (normalized.timestamp_us <= 0) {
        normalized.timestamp_us = NowTimestampUs();
    }
    normalized.output_group = normalized.output_group.empty()
                                  ? metadata.output_group
                                  : normalized.output_group;
    normalized.layer_metadata = metadata.layer_metadata;
    normalized.category = normalized.category.empty()
                              ? metadata.layer_metadata.category
                              : normalized.category;
    normalized.subtype = normalized.subtype.empty()
                             ? metadata.layer_metadata.subtype
                             : normalized.subtype;
    normalized.event_type = normalized.event_type.empty()
                                ? metadata.layer_metadata.event_type
                                : normalized.event_type;
    return normalized;
}

void LoggerRegistry::Register(const std::string& module_name,
                              spdlog::level::level_enum default_level) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    const auto it = loggers_.find(module_name);
    if (it != loggers_.end()) {
        it->second->set_level(default_level);
        return;
    }

    auto sink = std::make_shared<spdlog::sinks::null_sink_mt>();
    auto logger = std::make_shared<spdlog::logger>(module_name, sink);
    logger->set_level(default_level);
    loggers_.emplace(module_name, std::move(logger));
}

std::shared_ptr<spdlog::logger> LoggerRegistry::Get(
    const std::string& module_name) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    const auto it = loggers_.find(module_name);
    if (it == loggers_.end()) {
        throw std::runtime_error("logger module is not registered: " + module_name);
    }
    return it->second;
}

void LoggerRegistry::SetLevel(const std::string& module_name,
                              spdlog::level::level_enum level) {
    Get(module_name)->set_level(level);
}

void LoggerRegistry::SetLevelForAll(spdlog::level::level_enum level) {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    for (const auto& item : loggers_) {
        item.second->set_level(level);
    }
}

void LoggerRegistry::Clear() {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    loggers_.clear();
}

std::string TextFormatter::Format(const LogRecord& record) const {
    std::ostringstream oss;
    oss << '[' << FormatTimestampUs(record.timestamp_us) << "] "
        << '[' << ToString(record.layer_metadata.layer) << "] "
        << '[' << LevelName(record.level) << "] "
        << '[' << record.module << "] " << record.message;
    if (!record.context.trace_id.empty()) {
        oss << " trace_id=" << record.context.trace_id;
    }
    if (!record.context.source_id.empty()) {
        oss << " source_id=" << record.context.source_id;
    }
    if (!record.context.host.empty()) {
        oss << " host=" << record.context.host;
    }
    if (!record.context.process_name.empty()) {
        oss << " process_name=" << record.context.process_name;
    }
    if (record.context.pid > 0) {
        oss << " pid=" << record.context.pid;
    }
    if (!record.output_group.empty()) {
        oss << " output_group=" << record.output_group;
    }
    if (!record.payload.encoding.empty() && !record.payload.content.empty()) {
        oss << " payload_encoding=" << record.payload.encoding;
    }
    for (const auto& item : record.extra) {
        oss << ' ' << item.first << '=' << item.second;
    }
    return oss.str();
}

std::string JsonFormatter::Format(const LogRecord& record) const {
    std::ostringstream oss;
    oss << '{'
        << "\"timestamp_us\":" << record.timestamp_us << ','
        << "\"layer\":\"" << ToString(record.layer_metadata.layer) << "\","
        << "\"module\":\"" << EscapeJson(record.module) << "\","
        << "\"output_group\":\"" << EscapeJson(record.output_group) << "\","
        << "\"level\":\"" << spdlog::level::to_string_view(record.level).data()
        << "\","
        << "\"category\":\"" << EscapeJson(record.category) << "\","
        << "\"subtype\":\"" << EscapeJson(record.subtype) << "\","
        << "\"event_type\":\"" << EscapeJson(record.event_type) << "\","
        << "\"trace_id\":\"" << EscapeJson(record.context.trace_id) << "\","
        << "\"source_id\":\"" << EscapeJson(record.context.source_id) << "\","
        << "\"host\":\"" << EscapeJson(record.context.host) << "\","
        << "\"process_name\":\"" << EscapeJson(record.context.process_name) << "\","
        << "\"pid\":" << record.context.pid << ','
        << "\"payload_encoding\":\"" << EscapeJson(record.payload.encoding) << "\","
        << "\"message\":\"" << EscapeJson(record.message) << "\"";
    if (!record.payload.content.empty()) {
        oss << ",\"payload\":\"" << EscapeJson(record.payload.content) << '"';
    }
    for (const auto& item : record.extra) {
        oss << ",\"" << EscapeJson(item.first) << "\":\""
            << EscapeJson(item.second) << '"';
    }
    oss << '}';
    return oss.str();
}

FormatterSelector::FormatterSelector(std::shared_ptr<Formatter> text_formatter,
                                     std::shared_ptr<Formatter> json_formatter)
    : text_formatter_(std::move(text_formatter)),
      json_formatter_(std::move(json_formatter)) {}

std::shared_ptr<Formatter> FormatterSelector::Select(bool json_output) const {
    return json_output ? json_formatter_ : text_formatter_;
}

void ConsoleSinkAdapter::Write(const std::string& module_name,
                               const std::string& output_group,
                               const std::string& layer_name,
                               const std::string& formatted_line) {
    (void)module_name;
    (void)output_group;
    (void)layer_name;
    std::cout << formatted_line << '\n';
}

void ConsoleSinkAdapter::Flush() {
    std::cout.flush();
}

FileSinkAdapter::FileSinkAdapter(std::unique_ptr<FileLifecycleManager> lifecycle)
    : lifecycle_(std::move(lifecycle)) {}

void FileSinkAdapter::Write(const std::string& module_name,
                            const std::string& output_group,
                            const std::string& layer_name,
                            const std::string& formatted_line) {
    (void)module_name;
    (void)output_group;
    (void)layer_name;
    auto& stream = lifecycle_->AcquireStream(formatted_line.size() + 1);
    stream << formatted_line << '\n';
    lifecycle_->AdvanceSize(formatted_line.size() + 1);
}

void FileSinkAdapter::Flush() {
    lifecycle_->Flush();
}

ModuleFileSinkAdapter::ModuleFileSinkAdapter(std::filesystem::path root_dir,
                                             LogConfig config)
    : root_dir_(std::move(root_dir)), config_(std::move(config)) {}

void ModuleFileSinkAdapter::Write(const std::string& module_name,
                                  const std::string& output_group,
                                  const std::string& layer_name,
                                  const std::string& formatted_line) {
    (void)layer_name;
    auto& manager = managers_[module_name];
    if (!manager) {
        std::filesystem::path module_dir = root_dir_;
        if (!output_group.empty()) {
            module_dir /= output_group;
        }
        manager = std::make_unique<FileLifecycleManager>(
            config_.ResolveModuleFilePolicy(module_name, layer_name),
            module_dir, module_name);
    }
    auto& stream = manager->AcquireStream(formatted_line.size() + 1);
    stream << formatted_line << '\n';
    manager->AdvanceSize(formatted_line.size() + 1);
}

void ModuleFileSinkAdapter::Flush() {
    for (auto& item : managers_) {
        item.second->Flush();
    }
}

LayerFileSinkAdapter::LayerFileSinkAdapter(std::filesystem::path root_dir,
                                           LogConfig config)
    : root_dir_(std::move(root_dir)), config_(std::move(config)) {}

void LayerFileSinkAdapter::Write(const std::string& module_name,
                                 const std::string& output_group,
                                 const std::string& layer_name,
                                 const std::string& formatted_line) {
    (void)module_name;
    (void)output_group;
    auto& manager = managers_[layer_name];
    if (!manager) {
        manager = std::make_unique<FileLifecycleManager>(
            config_.ResolveLayerFilePolicy(layer_name), root_dir_ / layer_name,
            layer_name);
    }
    auto& stream = manager->AcquireStream(formatted_line.size() + 1);
    stream << formatted_line << '\n';
    manager->AdvanceSize(formatted_line.size() + 1);
}

void LayerFileSinkAdapter::Flush() {
    for (auto& item : managers_) {
        item.second->Flush();
    }
}

void CompositeSinkAdapter::AddSink(std::shared_ptr<SinkAdapter> sink) {
    if (sink) {
        sinks_.push_back(std::move(sink));
    }
}

void CompositeSinkAdapter::Write(const std::string& module_name,
                                 const std::string& output_group,
                                 const std::string& layer_name,
                                 const std::string& formatted_line) {
    for (const auto& sink : sinks_) {
        sink->Write(module_name, output_group, layer_name, formatted_line);
    }
}

void CompositeSinkAdapter::Flush() {
    for (const auto& sink : sinks_) {
        sink->Flush();
    }
}

std::shared_ptr<SinkAdapter> SinkFactory::Create(const LogConfig& config) {
    auto composite = std::make_shared<CompositeSinkAdapter>();

    if (config.sink.enable_console) {
        composite->AddSink(std::make_shared<ConsoleSinkAdapter>());
    }
    if (config.sink.enable_module_file) {
        composite->AddSink(std::make_shared<ModuleFileSinkAdapter>(
            std::filesystem::path(config.root_dir) / "module", config));
    }
    if (config.sink.enable_layer_file || config.enable_layer_aggregate) {
        composite->AddSink(std::make_shared<LayerFileSinkAdapter>(
            std::filesystem::path(config.root_dir) / "layer", config));
    }
    return composite;
}

void LogApi::Init(const LogConfig& config) {
    auto& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);
    if (state.initialized) {
        if (state.flush_scheduler) {
            state.flush_scheduler->Stop();
        }
        if (state.dispatcher) {
            state.dispatcher->Stop();
        }
        if (state.sink) {
            state.sink->Flush();
        }
        state.registry.Clear();
        state.logger_registry.Clear();
    }
    state.config = config;
    state.config.Validate();
    state.text_formatter = std::make_shared<TextFormatter>();
    state.json_formatter = std::make_shared<JsonFormatter>();
    state.sink = SinkFactory::Create(state.config);
    state.dispatcher = std::make_unique<AsyncDispatcher>(
        state.config.async_worker_threads, state.config.async_queue_size);
    state.flush_scheduler =
        std::make_unique<FlushScheduler>(state.config.flush_interval_seconds);
    state.validator = std::make_unique<LayerValidator>(&state.registry);
    state.dispatcher->Start();
    state.flush_scheduler->Start([]() { LogApi::Flush(); });
    state.initialized = true;
}

void LogApi::RegisterModule(const ModuleMetadata& metadata) {
    auto& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);
    state.registry.RegisterModule(metadata);
    state.logger_registry.Register(metadata.module_name,
                                   ParseLevel(state.config.default_level));
}

std::shared_ptr<spdlog::logger> LogApi::GetLogger(
    const std::string& module_name) {
    return State().logger_registry.Get(module_name);
}

void LogApi::WriteRecord(const LogRecord& record) {
    auto& state = State();
    if (!state.initialized) {
        throw std::runtime_error("LogApi is not initialized");
    }

    const auto logger = state.logger_registry.Get(record.module);
    if (!logger->should_log(record.level)) {
        return;
    }

    const LogRecord normalized = state.validator->NormalizeRecord(record);
    const FormatterSelector selector(state.text_formatter, state.json_formatter);
    const auto formatter = selector.Select(state.config.enable_json_output);
    const std::string line = formatter->Format(normalized);
    const std::string layer_name = ToString(normalized.layer_metadata.layer);
    const std::string output_group = normalized.output_group;
    const std::string module_name = normalized.module;

    state.dispatcher->Dispatch(
        [sink = state.sink, module_name, output_group, layer_name, line]() {
            sink->Write(module_name, output_group, layer_name, line);
        });
}

void LogApi::Write(const std::string& module_name,
                   spdlog::level::level_enum level,
                   const std::string& message,
                   const LogContext& context,
                   const LogExtra& extra) {
    auto& state = State();
    if (!state.initialized) {
        throw std::runtime_error("LogApi is not initialized");
    }
    const auto logger = state.logger_registry.Get(module_name);
    if (!logger->should_log(level)) {
        return;
    }

    const LogRecord record =
        state.validator->BuildRecord(module_name, level, message, context, extra);
    WriteRecord(record);
}

void LogApi::SetLevel(const std::string& module_name,
                      spdlog::level::level_enum level) {
    State().logger_registry.SetLevel(module_name, level);
}

void LogApi::SetLevelForAll(spdlog::level::level_enum level) {
    State().logger_registry.SetLevelForAll(level);
}

void LogApi::Flush() {
    auto& state = State();
    if (state.sink) {
        state.sink->Flush();
    }
}

void LogApi::Shutdown() {
    auto& state = State();
    if (state.flush_scheduler) {
        state.flush_scheduler->Stop();
    }
    if (state.dispatcher) {
        state.dispatcher->Stop();
    }
    Flush();
    state.registry.Clear();
    state.logger_registry.Clear();
    state.text_formatter.reset();
    state.json_formatter.reset();
    state.sink.reset();
    state.dispatcher.reset();
    state.flush_scheduler.reset();
    state.validator.reset();
    state.initialized = false;
}

}  // namespace naviai::log_module
