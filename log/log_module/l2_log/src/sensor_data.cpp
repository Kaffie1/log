#include "sensor_data.hpp"

#include <algorithm>
#include <stdexcept>

#include "operation.hpp"
#include "recorder.hpp"

namespace naviai::log_module::l2_log {

void RegisterSensorTopic(const L2TopicDescriptor& descriptor) {
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

void RecordSensorTopic(const L2TopicMessage& message) {
    auto& state = State();
    RefreshTaskStateByTime(message.message_time_us);
    const auto it = state.topics.find(message.topic);
    if (it == state.topics.end()) {
        throw std::runtime_error("topic is not registered: " + message.topic);
    }

    auto& runtime = it->second;
    L2TopicMessage normalized = message;
    normalized.task_id.clear();
    normalized.action_state.clear();

    if (state.options.sample_mode == L2SampleMode::Full ||
        (state.options.sample_mode == L2SampleMode::Normal && state.task_active)) {
        if (state.task_active) {
            normalized.task_id = state.task_id;
            normalized.action_state = "Running";
        }
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
