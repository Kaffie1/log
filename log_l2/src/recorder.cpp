#include "recorder.hpp"

#include <chrono>
#include <cctype>
#include <filesystem>
#include <iomanip>
#include <set>
#include <sstream>

#include <zlib.h>

#include "log_service.hpp"
#include "operation.hpp"

namespace naviai::log::l2_log {

namespace {

constexpr std::uint64_t kDefaultReplaySegmentSizeBytes =
    500ULL * 1024ULL * 1024ULL;
constexpr std::uint64_t kMinAdaptiveSegmentSizeBytes =
    128ULL * 1024ULL * 1024ULL;
constexpr std::uint64_t kMaxAdaptiveSegmentSizeBytes =
    8ULL * 1024ULL * 1024ULL * 1024ULL;
constexpr int64_t kRenameIntervalUs = 60LL * 1000000LL;
constexpr int64_t kReplayRetentionWindowUs = 48LL * 60LL * 60LL * 1000000LL;
constexpr size_t kSegmentOverlapRecordCount = 20U;
constexpr char kStaticMapTopic[] = "/zj_humanoid/navigation/map";
constexpr char kLocalMapTopic[] = "/zj_humanoid/navigation/local_map";

std::uint64_t ClampSegmentSizeBytes(std::uint64_t value) {
    if (value < kMinAdaptiveSegmentSizeBytes) {
        return kMinAdaptiveSegmentSizeBytes;
    }
    if (value > kMaxAdaptiveSegmentSizeBytes) {
        return kMaxAdaptiveSegmentSizeBytes;
    }
    return value;
}

const char* SampleModeRecordTag(L2SampleMode mode) {
    return mode == L2SampleMode::LowFrequency ? "low_5s" : "idle_1hz";
}

LogService BuildReplayNamingService() {
    return LogService(BuildReplaySessionRoot());
}

std::optional<FileNamingPlan> BuildSegmentPlan(const std::string& bucket_name,
                                               const char* extension,
                                               int64_t start_time_us,
                                               std::optional<int64_t> end_time_us) {
    auto service = BuildReplayNamingService();
    const std::string suffix =
        extension[0] == '.' ? std::string(extension + 1) : std::string(extension);
    if (end_time_us.has_value()) {
        return service.BuildSealedFilePlan(bucket_name, suffix, start_time_us, end_time_us);
    }
    return service.BuildActiveFilePlan(bucket_name, suffix, start_time_us);
}

std::uint64_t ResolveSegmentSizeBytes(const StorageRuntime& runtime) {
    if (runtime.target_compressed_segment_size_bytes > 0 &&
        runtime.estimated_compression_ratio > 0.0) {
        const double estimated_raw_bytes =
            static_cast<double>(
                runtime.target_compressed_segment_size_bytes) *
            runtime.estimated_compression_ratio;
        return ClampSegmentSizeBytes(
            static_cast<std::uint64_t>(estimated_raw_bytes));
    }
    return runtime.segment_size_bytes > 0
               ? runtime.segment_size_bytes
               : kDefaultReplaySegmentSizeBytes;
}

bool NeedsRotate(const StorageRuntime& runtime,
                 size_t current_size_bytes,
                 size_t next_write_size) {
    return current_size_bytes > 0 &&
           current_size_bytes + next_write_size > ResolveSegmentSizeBytes(runtime);
}

bool CompressFileToGzip(const std::filesystem::path& source_path) {
    std::error_code ec;
    if (source_path.empty() || !std::filesystem::exists(source_path, ec) ||
        source_path.extension() == ".gz") {
        return false;
    }

    std::ifstream input(source_path, std::ios::binary);
    if (!input.is_open()) {
        return false;
    }

    const auto gzip_path = source_path.string() + ".gz";
    gzFile output = gzopen(gzip_path.c_str(), "wb");
    if (output == nullptr) {
        return false;
    }

    char buffer[8192];
    while (input.good()) {
        input.read(buffer, sizeof(buffer));
        const auto bytes = static_cast<unsigned int>(input.gcount());
        if (bytes > 0 && gzwrite(output, buffer, bytes) != static_cast<int>(bytes)) {
            gzclose(output);
            std::filesystem::remove(gzip_path, ec);
            return false;
        }
    }

    gzclose(output);
    input.close();
    std::filesystem::remove(source_path, ec);
    return !ec;
}

bool TryParseStartTimeFromReplayPath(const std::filesystem::path& path,
                                     int64_t* start_time_us) {
    if (start_time_us == nullptr) {
        return false;
    }
    std::string name = path.filename().string();
    if (name.size() > 3 && name.substr(name.size() - 3) == ".gz") {
        name.resize(name.size() - 3);
    }
    const auto dot_pos = name.rfind('.');
    if (dot_pos != std::string::npos) {
        name.resize(dot_pos);
    }
    const auto dash_pos = name.find('-');
    if (dash_pos == std::string::npos) {
        return false;
    }
    const std::string start_text = name.substr(0, dash_pos);
    std::tm tm_buf{};
    std::istringstream stream(start_text);
    stream >> std::get_time(&tm_buf, "%Y%m%d_%H%M%S");
    if (stream.fail()) {
        return false;
    }
    *start_time_us = static_cast<int64_t>(std::mktime(&tm_buf)) * 1000000LL;
    return *start_time_us > 0;
}

std::filesystem::path BuildSegmentVariantPath(const std::filesystem::path& path,
                                              std::uint32_t suffix) {
    const auto extension = path.extension().string();
    const auto stem = path.stem().string();
    std::ostringstream oss;
    oss << stem << "_part_" << suffix << extension;
    return path.parent_path() / oss.str();
}

void ResolveClosedSegmentPaths(std::filesystem::path* data_path,
                               std::filesystem::path* index_path) {
    if (data_path == nullptr || index_path == nullptr ||
        data_path->empty() || index_path->empty()) {
        return;
    }

    std::error_code ec;
    const bool data_exists = std::filesystem::exists(*data_path, ec) ||
                             std::filesystem::exists(
                                 std::filesystem::path(data_path->string() + ".gz"), ec);
    ec.clear();
    const bool index_exists = std::filesystem::exists(*index_path, ec) ||
                              std::filesystem::exists(
                                  std::filesystem::path(index_path->string() + ".gz"), ec);
    if (!data_exists && !index_exists) {
        return;
    }

    for (std::uint32_t suffix = 1; suffix < 1000000; ++suffix) {
        const auto candidate_data = BuildSegmentVariantPath(*data_path, suffix);
        const auto candidate_index = BuildSegmentVariantPath(*index_path, suffix);
        const bool candidate_data_exists =
            std::filesystem::exists(candidate_data, ec) ||
            std::filesystem::exists(
                std::filesystem::path(candidate_data.string() + ".gz"), ec);
        ec.clear();
        const bool candidate_index_exists =
            std::filesystem::exists(candidate_index, ec) ||
            std::filesystem::exists(
                std::filesystem::path(candidate_index.string() + ".gz"), ec);
        ec.clear();
        if (!candidate_data_exists && !candidate_index_exists) {
            *data_path = candidate_data;
            *index_path = candidate_index;
            return;
        }
    }
}

bool IsEpochYearStartTimeUs(int64_t start_time_us) {
    if (start_time_us <= 0) {
        return false;
    }
    const auto time = static_cast<std::time_t>(start_time_us / 1000000LL);
    std::tm tm_buf{};
    localtime_r(&time, &tm_buf);
    return (tm_buf.tm_year + 1900) == 1970;
}

void CleanupExpiredReplayFilesImpl() {
    auto& state = State();
    const auto replay_root = BuildReplaySessionRoot();
    std::error_code ec;
    if (replay_root.empty() || !std::filesystem::exists(replay_root, ec)) {
        return;
    }

    const int64_t cutoff_time_us = NowTimestampUs() - kReplayRetentionWindowUs;
    std::set<std::filesystem::path> active_paths;
    if (!state.session_meta_path.empty()) {
        active_paths.insert(state.session_meta_path.lexically_normal());
    }
    for (const auto& item : state.storage_buckets) {
        if (!item.second.data_path.empty()) {
            active_paths.insert(item.second.data_path.lexically_normal());
        }
        if (!item.second.index_path.empty()) {
            active_paths.insert(item.second.index_path.lexically_normal());
        }
    }

    for (const auto& entry :
         std::filesystem::recursive_directory_iterator(replay_root, ec)) {
        if (ec) {
            break;
        }
        std::error_code entry_ec;
        if (!entry.is_regular_file(entry_ec)) {
            continue;
        }
        const auto normalized_path = entry.path().lexically_normal();
        if (active_paths.find(normalized_path) != active_paths.end()) {
            continue;
        }
        int64_t start_time_us = 0;
        if (!TryParseStartTimeFromReplayPath(normalized_path, &start_time_us) ||
            IsEpochYearStartTimeUs(start_time_us) ||
            start_time_us > cutoff_time_us) {
            continue;
        }
        std::filesystem::remove(normalized_path, entry_ec);
    }
}

void WriteBufferedRecordToCurrentSegment(StorageRuntime* runtime,
                                         const BufferedReplayRecord& record) {
    if (runtime == nullptr) {
        return;
    }

    runtime->data_stream.seekp(0, std::ios::end);
    const uint64_t offset = static_cast<uint64_t>(runtime->data_stream.tellp());
    runtime->data_stream.write(
        reinterpret_cast<const char*>(&record.header), sizeof(record.header));
    runtime->data_stream.write(
        record.metadata_json.data(),
        static_cast<std::streamsize>(record.metadata_json.size()));
    runtime->data_stream.write(record.payload.data(),
                               static_cast<std::streamsize>(record.payload.size()));
    runtime->data_stream.flush();
    runtime->current_data_size_bytes +=
        sizeof(record.header) + record.metadata_json.size() + record.payload.size();

    runtime->index_stream << offset << '\t' << record.index_line_without_offset;
    runtime->index_stream.flush();
    runtime->current_index_size_bytes +=
        std::to_string(offset).size() + 1 + record.index_line_without_offset.size();
}

void RenameActiveTopicStorage(StorageRuntime* runtime) {
    if (runtime == nullptr || runtime->data_path.empty() || runtime->index_path.empty() ||
        runtime->segment_start_message_time_us <= 0 ||
        runtime->segment_end_message_time_us <= 0) {
        return;
    }

    std::error_code ec;
    const auto final_data_plan = BuildSegmentPlan(runtime->bucket_name,
                                                  ".data",
                                                  runtime->segment_start_message_time_us,
                                                  runtime->segment_end_message_time_us);
    const auto final_index_plan = BuildSegmentPlan(runtime->bucket_name,
                                                   ".idx",
                                                   runtime->segment_start_message_time_us,
                                                   runtime->segment_end_message_time_us);
    if (!final_data_plan.has_value() || !final_index_plan.has_value()) {
        return;
    }

    auto final_data_path = final_data_plan->path;
    auto final_index_path = final_index_plan->path;
    if (final_data_path != runtime->data_path || final_index_path != runtime->index_path) {
        ResolveClosedSegmentPaths(&final_data_path, &final_index_path);
    }

    if (final_data_path != runtime->data_path) {
        std::filesystem::rename(runtime->data_path, final_data_path, ec);
        if (!ec) {
            runtime->data_path = final_data_path;
        } else {
            ec.clear();
        }
    }

    if (final_index_path != runtime->index_path) {
        std::filesystem::rename(runtime->index_path, final_index_path, ec);
        if (!ec) {
            runtime->index_path = final_index_path;
        }
    }
}

void CloseTopicStorage(StorageRuntime* runtime,
                       bool compress_closed_segment,
                       bool include_end_time_in_name) {
    if (runtime == nullptr) {
        return;
    }

    std::error_code ec;
    if (runtime->data_stream.is_open()) {
        runtime->data_stream.flush();
        runtime->data_stream.close();
    }
    if (runtime->index_stream.is_open()) {
        runtime->index_stream.flush();
        runtime->index_stream.close();
    }

    if (runtime->data_path.empty() || runtime->index_path.empty() ||
        runtime->segment_start_message_time_us <= 0 ||
        runtime->segment_end_message_time_us <= 0) {
        return;
    }

    if (include_end_time_in_name) {
        RenameActiveTopicStorage(runtime);
    }

    const std::uint64_t raw_segment_size_bytes =
        runtime->current_data_size_bytes + runtime->current_index_size_bytes;

    if (compress_closed_segment) {
        const auto data_gzip_path =
            std::filesystem::path(runtime->data_path.string() + ".gz");
        const auto index_gzip_path =
            std::filesystem::path(runtime->index_path.string() + ".gz");

        const bool data_compressed = CompressFileToGzip(runtime->data_path);
        const bool index_compressed = CompressFileToGzip(runtime->index_path);

        if (runtime->target_compressed_segment_size_bytes > 0 &&
            raw_segment_size_bytes > 0 && data_compressed && index_compressed) {
            std::error_code size_ec;
            const auto compressed_size_bytes =
                static_cast<std::uint64_t>(
                    std::filesystem::file_size(data_gzip_path, size_ec)) +
                static_cast<std::uint64_t>(
                    std::filesystem::file_size(index_gzip_path, size_ec));
            if (!size_ec && compressed_size_bytes > 0) {
                const double observed_ratio =
                    static_cast<double>(raw_segment_size_bytes) /
                    static_cast<double>(compressed_size_bytes);
                runtime->estimated_compression_ratio =
                    runtime->estimated_compression_ratio > 0.0
                        ? runtime->estimated_compression_ratio * 0.7 +
                              observed_ratio * 0.3
                        : observed_ratio;
            }
        }

        State().completed_files.push_back(data_gzip_path);
        State().completed_files.push_back(index_gzip_path);
    } else {
        State().completed_files.push_back(runtime->data_path);
        State().completed_files.push_back(runtime->index_path);
    }

    runtime->data_path.clear();
    runtime->index_path.clear();
    runtime->current_data_size_bytes = 0;
    runtime->current_index_size_bytes = 0;
    runtime->segment_start_message_time_us = 0;
    runtime->segment_end_message_time_us = 0;
    runtime->last_rename_time_bucket_us = -1;
    CleanupExpiredReplayFilesImpl();
}

}  // namespace

void CleanupExpiredReplayFiles() {
    CleanupExpiredReplayFilesImpl();
}

RecorderState& State() {
    static RecorderState state;
    return state;
}

int64_t NowTimestampUs() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

int64_t ResolveMessageTimeUs(int64_t message_time_us) {
    return message_time_us > 0 ? message_time_us : NowTimestampUs();
}

const char* SampleModeName(L2SampleMode mode) {
    switch (mode) {
        case L2SampleMode::Full:
            return "full";
        case L2SampleMode::Normal:
            return "normal";
        case L2SampleMode::LowFrequency:
            return "low";
    }
    return "normal";
}

void RefreshTaskStateByTime(int64_t message_time_us) {
    auto& state = State();
    if (!state.task_active || state.task_idle_deadline_us <= 0) {
        return;
    }

    const int64_t effective_time_us = ResolveMessageTimeUs(message_time_us);
    if (effective_time_us < state.task_idle_deadline_us) {
        return;
    }

    state.task_active = false;
    state.task_id.clear();
    state.task_idle_deadline_us = 0;
}

std::string EscapeJson(const std::string& value) {
    std::string output;
    output.reserve(value.size() + 8);
    for (const char ch : value) {
        switch (ch) {
            case '"':
                output += "\\\"";
                break;
            case '\\':
                output += "\\\\";
                break;
            case '\n':
                output += "\\n";
                break;
            case '\r':
                output += "\\r";
                break;
            case '\t':
                output += "\\t";
                break;
            default:
                output.push_back(ch);
                break;
        }
    }
    return output;
}

std::string Sanitize(std::string_view input) {
    std::string output;
    for (const char ch : input) {
        if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
            (ch >= '0' && ch <= '9')) {
            output.push_back(static_cast<char>(std::tolower(
                static_cast<unsigned char>(ch))));
        } else {
            output.push_back('_');
        }
    }
    while (!output.empty() && output.front() == '_') {
        output.erase(output.begin());
    }
    return output.empty() ? "unknown" : output;
}

