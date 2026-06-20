#include "business_data.hpp"

#include "recorder.hpp"

namespace naviai::log::l2_log {

void RegisterBusinessTopic(const L2TopicDescriptor& descriptor) {
    RegisterTopicRuntime(descriptor);
}

void StartTask(const std::string& task_id) {
    auto& state = State();
    state.task_id = task_id;
    state.task_active = true;
    state.task_idle_deadline_us = 0;
}

void UpdateTaskState(const std::string& action_state, int64_t message_time_us) {
    auto& state = State();
    const int64_t effective_time_us = ResolveMessageTimeUs(message_time_us);
    if (action_state == "Active" || action_state == "Running" ||
        action_state == "Arrived" || action_state == "Canceling") {
        state.task_active = true;
        state.task_idle_deadline_us = 0;
        return;
    }
    if (action_state == "Succeeded") {
        state.task_active = false;
        state.task_id.clear();
        state.task_idle_deadline_us = 0;
        return;
    }
    if (action_state == "Cancelled" || action_state == "Failed" ||
        action_state == "Error" || action_state == "Aborted") {
        state.task_active = true;
        state.task_idle_deadline_us = effective_time_us + 10 * 1000000;
    }
}

void FinishTask(const std::string& action_state, int64_t message_time_us) {
    UpdateTaskState(action_state, message_time_us);
}

void RecordBusinessTopic(const L2TopicMessage& message) {
    RecordTopicWithBusinessSemantics(message);
}

}  // namespace naviai::log::l2_log
