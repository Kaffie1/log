#pragma once

#include <map>
#include <string>

namespace naviai::log_module {

enum class LogLevel {
    Trace,
    Debug,
    Info,
    Warn,
    Error,
    Critical,
    Off,
};

struct ModuleOptions {
    std::string category;
    std::string subtype;
    std::string event_type;
    std::string output_group;
};

using PublicLogExtra = std::map<std::string, std::string>;

struct PublicLogContext {
    std::string trace_id;
    std::string source_id;
    std::string host;
    std::string process_name;
    int pid{0};
};

}  // namespace naviai::log_module