std::string TopicLeafName(std::string_view topic) {
    const size_t pos = topic.find_last_of('/');
    if (pos == std::string_view::npos || pos + 1 >= topic.size()) {
        return Sanitize(topic);
    }
    return Sanitize(topic.substr(pos + 1));
}

std::string DomainName(L2SourceDomain domain) {
    (void)domain;
    return "algorithm";
}

std::string FormatSummaryJson(const std::map<std::string, std::string>& summary) {
    std::ostringstream oss;
    oss << '{';
    bool first = true;
    for (const auto& item : summary) {
        if (!first) {
            oss << ',';
        }
        first = false;
        oss << '"' << EscapeJson(item.first) << "\":\""
            << EscapeJson(item.second) << '"';
    }
    oss << '}';
    return oss.str();
}

std::string BuildModuleName(const L2TopicDescriptor& descriptor) {
    return Sanitize(descriptor.source_module + "_" + descriptor.topic);
}

std::string BuildOutputGroup(const L2TopicDescriptor& descriptor) {
    return DomainName(descriptor.source_domain) + "/" +
           Sanitize(descriptor.source_type);
}

std::string BuildStorageBucketName(const L2TopicDescriptor& descriptor) {
    if (descriptor.topic == kStaticMapTopic) {
        return "static_data";
    }
    if (descriptor.topic == kLocalMapTopic) {
        return "large_data/" + TopicLeafName(descriptor.topic);
    }
    return "business_data";
}

