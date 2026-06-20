#include "../include/log.hpp"

#include "log_core.hpp"

namespace naviai::log_module {

ModuleMetadata BuildL3ModuleMetadata(std::string_view module_name,
                                     const ModuleRegistrationOptions& options) {
    ModuleMetadata metadata;
    metadata.module_name = std::string(module_name);
    metadata.layer_metadata.layer = LogLayer::L3;
    metadata.layer_metadata.category =
        options.category.empty() ? "module" : options.category;
    metadata.layer_metadata.subtype =
        options.subtype.empty() ? "runtime" : options.subtype;
    metadata.layer_metadata.event_type =
        options.event_type.empty() ? "event" : options.event_type;
    metadata.output_group =
        options.output_group.empty() ? metadata.module_name : options.output_group;
    return metadata;
}

void L3::Init(LogLevel level, const std::string& root_dir) {
    LogConfig config;
    config.root_dir = root_dir;
    config.default_level = ToLevelName(level);
    config.enable_layer_aggregate = true;
    LogApi::Init(config);
}

void L3::RegisterModule(const std::string& module_name,
                        const ModuleOptions& options) {
    LogApi::RegisterModule(
        BuildL3ModuleMetadata(module_name, ToInternalOptions(options)));
}

void L3::Write(const std::string& module_name,
               LogLevel level,
               const std::string& message,
               const PublicLogContext& context,
               const PublicLogExtra& extra) {
    LogApi::Write(module_name, ToSpdLogLevel(level), message,
                  ToInternalContext(context), extra);
}

void L3::SetLevel(const std::string& module_name, LogLevel level) {
    LogApi::SetLevel(module_name, ToSpdLogLevel(level));
}

void L3::SetLevelForAll(LogLevel level) {
    LogApi::SetLevelForAll(ToSpdLogLevel(level));
}

void L3::Flush() {
    LogApi::Flush();
}

void L3::Shutdown() {
    LogApi::Shutdown();
}

}  // namespace naviai::log_module
