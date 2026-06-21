#pragma once

#include <optional>
#include <string_view>

namespace naviai::log::l3 {

enum class LoggerModule {
    Platform,
    Application,
    Algorithm,
    Navigation,
    StateMachine,
    Scheduler,
    Scene,
    Unknown,
};

std::string_view ToString(LoggerModule module);
std::optional<LoggerModule> ParseLoggerModule(std::string_view module_name);

}  // namespace naviai::log::l3
