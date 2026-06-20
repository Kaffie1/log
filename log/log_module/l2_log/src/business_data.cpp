#include "business_data.hpp"

#include <algorithm>
#include <stdexcept>

#include "operation.hpp"
#include "recorder.hpp"

namespace naviai::log_module::l2_log {

void RegisterBusinessTopic(const L2TopicDescriptor& descriptor) {
    if (descriptor.topic.empty()) {
        throw std::invalid_argument("descriptor.topic must not be empty");
    }

    TopicRuntime runtime;
    runtime.descriptor = descriptor;
    runtime.module_name = BuildModuleName(descriptor);
    runtime.storage_bucket_name = BuildStorageBucketName(descriptor);
    runtime.storage_group_path = BuildOutputGroup(descriptor);
    auto& storage = State().storage_buckets[runtime.storage_bucket_name];
    storage.bucket_name = runtime.storage_bucket_name;
    storage.segment_size_bytes =
        std::max(storage.segment_size_bytes, descriptor.segment_size_bytes);
    storage.target_compressed_segment_size_bytes =
        std::max(storage.target_compressed_segment_size_bytes,
                 descriptor.target_compressed_segment_size_bytes);
    State().topics[descriptor.topic] = std::move(runtime);
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
    auto& state = State();
    RefreshTaskStateByTime(message.message_time_us);
    const auto it = state.topics.find(message.topic);
    if (it == state.topics.end()) {
        throw std::runtime_error("topic is not registered: " + message.topic);
    }

    auto& runtime = it->second;
    L2TopicMessage normalized = message;
    if (normalized.task_id.empty()) {
        normalized.task_id = state.task_id;
    }
    if (normalized.action_state.empty() && state.task_active) {
        normalized.action_state = "Running";
    }

    if (state.options.sample_mode == L2SampleMode::Full ||
        (state.options.sample_mode == L2SampleMode::Normal && state.task_active)) {
        FlushPendingTopic(&runtime, "full_raw_flush");
        WriteReplayRecord(runtime, normalized, "task_raw", "full_raw");
        return;
    }

    const int64_t current_time = ResolveMessageTimeUs(normalized.message_time_us);
    const int64_t sample_bucket = current_time / SampleWindowUs(state.options.sample_mode);
    if (!runtime.has_pending) {
        runtime.pending_second = sample_bucket;
        runtime.pending_message = normalized;
        runtime.has_pending = true;
        return;
    }
    if (runtime.pending_second == sample_bucket) {
        runtime.pending_message = normalized;
        return;
    }
    FlushPendingTopic(&runtime, state.options.sample_mode == L2SampleMode::LowFrequency
                                    ? "low_tick"
                                    : "idle_tick");
    runtime.pending_second = sample_bucket;
    runtime.pending_message = normalized;
    runtime.has_pending = true;
}

}  // namespace naviai::log_module::l2_log