std::string StorageBucketCategory(const std::string& bucket_name) {
    if (bucket_name == "static_data") {
        return "static_data";
    }
    if (bucket_name.rfind("large_data/", 0) == 0) {
        return "large_data";
    }
    return "business_data";
}

void RegisterTopicRuntime(const L2TopicDescriptor& descriptor) {
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

void RecordTopicWithBusinessSemantics(const L2TopicMessage& message) {
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

std::filesystem::path BuildReplaySessionRoot() {
    return std::filesystem::path(State().options.root_dir);
}

std::string FormatSegmentTime(int64_t message_time_us) {
    const auto time_point =
        std::chrono::system_clock::time_point(std::chrono::microseconds(message_time_us));
    const auto time = std::chrono::system_clock::to_time_t(time_point);
    std::tm tm_buf{};
    localtime_r(&time, &tm_buf);

    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%Y%m%d_%H%M%S");
    return oss.str();
}

std::filesystem::path BuildSessionMetaPath(int64_t start_message_time_us,
                                           int64_t end_message_time_us) {
    auto service = BuildReplayNamingService();
    const auto plan =
        service.BuildSealedFilePlan("", "meta", start_message_time_us, end_message_time_us);
    if (!plan.has_value()) {
        return BuildReplaySessionRoot() /
               (FormatSegmentTime(start_message_time_us) + "-" +
                FormatSegmentTime(end_message_time_us) + ".meta");
    }
    return plan->path;
}

void RefreshSessionMetaFile(int64_t message_time_us, bool force_rename) {
    auto& state = State();
    const int64_t effective_time_us = ResolveMessageTimeUs(message_time_us);
    if (state.session_start_message_time_us <= 0) {
        state.session_start_message_time_us = effective_time_us;
    }
    if (effective_time_us > state.session_end_message_time_us) {
        state.session_end_message_time_us = effective_time_us;
    }

    const auto next_meta_path = BuildSessionMetaPath(state.session_start_message_time_us,
                                                     state.session_end_message_time_us);
    const int64_t rename_bucket_us = state.session_end_message_time_us / kRenameIntervalUs;
    const bool should_rename = force_rename ||
                               rename_bucket_us > state.session_meta_rename_time_bucket_us ||
                               state.session_meta_path.empty();

    if (should_rename && !state.session_meta_path.empty() &&
        state.session_meta_path != next_meta_path) {
        std::error_code ec;
        std::filesystem::rename(state.session_meta_path, next_meta_path, ec);
        if (!ec) {
            for (auto& file_path : state.completed_files) {
                if (file_path == state.session_meta_path) {
                    file_path = next_meta_path;
                    break;
                }
            }
            state.session_meta_path = next_meta_path;
        }
    }

    if (state.session_meta_path.empty()) {
        state.session_meta_path = next_meta_path;
        state.completed_files.push_back(state.session_meta_path);
    } else if (should_rename) {
        state.session_meta_path = next_meta_path;
    }

    std::ofstream meta(state.session_meta_path, std::ios::out | std::ios::trunc);
    meta << "session_id=" << state.session_id << '\n';
    meta << "host=" << state.options.host << '\n';
    meta << "container=" << state.options.container << '\n';
    meta << "root_dir=" << state.options.root_dir << '\n';
    meta << "sample_mode=" << SampleModeName(state.options.sample_mode) << '\n';
    meta << "start_time=" << FormatSegmentTime(state.session_start_message_time_us) << '\n';
    meta << "end_time=" << FormatSegmentTime(state.session_end_message_time_us) << '\n';
    state.session_meta_rename_time_bucket_us = rename_bucket_us;
    CleanupExpiredReplayFilesImpl();
}

StorageRuntime* FindStorageRuntime(const TopicRuntime& runtime) {
    auto& buckets = State().storage_buckets;
    const auto it = buckets.find(runtime.storage_bucket_name);
    return it == buckets.end() ? nullptr : &it->second;
}

void EnsureTopicStorage(StorageRuntime* runtime, int64_t initial_message_time_us) {
    if (runtime == nullptr) {
        return;
    }
    if (runtime->data_stream.is_open() && runtime->index_stream.is_open()) {
        return;
    }

    std::error_code ec;
    std::filesystem::create_directories(BuildReplaySessionRoot() / runtime->bucket_name, ec);

    const int64_t effective_time_us = ResolveMessageTimeUs(initial_message_time_us);
    if (runtime->segment_start_message_time_us <= 0) {
        runtime->segment_start_message_time_us = effective_time_us;
    }
    if (runtime->segment_end_message_time_us <= 0) {
        runtime->segment_end_message_time_us = effective_time_us;
    }

    const auto data_plan = BuildSegmentPlan(runtime->bucket_name,
                                            ".data",
                                            runtime->segment_start_message_time_us,
                                            std::nullopt);
    const auto index_plan = BuildSegmentPlan(runtime->bucket_name,
                                             ".idx",
                                             runtime->segment_start_message_time_us,
                                             std::nullopt);
    if (!data_plan.has_value() || !index_plan.has_value()) {
        throw std::runtime_error("failed to build replay segment naming plan");
    }
    runtime->data_path = data_plan->path;
    runtime->index_path = index_plan->path;
    runtime->data_stream.open(runtime->data_path,
                              std::ios::out | std::ios::app | std::ios::binary);
    runtime->index_stream.open(runtime->index_path, std::ios::out | std::ios::app);
    runtime->current_data_size_bytes =
        std::filesystem::exists(runtime->data_path, ec)
            ? static_cast<size_t>(std::filesystem::file_size(runtime->data_path, ec))
            : 0U;
    runtime->current_index_size_bytes =
        std::filesystem::exists(runtime->index_path, ec)
            ? static_cast<size_t>(std::filesystem::file_size(runtime->index_path, ec))
            : 0U;
}

void RotateTopicStorageIfNeeded(StorageRuntime* runtime,
                                int64_t message_time_us,
                                size_t next_data_write_size,
                                size_t next_index_write_size) {
    if (runtime == nullptr) {
        return;
    }

    EnsureTopicStorage(runtime, message_time_us);
    if (!NeedsRotate(*runtime, runtime->current_data_size_bytes, next_data_write_size) &&
        !NeedsRotate(*runtime, runtime->current_index_size_bytes, next_index_write_size)) {
        return;
    }

    SwitchTopicStorage(runtime, message_time_us);
}

void SwitchTopicStorage(StorageRuntime* runtime, int64_t next_message_time_us) {
    if (runtime == nullptr) {
        return;
    }

    CloseTopicStorage(runtime, true, true);
    ++runtime->segment_index;
    runtime->current_data_size_bytes = 0;
    runtime->current_index_size_bytes = 0;
    if (!runtime->recent_records.empty()) {
        runtime->segment_start_message_time_us = runtime->recent_records.front().header.record_time_us;
        runtime->segment_end_message_time_us = runtime->recent_records.front().header.record_time_us;
    } else {
        runtime->segment_start_message_time_us = 0;
        runtime->segment_end_message_time_us = 0;
    }
    EnsureTopicStorage(runtime, next_message_time_us);
    const auto overlap_records = runtime->recent_records;
    for (const auto& record : overlap_records) {
        UpdateTopicSegmentTimeRange(runtime, record.header.record_time_us);
        WriteBufferedRecordToCurrentSegment(runtime, record);
    }
    if (!runtime->recent_records.empty()) {
        runtime->last_rename_time_bucket_us =
            runtime->segment_end_message_time_us / kRenameIntervalUs;
    }
}

void UpdateTopicSegmentTimeRange(StorageRuntime* runtime, int64_t record_time_us) {
    if (runtime == nullptr || record_time_us <= 0) {
        return;
    }
    if (runtime->segment_start_message_time_us <= 0 ||
        record_time_us < runtime->segment_start_message_time_us) {
        runtime->segment_start_message_time_us = record_time_us;
    }
    if (runtime->segment_end_message_time_us <= 0 ||
        record_time_us > runtime->segment_end_message_time_us) {
        runtime->segment_end_message_time_us = record_time_us;
    }
}

std::string BuildIndexLineWithoutOffset(const TopicRuntime& runtime,
                                        const ReplayBinaryRecordHeader& header,
                                        const std::string& sample_mode,
                                        const L2TopicMessage& message,
                                        const std::string& payload_summary_json) {
    std::ostringstream oss;
    oss << header.record_time_us << '\t'
        << header.message_time_us << '\t'
        << header.record_id << '\t'
        << runtime.descriptor.topic << '\t'
        << runtime.descriptor.topic_type << '\t'
        << sample_mode << '\t'
        << message.action_phase << '\t'
        << message.action_state << '\t'
        << message.task_id << '\t'
        << header.payload_size << '\t'
        << payload_summary_json << '\n';
    return oss.str();
}

const std::string& ReplayPayloadForWrite(const L2TopicMessage& message) {
    if (!message.replay_payload.empty()) {
        return message.replay_payload;
    }
    return message.payload;
}

void AppendRecentRecord(StorageRuntime* runtime, const BufferedReplayRecord& record) {
    if (runtime == nullptr) {
        return;
    }
    runtime->recent_records.push_back(record);
    while (runtime->recent_records.size() > kSegmentOverlapRecordCount) {
        runtime->recent_records.pop_front();
    }
}

void FinalizeTopicStorage(StorageRuntime* runtime, bool compress_closed_segment) {
    CloseTopicStorage(runtime, compress_closed_segment, runtime != nullptr);
}

std::string BuildReplayMetadataJson(const std::string& session_id,
                                    const L2TopicDescriptor& descriptor,
                                    const L2TopicMessage& message,
                                    const std::string& sample_mode,
                                    const std::string& sample_reason) {
    const int64_t record_time_us = NowTimestampUs();
    const int64_t message_time_us =
        message.message_time_us > 0 ? message.message_time_us : record_time_us;

    std::ostringstream oss;
    oss << '{'
        << "\"record_time_us\":" << record_time_us << ','
        << "\"message_time_us\":" << message_time_us << ','
        << "\"session_id\":\"" << EscapeJson(session_id) << "\","
        << "\"task_id\":\"" << EscapeJson(message.task_id) << "\","
        << "\"topic\":\"" << EscapeJson(descriptor.topic) << "\","
        << "\"topic_type\":\"" << EscapeJson(descriptor.topic_type) << "\","
        << "\"source_module\":\"" << EscapeJson(descriptor.source_module) << "\","
        << "\"source_domain\":\"" << DomainName(descriptor.source_domain) << "\","
        << "\"source_type\":\"" << EscapeJson(descriptor.source_type) << "\","
        << "\"layer\":\"L2\","
        << "\"l2_type\":\"topic_record\","
        << "\"sample_mode\":\"" << EscapeJson(sample_mode) << "\","
        << "\"sample_reason\":\"" << EscapeJson(sample_reason) << "\","
        << "\"action_phase\":\"" << EscapeJson(message.action_phase) << "\","
        << "\"action_state\":\"" << EscapeJson(message.action_state) << "\","
        << "\"frame_id\":\"" << EscapeJson(message.frame_id) << "\","
        << "\"sequence\":" << message.sequence << ','
        << "\"payload_encoding\":\"" << EscapeJson(message.payload_encoding) << "\","
        << "\"payload_summary\":" << FormatSummaryJson(message.payload_summary)
        << '}';
    return oss.str();
}

void WriteReplayRecord(const TopicRuntime& runtime,
                       const L2TopicMessage& message,
                       const std::string& sample_mode,
                       const std::string& sample_reason) {
    auto* storage = FindStorageRuntime(runtime);
    if (storage == nullptr) {
        throw std::runtime_error("storage bucket is not registered: " +
                                 runtime.storage_bucket_name);
    }

    const int64_t message_time_us = ResolveMessageTimeUs(message.message_time_us);
    RefreshSessionMetaFile(message_time_us, false);
    const std::string payload = ReplayPayloadForWrite(message);
    const std::string metadata_json =
        BuildReplayMetadataJson(State().session_id, runtime.descriptor, message,
                                sample_mode, sample_reason);
    ReplayBinaryRecordHeader header{};
    header.metadata_size = static_cast<uint32_t>(metadata_json.size());
    header.payload_size = static_cast<uint32_t>(payload.size());
    header.record_time_us = NowTimestampUs();
    header.message_time_us = message_time_us;
    header.sequence = message.sequence;
    header.record_id = storage->next_record_id++;

    BufferedReplayRecord record;
    record.header = header;
    record.metadata_json = metadata_json;
    record.payload = payload;
    record.index_line_without_offset = BuildIndexLineWithoutOffset(
        runtime, header, sample_mode, message, FormatSummaryJson(message.payload_summary));

    const size_t next_data_write_size =
        sizeof(record.header) + record.metadata_json.size() + record.payload.size();
    const size_t next_index_write_size =
        std::to_string(storage->current_data_size_bytes).size() + 1 +
        record.index_line_without_offset.size();
    RotateTopicStorageIfNeeded(storage, header.record_time_us, next_data_write_size,
                               next_index_write_size);
    UpdateTopicSegmentTimeRange(storage, header.record_time_us);
    WriteBufferedRecordToCurrentSegment(storage, record);
    AppendRecentRecord(storage, record);

    const int64_t rename_bucket_us = header.record_time_us / kRenameIntervalUs;
    if (storage->segment_index > 0 &&
        rename_bucket_us > storage->last_rename_time_bucket_us) {
        storage->last_rename_time_bucket_us = rename_bucket_us;
    }
    RefreshSessionMetaFile(header.message_time_us, false);
}

void FlushPendingTopic(TopicRuntime* runtime, const std::string& reason) {
    if (runtime == nullptr || !runtime->has_pending) {
        return;
    }
    WriteReplayRecord(*runtime, runtime->pending_message,
                      SampleModeRecordTag(State().options.sample_mode), reason);
    runtime->has_pending = false;
}

}  // namespace naviai::log::l2_log
