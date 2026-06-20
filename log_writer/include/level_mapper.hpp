#pragma once

#include "log_types.hpp"

#include <spdlog/async.h>
#include <spdlog/common.h>

namespace naviai::log {

spdlog::level::level_enum ToSpdlogLevel(LogLevel level);
const char* ToLevelName(LogLevel level);
spdlog::async_overflow_policy ToSpdlogOverflowPolicy(
    AsyncOverflowPolicy policy);

}  // namespace naviai::log
