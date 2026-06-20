#pragma once

#include "log_types.hpp"

#include <string>

namespace naviai::log {

void ReportInternalError(const std::string& stage, const std::string& message);
void FallbackWrite(const std::string& module_name,
                   LogLevel level,
                   const std::string& payload);

}  // namespace naviai::log
