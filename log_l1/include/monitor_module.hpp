#pragma once

#include <optional>
#include <string_view>

namespace naviai::log::l1 {

enum class MonitorModule {
    Health,
    Resource,
    Process,
    Heartbeat,
    Watchdog,
    Storage,
    Network,
    System,
    Unknown,
};

std::string_view ToString(MonitorModule module);
std::optional<MonitorModule> ParseMonitorModule(std::string_view module_name);

}  // namespace naviai::log::l1
