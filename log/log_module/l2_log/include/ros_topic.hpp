#pragma once

#include <string_view>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "types.hpp"

namespace naviai::log_module {

struct L2RosTopicDescriptor {
    std::string topic;
    std::string message_type;
    std::string source_module;
    L2SourceDomain source_domain{L2SourceDomain::Sensor};
    std::string source_type;
    std::uint64_t segment_size_bytes{0};
    std::uint64_t target_compressed_segment_size_bytes{0};
};

class L2RosTopicInput {
  public:
    void AddTopic(const L2RosTopicDescriptor& descriptor);
    void AddSensorTopic(const std::string& topic,
                        const std::string& message_type,
                        const std::string& source_module,
                        const std::string& source_type);
    void AddBusinessTopic(const std::string& topic,
                          const std::string& message_type,
                          const std::string& source_module,
                          const std::string& source_type);
    const std::vector<L2RosTopicDescriptor>& Topics() const;
    bool Empty() const;
    void Clear();

  private:
    std::vector<L2RosTopicDescriptor> topics_;
};

struct L2RosTopicMessage {
    std::string topic;
    int64_t message_time_us{0};
    int64_t message_time_ns{0};
    std::string task_id;
    std::string frame_id;
    int64_t sequence{0};
    std::string serialized_payload;
    std::map<std::string, std::string> payload_summary;
    std::string action_phase{"topic"};
    std::string action_state;
};

struct L2RosRecordMetadata {
    int64_t message_time_us{0};
    int64_t message_time_ns{0};
    std::string task_id;
    std::string frame_id;
    int64_t sequence{0};
    std::map<std::string, std::string> payload_summary;
    std::string action_phase{"topic"};
    std::string action_state;
};

L2TopicDescriptor ToL2TopicDescriptor(const L2RosTopicDescriptor& descriptor);
L2TopicMessage ToL2TopicMessage(const L2RosTopicMessage& message);
L2RosTopicMessage BuildRosTopicMessage(
    const std::string& topic,
    const std::string& serialized_payload,
    const L2RosRecordMetadata& metadata = {});
std::vector<L2RosTopicDescriptor> DefaultReplayRosTopics();
L2RosTopicInput DefaultReplayRosTopicInput();
bool IsDefaultReplayTopic(std::string_view topic);
std::vector<L2TopicDescriptor> ToL2TopicDescriptors(
    const std::vector<L2RosTopicDescriptor>& descriptors);

}  // namespace naviai::log_module
