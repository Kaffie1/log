#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace naviai::log {

enum class L2SourceDomain {
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
    L2SourceDomain source_domain{L2SourceDomain::Algorithm};
    std::string source_type;
    std::uint64_t segment_size_bytes{0};
    std::uint64_t target_compressed_segment_size_bytes{0};
};

struct L2RecorderOptions {
    std::string root_dir{"/var/log/robot/l2"};
    std::string session_id;
    std::string host;
    std::string container;
    L2SampleMode sample_mode{L2SampleMode::Normal};
    std::vector<L2TopicDescriptor> topics;
};

struct L2TopicMessage {
    std::string topic;
    std::int64_t message_time_us{0};
    std::string task_id;
    std::string frame_id;
    std::int64_t sequence{0};
    std::string payload_encoding{"json"};
    std::string payload;
    std::string replay_payload;
    std::map<std::string, std::string> payload_summary;
    std::string action_phase{"topic"};
    std::string action_state;
};

struct L2PackageOptions {
    std::string root_dir{"/var/log/robot/l2"};
    std::int64_t start_time_us{0};
    std::int64_t end_time_us{0};
    std::string output_path;
};

}  // namespace naviai::log
