#pragma once

#include <string_view>

#include <string>

#include "types.hpp"
#include "log_types.hpp"

namespace naviai::log_module::l2_log {

void InitLogLayer(LogLevel level, const std::string& root_dir);
void RegisterModule(const std::string& module_name, const ModuleOptions& options);
void WriteMessage(const std::string& module_name,
                  LogLevel level,
                  const std::string& message,
                  const PublicLogContext& context,
                  const PublicLogExtra& extra);
void SetLevel(const std::string& module_name, LogLevel level);
void SetLevelForAll(LogLevel level);
const char* TopicRecordLogModuleName();
const char* TopicRecordLogModuleNameForTopic(std::string_view topic);

void InitRecorder(const L2RecorderOptions& options);
void SetSampleMode(L2SampleMode mode);
void ForceRotateActiveSegments();
void SealActiveSegments();
void PackageRecords(const std::string& bundle_dir);
std::string PackageRecords(const L2PackageOptions& options);
void FlushAll();
void ShutdownAll();

}  // namespace naviai::log_module::l2_log
