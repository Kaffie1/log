#include "logger_module.hpp"

namespace naviai::log::l3 {

std::string_view ToString(LoggerModule module) {
    switch (module) {
        case LoggerModule::Platform:
            return "PLATFORM";
        case LoggerModule::Application:
            return "APPLICATION";
        case LoggerModule::Algorithm:
            return "ALGORITHM";
        case LoggerModule::Navigation:
            return "NAVIGATION";
        case LoggerModule::StateMachine:
            return "STATE_MACHINE";
        case LoggerModule::Scheduler:
            return "SCHEDULER";
        case LoggerModule::Scene:
            return "SCENE";
        case LoggerModule::Unknown:
        default:
            return "UNKNOWN";
    }
}

std::optional<LoggerModule> ParseLoggerModule(std::string_view module_name) {
    if (module_name == "PLATFORM") {
        return LoggerModule::Platform;
    }
    if (module_name == "APPLICATION") {
        return LoggerModule::Application;
    }
    if (module_name == "ALGORITHM") {
        return LoggerModule::Algorithm;
    }
    if (module_name == "NAVIGATION") {
        return LoggerModule::Navigation;
    }
    if (module_name == "STATE_MACHINE") {
        return LoggerModule::StateMachine;
    }
    if (module_name == "SCHEDULER") {
        return LoggerModule::Scheduler;
    }
    if (module_name == "SCENE") {
        return LoggerModule::Scene;
    }
    if (module_name == "UNKNOWN") {
        return LoggerModule::Unknown;
    }
    return std::nullopt;
}

}  // namespace naviai::log::l3
