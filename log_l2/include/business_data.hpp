#pragma once

#include <string>

#include "log_l2_types.hpp"

namespace naviai::log::l2_log {

void RegisterBusinessTopic(const L2TopicDescriptor& descriptor);
void StartTask(const std::string& task_id);
void UpdateTaskState(const std::string& action_state, int64_t message_time_us = 0);
void FinishTask(const std::string& action_state, int64_t message_time_us = 0);
void RecordBusinessTopic(const L2TopicMessage& message);

}  // namespace naviai::log::l2_log
