#include "monitor_module.hpp"

namespace naviai::log::l1 {

std::string_view ToString(MonitorModule module) {
    switch (module) {
        case MonitorModule::Health:
            return "HEALTH";
        case MonitorModule::Resource:
            return "RESOURCE";
        case MonitorModule::Process:
            return "PROCESS";
        case MonitorModule::Heartbeat:
            return "HEARTBEAT";
        case MonitorModule::Watchdog:
            return "WATCHDOG";
        case MonitorModule::Storage:
            return "STORAGE";
        case MonitorModule::Network:
            return "NETWORK";
        case MonitorModule::System:
            return "SYSTEM";
        case MonitorModule::Unknown:
        default:
            return "UNKNOWN";
    }
}

std::optional<MonitorModule> ParseMonitorModule(std::string_view module_name) {
    if (module_name == "HEALTH") {
        return MonitorModule::Health;
    }
    if (module_name == "RESOURCE") {
        return MonitorModule::Resource;
    }
    if (module_name == "PROCESS") {
        return MonitorModule::Process;
    }
    if (module_name == "HEARTBEAT") {
        return MonitorModule::Heartbeat;
    }
    if (module_name == "WATCHDOG") {
        return MonitorModule::Watchdog;
    }
    if (module_name == "STORAGE") {
        return MonitorModule::Storage;
    }
    if (module_name == "NETWORK") {
        return MonitorModule::Network;
    }
    if (module_name == "SYSTEM") {
        return MonitorModule::System;
    }
    if (module_name == "UNKNOWN") {
        return MonitorModule::Unknown;
    }
    return std::nullopt;
}

}  // namespace naviai::log::l1
