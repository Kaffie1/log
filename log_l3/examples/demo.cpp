#include "log_l3.hpp"

using naviai::log::LogLevel;
using naviai::log::l3::L3;
using naviai::log::l3::LoggerModule;

int main() {
    L3::Init(LogLevel::Info, "/tmp/logs/l3_demo");
    L3_LOG_INFO(LoggerModule::Navigation, "task started");
    L3_LOG_WARN(LoggerModule::Scheduler, "fallback triggered");
    L3_LOG_ERROR(LoggerModule::Application, "planner failed");
    L3::Flush();
    L3::Shutdown();
    return 0;
}
