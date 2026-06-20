#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "log_types.hpp"

namespace naviai::log_module {

enum class L2SourceDomain {
    Sensor,
    Algorithm,
};

enum class L2SampleMode {
    Full,
    Normal,
    LowFrequency,
};

struct L2TopicDescriptor {
    std::string topic;
    std::string topic_type;
    std::string source_module;
    L2SourceDomain source_domain{L2SourceDomain::Sensor};
    std::string source_type;
    std::uint64_t segment_size_bytes{0};
    std::uint64_t target_compressed_segment_size_bytes{0};
};

class L2TopicInput {
  public:
    void AddTopic(const L2TopicDescriptor& descriptor);
    void AddSensorTopic(const std::string& topic,
                        const std::string& topic_type,
                        const std::string& source_module,
                        const std::string& source_type);
    void AddBusinessTopic(const std::string& topic,
                          const std::string& topic_type,
                          const std::string& source_module,
                          const std::string& source_type);
    const std::vector<L2TopicDescriptor>& Topics() const;
    bool Empty() const;
    void Clear();

  private:
    std::vector<L2TopicDescriptor> topics_;
};

struct L2RecorderOptions {
    LogLevel level{LogLevel::Info};
    std::string root_dir{"/var/log/robot/l2"};
    std::string session_id;
    std::string host;
    std::string container;
    L2SampleMode sample_mode{L2SampleMode::Normal};
    std::vector<L2TopicDescriptor> topics;
};

struct L2TopicMessage {
    std::string topic;
    int64_t message_time_us{0};
    std::string task_id;
    std::string frame_id;
    int64_t sequence{0};
    std::string payload_encoding{"json"};
    std::string payload;
    std::string replay_payload;
    std::map<std::string, std::string> payload_summary;
    std::string action_phase{"topic"};
    std::string action_state;
};

struct L2PackageOptions {
    std::string root_dir{"/var/log/robot/l2"};
    int64_t start_time_us{0};
    int64_t end_time_us{0};
    std::string output_path;
};

}  // namespace naviai::log_module
