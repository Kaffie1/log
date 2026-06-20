#include "log_config.hpp"

#include <fstream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace naviai::log_module {

namespace {

std::string Trim(const std::string& value) {
    const auto begin = value.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return {};
    }
    const auto end = value.find_last_not_of(" \t\r\n");
    return value.substr(begin, end - begin + 1);
}

bool ParseBool(const std::string& value) {
    return value == "true" || value == "True" || value == "1";
}

size_t ParseSize(const std::string& value) {
    return static_cast<size_t>(std::stoull(value));
}

std::vector<std::string> SplitPath(const std::string& path) {
    std::vector<std::string> parts;
    std::stringstream stream(path);
    std::string item;
    while (std::getline(stream, item, '.')) {
        if (!item.empty()) {
            parts.push_back(item);
        }
    }
    return parts;
}

void ApplyValue(LogConfig& config, const std::vector<std::string>& path,
                const std::string& value) {
    if (path.empty()) {
        return;
    }

    if (path.size() == 1) {
        const std::string& key = path[0];
        if (key == "root_dir") {
            config.root_dir = value;
        } else if (key == "default_level") {
            config.default_level = value;
        } else if (key == "default_max_file_size_bytes") {
            config.default_max_file_size_bytes = ParseSize(value);
        } else if (key == "max_files") {
            config.max_files = ParseSize(value);
        } else if (key == "async_queue_size") {
            config.async_queue_size = ParseSize(value);
        } else if (key == "async_worker_threads") {
            config.async_worker_threads = ParseSize(value);
        } else if (key == "flush_interval_seconds") {
            config.flush_interval_seconds = ParseSize(value);
        } else if (key == "enable_layer_aggregate") {
            config.enable_layer_aggregate = ParseBool(value);
        } else if (key == "enable_json_output") {
            config.enable_json_output = ParseBool(value);
            config.sink.json_output = config.enable_json_output;
        }
        return;
    }

    if (path[0] == "sink" && path.size() == 2) {
        const std::string& key = path[1];
        if (key == "enable_console") {
            config.sink.enable_console = ParseBool(value);
        } else if (key == "enable_module_file") {
            config.sink.enable_module_file = ParseBool(value);
        } else if (key == "enable_layer_file") {
            config.sink.enable_layer_file = ParseBool(value);
        } else if (key == "json_output") {
            config.sink.json_output = ParseBool(value);
        } else if (key == "module_file_max_file_size_bytes") {
            config.sink.module_file_max_file_size_bytes = ParseSize(value);
        } else if (key == "layer_file_max_file_size_bytes") {
            config.sink.layer_file_max_file_size_bytes = ParseSize(value);
        } else if (key == "module_file_max_files") {
            config.sink.module_file_max_files = ParseSize(value);
        } else if (key == "layer_file_max_files") {
            config.sink.layer_file_max_files = ParseSize(value);
        }
        return;
    }

    if (path[0] == "modules" && path.size() == 3 &&
        path[2] == "max_file_size_bytes") {
        config.modules[path[1]].max_file_size_bytes = ParseSize(value);
        return;
    }

    if (path[0] == "layers" && path.size() == 3 &&
        path[2] == "max_file_size_bytes") {
        config.layers[path[1]].max_file_size_bytes = ParseSize(value);
    }
}

}  // namespace

LogConfig LogConfig::LoadFromFile(const std::string& path) {
    LogConfig config;
    std::ifstream input(path);
    if (!input.is_open()) {
        return config;
    }

    std::string line;
    std::vector<std::string> sections;
    while (std::getline(input, line)) {
        const auto comment_pos = line.find('#');
        if (comment_pos != std::string::npos) {
            line = line.substr(0, comment_pos);
        }
        if (Trim(line).empty()) {
            continue;
        }

        const auto pos = line.find(':');
        if (pos == std::string::npos) {
            continue;
        }

        size_t indent = 0;
        while (indent < line.size() && line[indent] == ' ') {
            ++indent;
        }
        const size_t depth = indent / 2;
        const std::string key = Trim(line.substr(indent, pos - indent));
        const std::string value = Trim(line.substr(pos + 1));

        if (sections.size() > depth) {
            sections.resize(depth);
        }

        if (value.empty()) {
            if (sections.size() == depth) {
                sections.push_back(key);
            } else {
                sections[depth] = key;
            }
            continue;
        }

        std::string path_key;
        for (const auto& section : sections) {
            if (!path_key.empty()) {
                path_key += '.';
            }
            path_key += section;
        }
        if (!path_key.empty()) {
            path_key += '.';
        }
        path_key += key;

        ApplyValue(config, SplitPath(path_key), value);
    }

    config.Validate();
    return config;
}

void LogConfig::Validate() const {
    if (root_dir.empty()) {
        throw std::invalid_argument("log root_dir must not be empty");
    }
    if (default_max_file_size_bytes == 0) {
        throw std::invalid_argument("default_max_file_size_bytes must be > 0");
    }
    if (max_files == 0) {
        throw std::invalid_argument("max_files must be > 0");
    }
    if (async_queue_size == 0) {
        throw std::invalid_argument("async_queue_size must be > 0");
    }
    if (async_worker_threads == 0) {
        throw std::invalid_argument("async_worker_threads must be > 0");
    }
}

FileRotationPolicy LogConfig::ResolveModuleFilePolicy(
    std::string_view module_name, std::string_view layer_name) const {
    FileRotationPolicy policy{
        default_max_file_size_bytes,
        max_files,
    };

    if (sink.module_file_max_file_size_bytes > 0) {
        policy.max_file_size_bytes = sink.module_file_max_file_size_bytes;
    } else {
        const auto module_it = modules.find(std::string(module_name));
        if (module_it != modules.end() && module_it->second.max_file_size_bytes > 0) {
            policy.max_file_size_bytes = module_it->second.max_file_size_bytes;
        } else {
            const auto layer_it = layers.find(std::string(layer_name));
            if (layer_it != layers.end() && layer_it->second.max_file_size_bytes > 0) {
                policy.max_file_size_bytes = layer_it->second.max_file_size_bytes;
            }
        }
    }

    if (sink.module_file_max_files > 0) {
        policy.max_files = sink.module_file_max_files;
    }
    return policy;
}

FileRotationPolicy LogConfig::ResolveLayerFilePolicy(
    std::string_view layer_name) const {
    FileRotationPolicy policy{
        default_max_file_size_bytes,
        max_files,
    };

    if (sink.layer_file_max_file_size_bytes > 0) {
        policy.max_file_size_bytes = sink.layer_file_max_file_size_bytes;
    } else {
        const auto layer_it = layers.find(std::string(layer_name));
        if (layer_it != layers.end() && layer_it->second.max_file_size_bytes > 0) {
            policy.max_file_size_bytes = layer_it->second.max_file_size_bytes;
        }
    }

    if (sink.layer_file_max_files > 0) {
        policy.max_files = sink.layer_file_max_files;
    }
    return policy;
}

}  // namespace naviai::log_module
