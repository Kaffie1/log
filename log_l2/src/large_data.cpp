#include "large_data.hpp"

#include <stdexcept>

#include "recorder.hpp"

namespace naviai::log::l2_log {

void RegisterLargeDataTopic(const L2TopicDescriptor& descriptor) {
    RegisterTopicRuntime(descriptor);
}

void RecordLargeDataTopic(const L2TopicMessage& message) {
    RecordTopicWithBusinessSemantics(message);
}

}  // namespace naviai::log::l2_log
