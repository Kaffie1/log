#pragma once

#include "log_types.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace naviai::log {

struct ModuleConfig {
    std::string module_name;
    std::string output_group;
    LogLevel level{LogLevel::Info};
    bool custom_level{false};
};

class ModuleRegistry {
  public:
    void Clear();
    const ModuleConfig& GetOrCreate(const std::string& module_name,
                                    LogLevel default_level);
    const ModuleConfig& SetModuleLevel(const std::string& module_name,
                                       LogLevel level,
                                       LogLevel default_level);
    void SetDefaultLevel(LogLevel level);
    std::vector<ModuleConfig> List() const;

  private:
    std::unordered_map<std::string, ModuleConfig> modules_;
};

}  // namespace naviai::log
