#include "record_builder.hpp"

#include <chrono>
#include <utility>

namespace naviai::log {

LogRecord RecordBuilder::Build(const ModuleConfig& module_config,
                               LogLevel level,
                               std::string payload) {
    const auto now = std::chrono::time_point_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now());

    LogRecord record;
    record.timestamp_us = now.time_since_epoch().count();
    record.module_name = module_config.module_name;
    record.output_group = module_config.output_group;
    record.level = level;
    record.payload = std::move(payload);
    return record;
}

}  // namespace naviai::log
