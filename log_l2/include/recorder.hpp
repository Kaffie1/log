#pragma once

#include <cstdint>
#include <deque>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include "log_l2_types.hpp"

namespace naviai::log::l2_log {

struct ReplayBinaryRecordHeader {
    uint32_t metadata_size;
    uint32_t payload_size;
    int64_t record_time_us;
    int64_t message_time_us;
    int64_t sequence;
    uint64_t record_id;
};

struct BufferedReplayRecord {
    ReplayBinaryRecordHeader header;
    std::string metadata_json;
    std::string payload;
    std::string index_line_without_offset;
};

struct StorageRuntime {
    std::string bucket_name;
    uint64_t segment_index{0};
    int64_t segment_start_message_time_us{0};
    int64_t segment_end_message_time_us{0};
    int64_t last_rename_time_bucket_us{-1};
    std::filesystem::path data_path;
    std::filesystem::path index_path;
    std::ofstream data_stream;
    std::ofstream index_stream;
    size_t current_data_size_bytes{0};
    size_t current_index_size_bytes{0};
    double estimated_compression_ratio{0.0};
    uint64_t next_record_id{0};
    std::deque<BufferedReplayRecord> recent_records;
    std::uint64_t segment_size_bytes{0};
    std::uint64_t target_compressed_segment_size_bytes{0};
};

struct TopicRuntime {
    L2TopicDescriptor descriptor;
    std::string module_name;
    std::string storage_bucket_name;
    std::string storage_group_path;
    int64_t pending_second{-1};
    L2TopicMessage pending_message;
    bool has_pending{false};
};

struct RecorderState {
    L2RecorderOptions options;
    std::string session_id;
    std::filesystem::path session_meta_path;
    int64_t session_start_message_time_us{0};
    int64_t session_end_message_time_us{0};
    int64_t session_meta_rename_time_bucket_us{-1};
    std::string task_id;
    bool task_active{false};
    int64_t task_idle_deadline_us{0};
    bool initialized{false};
    std::vector<std::filesystem::path> completed_files;
    std::unordered_map<std::string, TopicRuntime> topics;
    std::unordered_map<std::string, StorageRuntime> storage_buckets;
};

RecorderState& State();

int64_t NowTimestampUs();
int64_t ResolveMessageTimeUs(int64_t message_time_us);
const char* SampleModeName(L2SampleMode mode);
void RefreshTaskStateByTime(int64_t message_time_us);
std::string EscapeJson(const std::string& value);
std::string Sanitize(std::string_view input);
std::string DomainName(L2SourceDomain domain);
std::string FormatSummaryJson(const std::map<std::string, std::string>& summary);
std::string BuildModuleName(const L2TopicDescriptor& descriptor);
std::string BuildOutputGroup(const L2TopicDescriptor& descriptor);
std::string BuildStorageBucketName(const L2TopicDescriptor& descriptor);
std::string StorageBucketCategory(const std::string& bucket_name);
void RegisterTopicRuntime(const L2TopicDescriptor& descriptor);
void RecordTopicWithBusinessSemantics(const L2TopicMessage& message);
std::filesystem::path BuildReplaySessionRoot();
std::string FormatSegmentTime(int64_t message_time_us);
inline int64_t SampleWindowUs(L2SampleMode mode) {
    return mode == L2SampleMode::LowFrequency ? 5LL * 1000000LL
                                              : 1LL * 1000000LL;
}
std::filesystem::path BuildSessionMetaPath(int64_t start_message_time_us,
                                           int64_t end_message_time_us);
void RefreshSessionMetaFile(int64_t message_time_us, bool force_rename);
void CleanupExpiredReplayFiles();
std::string BuildIndexLineWithoutOffset(const TopicRuntime& runtime,
                                        const ReplayBinaryRecordHeader& header,
                                        const std::string& sample_mode,
                                        const L2TopicMessage& message,
                                        const std::string& payload_summary_json);
StorageRuntime* FindStorageRuntime(const TopicRuntime& runtime);
void EnsureTopicStorage(StorageRuntime* runtime, int64_t initial_message_time_us);
void RotateTopicStorageIfNeeded(StorageRuntime* runtime,
                                int64_t message_time_us,
                                size_t next_data_write_size,
                                size_t next_index_write_size);
void SwitchTopicStorage(StorageRuntime* runtime, int64_t next_message_time_us);
void UpdateTopicSegmentTimeRange(StorageRuntime* runtime, int64_t message_time_us);
void AppendRecentRecord(StorageRuntime* runtime, const BufferedReplayRecord& record);
void FinalizeTopicStorage(StorageRuntime* runtime, bool compress_closed_segment);
std::string BuildReplayMetadataJson(const std::string& session_id,
                                    const L2TopicDescriptor& descriptor,
                                    const L2TopicMessage& message,
                                    const std::string& sample_mode,
                                    const std::string& sample_reason);
void WriteReplayRecord(const TopicRuntime& runtime,
                       const L2TopicMessage& message,
                       const std::string& sample_mode,
                       const std::string& sample_reason);
void FlushPendingTopic(TopicRuntime* runtime, const std::string& reason);

}  // namespace naviai::log::l2_log
