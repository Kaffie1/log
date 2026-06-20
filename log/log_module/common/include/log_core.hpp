#pragma once

#include <map>
#include <memory>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <fmt/core.h>

#if defined(FMT_VERSION) && FMT_VERSION >= 110000
namespace fmt {
template <typename Char>
using basic_runtime = runtime_format_string<Char>;
}
#endif

#include <spdlog/common.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/null_sink.h>

#include "async_dispatcher.hpp"
#include "file_lifecycle.hpp"
#include "log_config.hpp"
#include "log_types.hpp"

namespace naviai::log_module {

enum class LogLayer {
    L1,
    L2,
    L3,
};

const char* ToString(LogLayer layer);
spdlog::level::level_enum ToSpdLogLevel(LogLevel level);
const char* ToLevelName(LogLevel level);

struct LayerMetadata {
    LogLayer layer{LogLayer::L3};
    std::string category;
    std::string subtype;
    std::string event_type;
};

struct ModuleMetadata {
    std::string module_name;
    LayerMetadata layer_metadata;
    std::string output_group;
};

struct ModuleRegistrationOptions {
    std::string category;
    std::string subtype;
    std::string event_type;
    std::string output_group;
};

ModuleRegistrationOptions ToInternalOptions(const ModuleOptions& options);
ModuleMetadata BuildL1ModuleMetadata(
    std::string_view module_name,
    const ModuleRegistrationOptions& options = {});
ModuleMetadata BuildL2ModuleMetadata(
    std::string_view module_name,
    const ModuleRegistrationOptions& options = {});
ModuleMetadata BuildL3ModuleMetadata(
    std::string_view module_name,
    const ModuleRegistrationOptions& options = {});

using LogExtra = std::map<std::string, std::string>;

struct StructuredPayload {
    std::string encoding{"text"};
    std::string content;
};

struct LogContext {
    std::string trace_id;
    std::string source_id;
    std::string host;
    std::string process_name;
    int pid{0};
};

LogContext ToInternalContext(const PublicLogContext& context);

struct LogRecord {
    int64_t timestamp_us{0};
    std::string module;
    std::string output_group;
    spdlog::level::level_enum level{spdlog::level::info};
    std::string message;
    LayerMetadata layer_metadata;
    std::string category;
    std::string subtype;
    std::string event_type;
    LogContext context;
    LogExtra extra;
    StructuredPayload payload;
};

class LayerMetadataRegistry {
  public:
    void RegisterModule(const ModuleMetadata& metadata);
    const ModuleMetadata& Get(std::string_view module_name) const;
    bool Has(std::string_view module_name) const;
    void Clear();

  private:
    std::unordered_map<std::string, ModuleMetadata> modules_;
};

class LayerValidator {
  public:
    explicit LayerValidator(const LayerMetadataRegistry* registry);
    LogRecord BuildRecord(const std::string& module_name,
                          spdlog::level::level_enum level,
                          const std::string& message,
                          const LogContext& context,
                          const LogExtra& extra) const;
    LogRecord NormalizeRecord(const LogRecord& record) const;

  private:
    const LayerMetadataRegistry* registry_;
};

class LoggerRegistry {
  public:
    void Register(const std::string& module_name,
                  spdlog::level::level_enum default_level);
    std::shared_ptr<spdlog::logger> Get(const std::string& module_name) const;
    void SetLevel(const std::string& module_name, spdlog::level::level_enum level);
    void SetLevelForAll(spdlog::level::level_enum level);
    void Clear();

  private:
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, std::shared_ptr<spdlog::logger>> loggers_;
};

class Formatter {
  public:
    virtual ~Formatter() = default;
    virtual std::string Format(const LogRecord& record) const = 0;
};

class TextFormatter : public Formatter {
  public:
    std::string Format(const LogRecord& record) const override;
};

class JsonFormatter : public Formatter {
  public:
    std::string Format(const LogRecord& record) const override;
};

class FormatterSelector {
  public:
    FormatterSelector(std::shared_ptr<Formatter> text_formatter,
                      std::shared_ptr<Formatter> json_formatter);
    std::shared_ptr<Formatter> Select(bool json_output) const;

  private:
    std::shared_ptr<Formatter> text_formatter_;
    std::shared_ptr<Formatter> json_formatter_;
};

class SinkAdapter {
  public:
    virtual ~SinkAdapter() = default;
    virtual void Write(const std::string& module_name,
                       const std::string& output_group,
                       const std::string& layer_name,
                       const std::string& formatted_line) = 0;
    virtual void Flush() = 0;
};

class ConsoleSinkAdapter : public SinkAdapter {
  public:
    void Write(const std::string& module_name,
               const std::string& output_group,
               const std::string& layer_name,
               const std::string& formatted_line) override;
    void Flush() override;
};

class FileSinkAdapter : public SinkAdapter {
  public:
    explicit FileSinkAdapter(std::unique_ptr<FileLifecycleManager> lifecycle);
    void Write(const std::string& module_name,
               const std::string& output_group,
               const std::string& layer_name,
               const std::string& formatted_line) override;
    void Flush() override;

  private:
    std::unique_ptr<FileLifecycleManager> lifecycle_;
};

class ModuleFileSinkAdapter : public SinkAdapter {
  public:
    ModuleFileSinkAdapter(std::filesystem::path root_dir, LogConfig config);
    void Write(const std::string& module_name,
               const std::string& output_group,
               const std::string& layer_name,
               const std::string& formatted_line) override;
    void Flush() override;

  private:
    std::filesystem::path root_dir_;
    LogConfig config_;
    std::unordered_map<std::string, std::unique_ptr<FileLifecycleManager>>
        managers_;
};

class LayerFileSinkAdapter : public SinkAdapter {
  public:
    LayerFileSinkAdapter(std::filesystem::path root_dir, LogConfig config);
    void Write(const std::string& module_name,
               const std::string& output_group,
               const std::string& layer_name,
               const std::string& formatted_line) override;
    void Flush() override;

  private:
    std::filesystem::path root_dir_;
    LogConfig config_;
    std::unordered_map<std::string, std::unique_ptr<FileLifecycleManager>>
        managers_;
};

class CompositeSinkAdapter : public SinkAdapter {
  public:
    void AddSink(std::shared_ptr<SinkAdapter> sink);
    void Write(const std::string& module_name,
               const std::string& output_group,
               const std::string& layer_name,
               const std::string& formatted_line) override;
    void Flush() override;

  private:
    std::vector<std::shared_ptr<SinkAdapter>> sinks_;
};

class SinkFactory {
  public:
    static std::shared_ptr<SinkAdapter> Create(const LogConfig& config);
};

class LogApi {
  public:
    static void Init(const LogConfig& config);
    static void RegisterModule(const ModuleMetadata& metadata);
    static std::shared_ptr<spdlog::logger> GetLogger(
        const std::string& module_name);
    static void WriteRecord(const LogRecord& record);
    static void Write(const std::string& module_name,
                      spdlog::level::level_enum level,
                      const std::string& message,
                      const LogContext& context = {},
                      const LogExtra& extra = {});
    static void SetLevel(const std::string& module_name,
                         spdlog::level::level_enum level);
    static void SetLevelForAll(spdlog::level::level_enum level);
    static void Flush();
    static void Shutdown();
};

}  // namespace naviai::log_module
