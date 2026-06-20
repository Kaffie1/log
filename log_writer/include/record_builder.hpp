#pragma once

#include "log_types.hpp"
#include "module_registry.hpp"

#include <string>

namespace naviai::log {

class RecordBuilder {
  public:
    static LogRecord Build(const ModuleConfig& module_config,
                           LogLevel level,
                           std::string payload);
};

}  // namespace naviai::log
