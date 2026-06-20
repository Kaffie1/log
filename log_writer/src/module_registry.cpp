#include "module_registry.hpp"

namespace naviai::log {

void ModuleRegistry::Clear() {
    modules_.clear();
}

const ModuleConfig& ModuleRegistry::GetOrCreate(const std::string& module_name,
                                                LogLevel default_level) {
    auto it = modules_.find(module_name);
    if (it != modules_.end()) {
        return it->second;
    }

    ModuleConfig config;
    config.module_name = module_name;
    config.output_group = module_name;
    config.level = default_level;
    auto [inserted_it, inserted] = modules_.emplace(module_name, std::move(config));
    (void)inserted;
    return inserted_it->second;
}

const ModuleConfig& ModuleRegistry::SetModuleLevel(const std::string& module_name,
                                                   LogLevel level,
                                                   LogLevel default_level) {
    auto& config = const_cast<ModuleConfig&>(GetOrCreate(module_name, default_level));
    config.level = level;
    config.custom_level = true;
    return config;
}

void ModuleRegistry::SetDefaultLevel(LogLevel level) {
    for (auto& item : modules_) {
        if (!item.second.custom_level) {
            item.second.level = level;
        }
    }
}

std::vector<ModuleConfig> ModuleRegistry::List() const {
    std::vector<ModuleConfig> result;
    result.reserve(modules_.size());
    for (const auto& item : modules_) {
        result.push_back(item.second);
    }
    return result;
}

}  // namespace naviai::log
