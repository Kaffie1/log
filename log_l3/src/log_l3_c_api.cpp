#include "log_l3_c_api.hpp"

#include "log_l3.hpp"
#include "logger_module.hpp"

#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>

namespace naviai::log::l3::c_api {
namespace {

std::mutex& ErrorMutex() {
    static std::mutex mutex;
    return mutex;
}

std::string& LastError() {
    static std::string error;
    return error;
}

void SetLastError(const std::string& error) {
    std::lock_guard<std::mutex> lock(ErrorMutex());
    LastError() = error;
}

void ClearLastError() {
    SetLastError("");
}

std::optional<LogLevel> ParseLevel(const char* level_name) {
    if (level_name == nullptr) {
        return std::nullopt;
    }
    const std::string level(level_name);
    if (level == "TRACE") {
        return LogLevel::Trace;
    }
    if (level == "DEBUG") {
        return LogLevel::Debug;
    }
    if (level == "INFO") {
        return LogLevel::Info;
    }
    if (level == "WARN" || level == "WARNING") {
        return LogLevel::Warn;
    }
    if (level == "ERROR") {
        return LogLevel::Error;
    }
    if (level == "CRITICAL") {
        return LogLevel::Critical;
    }
    return std::nullopt;
}

}  // namespace
}  // namespace naviai::log::l3::c_api

extern "C" {

int naviai_log_l3_init(const char* level_name,
                       const char* root_dir,
                       const char* robot_sn) {
    using namespace naviai::log::l3;
    try {
        auto level = c_api::ParseLevel(level_name);
        if (!level.has_value()) {
            throw std::invalid_argument("invalid level");
        }
        if (root_dir == nullptr || std::string(root_dir).empty()) {
            throw std::invalid_argument("root_dir must not be empty");
        }
        L3SdkOptions options;
        options.level = *level;
        options.root_dir = root_dir;
        options.robot_sn = robot_sn == nullptr ? "" : robot_sn;
        L3::Init(options);
        c_api::ClearLastError();
        return 0;
    } catch (const std::exception& ex) {
        c_api::SetLastError(ex.what());
        return -1;
    }
}

int naviai_log_l3_write(const char* module_name,
                        const char* level_name,
                        const char* payload,
                        std::int64_t timestamp_us) {
    using namespace naviai::log::l3;
    try {
        auto module = ParseLoggerModule(module_name == nullptr ? "" : module_name);
        if (!module.has_value()) {
            throw std::invalid_argument("invalid module");
        }
        auto level = c_api::ParseLevel(level_name);
        if (!level.has_value()) {
            throw std::invalid_argument("invalid level");
        }
        L3::Write(L3LogInput{
            *module,
            *level,
            payload == nullptr ? "" : payload,
            timestamp_us,
        });
        c_api::ClearLastError();
        return 0;
    } catch (const std::exception& ex) {
        c_api::SetLastError(ex.what());
        return -1;
    }
}

int naviai_log_l3_set_level(const char* level_name) {
    using namespace naviai::log::l3;
    try {
        auto level = c_api::ParseLevel(level_name);
        if (!level.has_value()) {
            throw std::invalid_argument("invalid level");
        }
        L3::SetLevel(*level);
        c_api::ClearLastError();
        return 0;
    } catch (const std::exception& ex) {
        c_api::SetLastError(ex.what());
        return -1;
    }
}

int naviai_log_l3_set_module_level(const char* module_name, const char* level_name) {
    using namespace naviai::log::l3;
    try {
        auto module = ParseLoggerModule(module_name == nullptr ? "" : module_name);
        if (!module.has_value()) {
            throw std::invalid_argument("invalid module");
        }
        auto level = c_api::ParseLevel(level_name);
        if (!level.has_value()) {
            throw std::invalid_argument("invalid level");
        }
        L3::SetLevel(*module, *level);
        c_api::ClearLastError();
        return 0;
    } catch (const std::exception& ex) {
        c_api::SetLastError(ex.what());
        return -1;
    }
}

void naviai_log_l3_flush() {
    try {
        naviai::log::l3::L3::Flush();
        naviai::log::l3::c_api::ClearLastError();
    } catch (const std::exception& ex) {
        naviai::log::l3::c_api::SetLastError(ex.what());
    }
}

void naviai_log_l3_shutdown() {
    try {
        naviai::log::l3::L3::Shutdown();
        naviai::log::l3::c_api::ClearLastError();
    } catch (const std::exception& ex) {
        naviai::log::l3::c_api::SetLastError(ex.what());
    }
}

const char* naviai_log_l3_last_error() {
    std::lock_guard<std::mutex> lock(naviai::log::l3::c_api::ErrorMutex());
    return naviai::log::l3::c_api::LastError().c_str();
}

}
