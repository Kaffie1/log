#include "../include/log.hpp"

#include "business_data.hpp"
#include "operation.hpp"
#include "recorder.hpp"
#include "ros_common.hpp"
#include "sensor_data.hpp"

namespace naviai::log_module {

namespace {

int64_t ResolveRosRecordTimeUs(const L2RosRecordMetadata& metadata) {
    return metadata.message_time_us > 0 ? metadata.message_time_us
                                        : metadata.message_time_ns / 1000;
}

}  // namespace

void L2::Init(LogLevel level, const std::string& root_dir) {
    l2_log::InitLogLayer(level, root_dir);
}

void L2::RegisterModule(const std::string& module_name,
                        const ModuleOptions& options) {
    l2_log::RegisterModule(module_name, options);
}

void L2::Write(const std::string& module_name,
               LogLevel level,
               const std::string& message,
               const PublicLogContext& context,
               const PublicLogExtra& extra) {
    l2_log::WriteMessage(module_name, level, message, context, extra);
}

void L2::SetLevel(const std::string& module_name, LogLevel level) {
    l2_log::SetLevel(module_name, level);
}

void L2::SetLevelForAll(LogLevel level) {
    l2_log::SetLevelForAll(level);
}

void L2::InitRecorder(const L2RecorderOptions& options) {
    l2_log::InitRecorder(options);
}

void L2::SetSampleMode(L2SampleMode mode) {
    l2_log::SetSampleMode(mode);
}

void L2::RegisterTopic(const L2TopicDescriptor& descriptor) {
    if (descriptor.source_domain == L2SourceDomain::Sensor) {
        l2_log::RegisterSensorTopic(descriptor);
        return;
    }
    l2_log::RegisterBusinessTopic(descriptor);
}

void L2::RegisterTopics(const std::vector<L2TopicDescriptor>& descriptors) {
    for (const auto& descriptor : descriptors) {
        RegisterTopic(descriptor);
    }
}

void L2::RegisterTopics(const L2TopicInput& input) {
    RegisterTopics(input.Topics());
}

void L2::RegisterRosTopic(const L2RosTopicDescriptor& descriptor) {
    RegisterTopic(ToL2TopicDescriptor(descriptor));
}

void L2::RegisterRosTopics(
    const std::vector<L2RosTopicDescriptor>& descriptors) {
    RegisterTopics(ToL2TopicDescriptors(descriptors));
}

void L2::RegisterRosTopics(const L2RosTopicInput& input) {
    RegisterRosTopics(input.Topics());
}

void L2::StartTask(const std::string& task_id) {
    l2_log::StartTask(task_id);
}

void L2::UpdateTaskState(const std::string& action_state) {
    l2_log::UpdateTaskState(action_state);
}

void L2::FinishTask(const std::string& action_state) {
    l2_log::FinishTask(action_state);
}

void L2::RecordTopic(const L2TopicMessage& message) {
    const auto& topics = l2_log::State().topics;
    const auto it = topics.find(message.topic);
    if (it == topics.end()) {
        throw std::runtime_error("topic is not registered: " + message.topic);
    }
    if (it->second.descriptor.source_domain == L2SourceDomain::Sensor) {
        l2_log::RecordSensorTopic(message);
        return;
    }
    l2_log::RecordBusinessTopic(message);
}

void L2::RecordRosTopic(const L2RosTopicMessage& message) {
    RecordTopic(ToL2TopicMessage(message));
}

void L2::RecordRosSerialized(const std::string& topic,
                             const std::string& serialized_payload,
                             const L2RosRecordMetadata& metadata) {
    RecordRosTopic(BuildRosTopicMessage(topic, serialized_payload, metadata));
}

void L2::RecordNavigationGoal(const std::string& serialized_payload,
                              const std::string& task_id,
                              const L2NavigationGoalSummaryInput& summary_input,
                              const L2RosRecordMetadata& base) {
    l2_log::StartTask(task_id);
    RecordRosSerialized(l2_ros::kNavigationGoalTopic, serialized_payload,
                        BuildNavigationGoalMetadata(task_id, summary_input, base));
}

void L2::RecordNavigationFeedback(
    const std::string& serialized_payload,
    const std::string& task_id,
    const L2NavigationFeedbackSummaryInput& summary_input,
    const L2RosRecordMetadata& base) {
    l2_log::UpdateTaskState(summary_input.state_name, ResolveRosRecordTimeUs(base));
    RecordRosSerialized(l2_ros::kNavigationFeedbackTopic, serialized_payload,
                        BuildNavigationFeedbackMetadata(task_id, summary_input, base));
}

void L2::RecordNavigationResult(
    const std::string& serialized_payload,
    const std::string& task_id,
    const L2NavigationResultSummaryInput& summary_input,
    const L2RosRecordMetadata& base) {
    RecordRosSerialized(l2_ros::kNavigationResultTopic, serialized_payload,
                        BuildNavigationResultMetadata(task_id, summary_input, base));
    l2_log::FinishTask(summary_input.state_name, ResolveRosRecordTimeUs(base));
}

std::string L2::PackageRecords(const std::string& bundle_dir) {
    l2_log::PackageRecords(bundle_dir);
    const auto& state = l2_log::State();
    return bundle_dir.empty() ? (state.options.root_dir + "/" + state.session_id + ".bundle")
                              : bundle_dir;
}

std::string L2::PackageRecords(const L2PackageOptions& options) {
    return l2_log::PackageRecords(options);
}

void L2::Flush() {
    l2_log::FlushAll();
}

void L2::Shutdown() {
    l2_log::ShutdownAll();
}

}  // namespace naviai::log_module
