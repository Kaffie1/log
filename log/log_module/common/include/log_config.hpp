#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>

namespace naviai::log_module {

struct FileRotationPolicy {
    size_t max_file_size_bytes{20U * 1024U * 1024U};
    size_t max_files{100U};
};

struct SinkConfig {
    bool enable_console{true};
    bool enable_module_file{true};
    bool enable_layer_file{false};
    bool json_output{false};
    size_t module_file_max_file_size_bytes{0};
    size_t layer_file_max_file_size_bytes{0};
    size_t module_file_max_files{0};
    size_t layer_file_max_files{0};
};

struct ModuleConfig {
    size_t max_file_size_bytes{0};
};

struct LayerConfig {
    size_t max_file_size_bytes{0};
};

struct LogConfig {
    std::string root_dir{"/var/log/robot"};
    std::string default_level{"info"};
    size_t default_max_file_size_bytes{20U * 1024U * 1024U};
    size_t max_files{100U};
    size_t async_queue_size{16384U};
    size_t async_worker_threads{1U};
    size_t flush_interval_seconds{1U};
    bool enable_layer_aggregate{false};
    bool enable_json_output{false};
    SinkConfig sink;
    std::unordered_map<std::string, ModuleConfig> modules;
    std::unordered_map<std::string, LayerConfig> layers;

    static LogConfig LoadFromFile(const std::string& path);
    void Validate() const;
    FileRotationPolicy ResolveModuleFilePolicy(std::string_view module_name,
                                               std::string_view layer_name) const;
    FileRotationPolicy ResolveLayerFilePolicy(std::string_view layer_name) const;
};

}  // namespace naviai::log_module
