#include "types.hpp"

namespace naviai::log_module {

namespace {

constexpr std::uint64_t kDefaultReplaySegmentSizeBytes =
    500ULL * 1024ULL * 1024ULL;

}

void L2TopicInput::AddTopic(const L2TopicDescriptor& descriptor) {
    topics_.push_back(descriptor);
}

void L2TopicInput::AddSensorTopic(const std::string& topic,
                                  const std::string& topic_type,
                                  const std::string& source_module,
                                  const std::string& source_type) {
    L2TopicDescriptor descriptor;
    descriptor.topic = topic;
    descriptor.topic_type = topic_type;
    descriptor.source_module = source_module;
    descriptor.source_domain = L2SourceDomain::Sensor;
    descriptor.source_type = source_type;
    descriptor.segment_size_bytes = kDefaultReplaySegmentSizeBytes;
    descriptor.target_compressed_segment_size_bytes = 0;
    AddTopic(descriptor);
}

void L2TopicInput::AddBusinessTopic(const std::string& topic,
                                    const std::string& topic_type,
                                    const std::string& source_module,
                                    const std::string& source_type) {
    L2TopicDescriptor descriptor;
    descriptor.topic = topic;
    descriptor.topic_type = topic_type;
    descriptor.source_module = source_module;
    descriptor.source_domain = L2SourceDomain::Algorithm;
    descriptor.source_type = source_type;
    descriptor.segment_size_bytes = kDefaultReplaySegmentSizeBytes;
    descriptor.target_compressed_segment_size_bytes = 0;
    AddTopic(descriptor);
}

const std::vector<L2TopicDescriptor>& L2TopicInput::Topics() const {
    return topics_;
}

bool L2TopicInput::Empty() const {
    return topics_.empty();
}

void L2TopicInput::Clear() {
    topics_.clear();
}

}  // namespace naviai::log_module
