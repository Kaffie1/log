#include "log_api.hpp"

int main() {
    naviai::log::LogManager::Init(LOG_LEVEL_DEBUG, "/tmp/log_writer_demo");
    naviai::log::LogManager::SetFlushInterval(1);

    LOG_TRACE("LIO_LOCALIZATION", "trace message");
    LOG_DEBUG("LOC", "debug message");
    LOG_INFO("NAVIGATION", "info message");
    LOG_WARN("SYSTEM", "warn message");
    LOG_ERROR("COMMON", "error message");
    LOG_CRITICAL("ADAPTER", "critical message");

    naviai::log::LogManager::Flush();
    naviai::log::LogManager::Shutdown();
    return 0;
}
