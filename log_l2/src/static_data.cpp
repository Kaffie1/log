#include "static_data.hpp"

#include <stdexcept>

#include "recorder.hpp"

namespace naviai::log::l2_log {

void RegisterStaticDataTopic(const L2TopicDescriptor& descriptor) {
    RegisterTopicRuntime(descriptor);
}

void RecordStaticDataTopic(const L2TopicMessage& message) {
    RecordTopicWithBusinessSemantics(message);
}

}  // namespace naviai::log::l2_log
