#pragma once

#include <fmt/format.h>
#include <string>
#include <utility>

#include "navigation_action.hpp"
#include "ros_topic.hpp"
#include "types.hpp"

namespace naviai::log_module {

class L2 {
  public:
    static void Init(LogLevel level = LogLevel::Info,
                     const std::string& root_dir = "/var/log/robot");
    static void RegisterModule(const std::string& module_name,
                               const ModuleOptions& options = {});
    static void Write(const std::string& module_name,
                      LogLevel level,
                      const std::string& message,
                      const PublicLogContext& context = {},
                      const PublicLogExtra& extra = {});
    static void SetLevel(const std::string& module_name, LogLevel level);
    static void SetLevelForAll(LogLevel level);

    static void InitRecorder(const L2RecorderOptions& options = {});
    static void SetSampleMode(L2SampleMode mode);
    static void RegisterTopic(const L2TopicDescriptor& descriptor);
    static void RegisterTopics(const std::vector<L2TopicDescriptor>& descriptors);
    static void RegisterTopics(const L2TopicInput& input);
    static void RegisterRosTopic(const L2RosTopicDescriptor& descriptor);
    static void RegisterRosTopics(const std::vector<L2RosTopicDescriptor>& descriptors);
    static void RegisterRosTopics(const L2RosTopicInput& input);
    static void StartTask(const std::string& task_id);
    static void UpdateTaskState(const std::string& action_state);
    static void FinishTask(const std::string& action_state = "Succeeded");
    static void RecordTopic(const L2TopicMessage& message);
    static void RecordRosTopic(const L2RosTopicMessage& message);
    static void RecordRosSerialized(
        const std::string& topic,
        const std::string& serialized_payload,
        const L2RosRecordMetadata& metadata = {});
    static void RecordNavigationGoal(
        const std::string& serialized_payload,
        const std::string& task_id,
        const L2NavigationGoalSummaryInput& summary_input,
        const L2RosRecordMetadata& base = {});
    static void RecordNavigationFeedback(
        const std::string& serialized_payload,
        const std::string& task_id,
        const L2NavigationFeedbackSummaryInput& summary_input,
        const L2RosRecordMetadata& base = {});
    static void RecordNavigationResult(
        const std::string& serialized_payload,
        const std::string& task_id,
        const L2NavigationResultSummaryInput& summary_input,
        const L2RosRecordMetadata& base = {});
    static std::string PackageRecords(const std::string& bundle_dir = {});
    static std::string PackageRecords(const L2PackageOptions& options);

    static void Flush();
    static void Shutdown();
};

template <typename CallbackArgT, typename SerializeFn, typename MetadataFn>
auto MakeRosTopicCallback(const std::string& topic,
                          SerializeFn&& serializer,
                          MetadataFn&& metadata_extractor) {
    return [topic,
            serializer = std::forward<SerializeFn>(serializer),
            metadata_extractor = std::forward<MetadataFn>(metadata_extractor)](
               const CallbackArgT& message) mutable {
        L2::RecordRosSerialized(topic, serializer(message),
                                metadata_extractor(message));
    };
}

template <typename CallbackArgT, typename SerializeFn>
auto MakeRosTopicCallback(const std::string& topic, SerializeFn&& serializer) {
    return MakeRosTopicCallback<CallbackArgT>(
        topic, std::forward<SerializeFn>(serializer),
        [](const CallbackArgT&) { return L2RosRecordMetadata{}; });
}

}  // namespace naviai::log_module

#define LOG_L2_TRACE(module, ...) \
    ::naviai::log_module::L2::Write(module, ::naviai::log_module::LogLevel::Trace, fmt::format(__VA_ARGS__))
#define LOG_L2_DEBUG(module, ...) \
    ::naviai::log_module::L2::Write(module, ::naviai::log_module::LogLevel::Debug, fmt::format(__VA_ARGS__))
#define LOG_L2_INFO(module, ...) \
    ::naviai::log_module::L2::Write(module, ::naviai::log_module::LogLevel::Info, fmt::format(__VA_ARGS__))
#define LOG_L2_WARN(module, ...) \
    ::naviai::log_module::L2::Write(module, ::naviai::log_module::LogLevel::Warn, fmt::format(__VA_ARGS__))
#define LOG_L2_ERROR(module, ...) \
    ::naviai::log_module::L2::Write(module, ::naviai::log_module::LogLevel::Error, fmt::format(__VA_ARGS__))
#define LOG_L2_CRITICAL(module, ...) \
    ::naviai::log_module::L2::Write(module, ::naviai::log_module::LogLevel::Critical, fmt::format(__VA_ARGS__))
