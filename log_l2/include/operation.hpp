#pragma once

#include <string_view>

#include <string>

#include "log_l2_types.hpp"

namespace naviai::log::l2_log {

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

}  // namespace naviai::log::l2_log
