#include "ros_topic.hpp"

#include <stdexcept>
#include <unordered_set>

namespace naviai::log {

namespace {

constexpr std::uint64_t kDefaultReplaySegmentSizeBytes =
    500ULL * 1024ULL * 1024ULL;
constexpr std::uint64_t kLargeLocalMapSegmentSizeBytes =
    2ULL * 1024ULL * 1024ULL * 1024ULL;
constexpr std::uint64_t kTargetCompressedSegmentSizeBytes =
    10ULL * 1024ULL * 1024ULL;

std::string Base64Encode(const std::string& data) {
    static constexpr char kAlphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string encoded;
    encoded.reserve(((data.size() + 2) / 3) * 4);

    std::size_t index = 0;
    while (index + 3 <= data.size()) {
        const auto b0 = static_cast<unsigned char>(data[index++]);
        const auto b1 = static_cast<unsigned char>(data[index++]);
        const auto b2 = static_cast<unsigned char>(data[index++]);

        encoded.push_back(kAlphabet[(b0 >> 2) & 0x3f]);
        encoded.push_back(kAlphabet[((b0 & 0x03) << 4) | ((b1 >> 4) & 0x0f)]);
        encoded.push_back(kAlphabet[((b1 & 0x0f) << 2) | ((b2 >> 6) & 0x03)]);
        encoded.push_back(kAlphabet[b2 & 0x3f]);
    }

    const auto remain = data.size() - index;
    if (remain == 1) {
        const auto b0 = static_cast<unsigned char>(data[index]);
        encoded.push_back(kAlphabet[(b0 >> 2) & 0x3f]);
        encoded.push_back(kAlphabet[(b0 & 0x03) << 4]);
        encoded.push_back('=');
        encoded.push_back('=');
    } else if (remain == 2) {
        const auto b0 = static_cast<unsigned char>(data[index++]);
        const auto b1 = static_cast<unsigned char>(data[index]);
        encoded.push_back(kAlphabet[(b0 >> 2) & 0x3f]);
        encoded.push_back(kAlphabet[((b0 & 0x03) << 4) | ((b1 >> 4) & 0x0f)]);
        encoded.push_back(kAlphabet[(b1 & 0x0f) << 2]);
        encoded.push_back('=');
    }

    return encoded;
}

const std::vector<L2RosTopicDescriptor>& ReplayTopicCatalog() {
    static const std::vector<L2RosTopicDescriptor> topics = {
        {"/zj_humanoid/navigation/odom_info",
         "nav_msgs/Odometry",
         "location",
         L2SourceDomain::Algorithm,
         "localization",
         kDefaultReplaySegmentSizeBytes,
         0},
        {"/zj_humanoid/navigation/map",
         "nav_msgs/OccupancyGrid",
         "map_server",
         L2SourceDomain::Algorithm,
         "map",
         kDefaultReplaySegmentSizeBytes,
         0},
        {"/zj_humanoid/navigation/local_map",
         "navigation/LocalMap",
         "perception",
         L2SourceDomain::Algorithm,
         "local_map",
         kLargeLocalMapSegmentSizeBytes,
         kTargetCompressedSegmentSizeBytes},
        {"/zj_humanoid/perception/location_code",
         "module_common_msgs/ModuleStatus",
         "location",
         L2SourceDomain::Algorithm,
         "location_status",
         kDefaultReplaySegmentSizeBytes,
         0},
        {"/zj_humanoid/perception/perception_code",
         "module_common_msgs/ModuleStatus",
         "perception",
         L2SourceDomain::Algorithm,
         "perception_status",
         kDefaultReplaySegmentSizeBytes,
         0},
        {"/zj_humanoid/navigation/navigation_code",
         "navigation/ModuleStatus",
         "navigation",
         L2SourceDomain::Algorithm,
         "navigation_status",
         kDefaultReplaySegmentSizeBytes,
         0},
        {"/zj_humanoid/chassis/agv_state",
         "chassis_msgs/AGVState",
         "chassis",
         L2SourceDomain::Algorithm,
         "agv_state",
         kDefaultReplaySegmentSizeBytes,
         0},
        {"/zj_humanoid/cmd_vel/calib",
         "geometry_msgs/Twist",
         "chassis",
         L2SourceDomain::Algorithm,
         "control",
         kDefaultReplaySegmentSizeBytes,
         0},
        {"/zj_humanoid/navigation/navigation/goal",
         "navigation/NavigationActionGoal",
         "navigation",
         L2SourceDomain::Algorithm,
         "navigation_action",
         kDefaultReplaySegmentSizeBytes,
         0},
        {"/zj_humanoid/navigation/navigation/feedback",
         "navigation/NavigationActionFeedback",
         "navigation",
         L2SourceDomain::Algorithm,
         "navigation_action",
         kDefaultReplaySegmentSizeBytes,
         0},
        {"/zj_humanoid/navigation/navigation/result",
         "navigation/NavigationActionResult",
         "navigation",
         L2SourceDomain::Algorithm,
         "navigation_action",
         kDefaultReplaySegmentSizeBytes,
         0},
    };
    return topics;
}

}  // namespace

void L2RosTopicInput::AddTopic(const L2RosTopicDescriptor& descriptor) {
    topics_.push_back(descriptor);
}

void L2RosTopicInput::AddBusinessTopic(const std::string& topic,
                                       const std::string& message_type,
                                       const std::string& source_module,
                                       const std::string& source_type) {
    L2RosTopicDescriptor descriptor;
    descriptor.topic = topic;
    descriptor.message_type = message_type;
    descriptor.source_module = source_module;
    descriptor.source_domain = L2SourceDomain::Algorithm;
    descriptor.source_type = source_type;
    descriptor.segment_size_bytes = kDefaultReplaySegmentSizeBytes;
    descriptor.target_compressed_segment_size_bytes = 0;
    AddTopic(descriptor);
}

const std::vector<L2RosTopicDescriptor>& L2RosTopicInput::Topics() const {
    return topics_;
}

bool L2RosTopicInput::Empty() const {
    return topics_.empty();
}

void L2RosTopicInput::Clear() {
    topics_.clear();
}

L2TopicDescriptor ToL2TopicDescriptor(const L2RosTopicDescriptor& descriptor) {
    L2TopicDescriptor output;
    output.topic = descriptor.topic;
    output.topic_type = descriptor.message_type;
    output.source_module = descriptor.source_module;
    output.source_domain = descriptor.source_domain;
    output.source_type = descriptor.source_type;
    output.segment_size_bytes = descriptor.segment_size_bytes;
    output.target_compressed_segment_size_bytes =
        descriptor.target_compressed_segment_size_bytes;
    return output;
}

L2TopicMessage ToL2TopicMessage(const L2RosTopicMessage& message) {
    if (message.topic.empty()) {
        throw std::invalid_argument("message.topic must not be empty");
    }

    L2TopicMessage output;
    output.topic = message.topic;
    output.message_time_us =
        message.message_time_us > 0 ? message.message_time_us : message.message_time_ns / 1000;
    output.task_id = message.task_id;
    output.frame_id = message.frame_id;
    output.sequence = message.sequence;
    output.payload_encoding = "ros_serialized";
    output.payload = Base64Encode(message.serialized_payload);
    output.replay_payload = message.serialized_payload;
    output.payload_summary = message.payload_summary;
    output.action_phase = message.action_phase;
    output.action_state = message.action_state;
    output.payload_summary["payload_size"] =
        std::to_string(message.serialized_payload.size());
    return output;
}

L2RosTopicMessage BuildRosTopicMessage(
    const std::string& topic,
    const std::string& serialized_payload,
    const L2RosRecordMetadata& metadata) {
    L2RosTopicMessage message;
    message.topic = topic;
    message.message_time_us = metadata.message_time_us;
    message.message_time_ns = metadata.message_time_ns;
    message.task_id = metadata.task_id;
    message.frame_id = metadata.frame_id;
    message.sequence = metadata.sequence;
    message.serialized_payload = serialized_payload;
    message.payload_summary = metadata.payload_summary;
    message.action_phase = metadata.action_phase;
    message.action_state = metadata.action_state;
    return message;
}

std::vector<L2RosTopicDescriptor> DefaultReplayRosTopics() {
    return ReplayTopicCatalog();
}

L2RosTopicInput DefaultReplayRosTopicInput() {
    L2RosTopicInput input;
    for (const auto& topic : ReplayTopicCatalog()) {
        input.AddTopic(topic);
    }
    return input;
}

bool IsDefaultReplayTopic(std::string_view topic) {
    static const std::unordered_set<std::string> topic_set = []() {
        std::unordered_set<std::string> values;
        for (const auto& descriptor : ReplayTopicCatalog()) {
            values.insert(descriptor.topic);
        }
        return values;
    }();
    return topic_set.count(std::string(topic)) > 0;
}

std::vector<L2TopicDescriptor> ToL2TopicDescriptors(
    const std::vector<L2RosTopicDescriptor>& descriptors) {
    std::vector<L2TopicDescriptor> output;
    output.reserve(descriptors.size());
    for (const auto& descriptor : descriptors) {
        output.push_back(ToL2TopicDescriptor(descriptor));
    }
    return output;
}

}  // namespace naviai::log
