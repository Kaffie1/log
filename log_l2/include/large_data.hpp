#pragma once

#include "log_l2_types.hpp"

namespace naviai::log::l2_log {

void RegisterLargeDataTopic(const L2TopicDescriptor& descriptor);
void RecordLargeDataTopic(const L2TopicMessage& message);

}  // namespace naviai::log::l2_log
