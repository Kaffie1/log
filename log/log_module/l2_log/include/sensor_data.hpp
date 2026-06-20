#pragma once

#include "types.hpp"

namespace naviai::log_module::l2_log {

void RegisterSensorTopic(const L2TopicDescriptor& descriptor);
void RecordSensorTopic(const L2TopicMessage& message);

}  // namespace naviai::log_module::l2_log
