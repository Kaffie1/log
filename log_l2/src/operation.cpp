#include "operation.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <unordered_set>

#include <zlib.h>

#include "business_data.hpp"
#include "large_data.hpp"
#include "recorder.hpp"
#include "static_data.hpp"

namespace naviai::log::l2_log {

namespace {

constexpr char kTopicRecordLogModuleName[] = "l2_topic_record";
constexpr char kTopicRecordLogOutputGroup[] = "replay/topic_record";
constexpr char kStaticMapTopicRecordLogModuleName[] = "l2_topic_record_map";
constexpr char kStaticMapTopicRecordLogOutputGroup[] = "replay/static_map";
constexpr char kStaticMapTopic[] = "/zj_humanoid/navigation/map";

struct SegmentFileGroup {
    std::filesystem::path data_path;
    std::filesystem::path index_path;
    std::filesystem::path relative_data_path;
    std::filesystem::path relative_index_path;
    int64_t start_time_us{0};
    int64_t end_time_us{0};
};

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

bool ReadSegmentTimeRangeFromIndex(const std::filesystem::path& index_path,
                                   int64_t* start_message_time_us,
                                   int64_t* end_message_time_us) {
    if (start_message_time_us == nullptr || end_message_time_us == nullptr) {
        return false;
    }

    std::ifstream input(index_path);
    if (!input.is_open()) {
        return false;
    }

    std::string line;
    bool found = false;
    int64_t min_time_us = 0;
    int64_t max_time_us = 0;
    while (std::getline(input, line)) {
        if (line.empty()) {
            continue;
        }

        std::istringstream iss(line);
        std::vector<std::string> fields;
        std::string field;
        while (std::getline(iss, field, '\t')) {
            fields.push_back(field);
        }
        if (fields.size() < 2) {
            continue;
        }

        try {
            // New index format stores record_time_us in column 2. Legacy indexes
            // store message_time_us there, so reading column 2 keeps compatibility.
            const std::string& time_field = fields[1];
            const int64_t parsed_time_us = std::stoll(time_field);
            if (!found) {
                min_time_us = parsed_time_us;
                max_time_us = parsed_time_us;
                found = true;
            }
            min_time_us = std::min(min_time_us, parsed_time_us);
            max_time_us = std::max(max_time_us, parsed_time_us);
        } catch (...) {
            continue;
        }
    }

    if (!found) {
        return false;
    }

    *start_message_time_us = min_time_us;
    *end_message_time_us = max_time_us;
    return true;
}

bool ReadSegmentTimeRangeFromMaybeCompressedIndex(
    const std::filesystem::path& index_path,
    int64_t* start_message_time_us,
    int64_t* end_message_time_us) {
    if (start_message_time_us == nullptr || end_message_time_us == nullptr) {
        return false;
    }

    if (index_path.extension() != ".gz") {
        return ReadSegmentTimeRangeFromIndex(index_path, start_message_time_us,
                                             end_message_time_us);
    }

    gzFile input = gzopen(index_path.string().c_str(), "rb");
    if (input == nullptr) {
        return false;
    }

    char buffer[8192];
    std::string line;
    bool found = false;
    int64_t min_time_us = 0;
    int64_t max_time_us = 0;
    while (gzgets(input, buffer, static_cast<int>(sizeof(buffer))) != nullptr) {
        line.assign(buffer);
        while (!line.empty() &&
               (line.back() == '\n' || line.back() == '\r')) {
            line.pop_back();
        }
        if (line.empty()) {
            continue;
        }

        std::istringstream iss(line);
        std::vector<std::string> fields;
        std::string field;
        while (std::getline(iss, field, '\t')) {
            fields.push_back(field);
        }
        if (fields.size() < 2) {
            continue;
        }

        try {
            // New index format stores record_time_us in column 2. Legacy indexes
            // store message_time_us there, so reading column 2 keeps compatibility.
            const std::string& time_field = fields[1];
            const int64_t parsed_time_us = std::stoll(time_field);
            if (!found) {
                min_time_us = parsed_time_us;
                max_time_us = parsed_time_us;
                found = true;
            }
            min_time_us = std::min(min_time_us, parsed_time_us);
            max_time_us = std::max(max_time_us, parsed_time_us);
        } catch (...) {
            continue;
        }
    }

    gzclose(input);
    if (!found) {
        return false;
    }

    *start_message_time_us = min_time_us;
    *end_message_time_us = max_time_us;
    return true;
}

std::filesystem::path BuildRecoveredSegmentPath(const std::filesystem::path& source_path,
                                                int64_t start_message_time_us,
                                                int64_t end_message_time_us,
                                                const char* extension) {
    const std::string start = FormatSegmentTime(start_message_time_us);
    const std::string end = FormatSegmentTime(end_message_time_us);
    std::filesystem::path candidate =
        source_path.parent_path() / (start + "-" + end + extension);
    if (!std::filesystem::exists(candidate)) {
        return candidate;
    }

    for (std::uint32_t suffix = 1; suffix < 1000000; ++suffix) {
        std::ostringstream oss;
        oss << start << "-" << end << "_recovered_" << suffix << extension;
        candidate = source_path.parent_path() / oss.str();
        if (!std::filesystem::exists(candidate)) {
            return candidate;
        }
    }

    return source_path;
}

void RenameRecoveredSegmentPair(const std::filesystem::path& data_path,
                                const std::filesystem::path& index_path) {
    int64_t start_message_time_us = 0;
    int64_t end_message_time_us = 0;
    if (!ReadSegmentTimeRangeFromIndex(index_path, &start_message_time_us,
                                       &end_message_time_us) ||
        start_message_time_us <= 0 || end_message_time_us <= 0) {
        return;
    }

    std::error_code ec;
    const auto recovered_data_path =
        BuildRecoveredSegmentPath(data_path, start_message_time_us, end_message_time_us,
                                  ".data");
    const auto recovered_index_path =
        BuildRecoveredSegmentPath(index_path, start_message_time_us, end_message_time_us,
                                  ".idx");

    if (recovered_data_path != data_path) {
        std::filesystem::rename(data_path, recovered_data_path, ec);
        if (ec) {
            return;
        }
    }

    ec.clear();
    if (recovered_index_path != index_path) {
        std::filesystem::rename(index_path, recovered_index_path, ec);
    }
}

void RecoverReplayFilesBeforeStart(const std::filesystem::path& replay_root) {
    std::error_code ec;
    if (replay_root.empty() || !std::filesystem::exists(replay_root, ec)) {
        return;
    }

    for (const auto& entry : std::filesystem::recursive_directory_iterator(replay_root)) {
        if (!entry.is_regular_file()) {
            continue;
        }

        const auto path = entry.path();
        if (path.extension() != ".idx") {
            continue;
        }

        const auto data_path = path.parent_path() / (path.stem().string() + ".data");
        if (!std::filesystem::exists(data_path, ec)) {
            continue;
        }

        RenameRecoveredSegmentPair(data_path, path);
    }

    for (const auto& entry : std::filesystem::recursive_directory_iterator(replay_root)) {
        if (!entry.is_regular_file()) {
            continue;
        }

        const auto path = entry.path();
        const auto extension = path.extension().string();
        if (extension != ".data" && extension != ".idx") {
            continue;
        }

        const auto gzip_path = std::filesystem::path(path.string() + ".gz");
        if (std::filesystem::exists(gzip_path, ec)) {
            continue;
        }

        CompressFileToGzip(path);
    }
}

void CollectTopicRecordLogFiles(const std::filesystem::path& root_dir,
                                std::vector<std::filesystem::path>* files) {
    if (files == nullptr) {
        return;
    }

    const std::filesystem::path log_roots[] = {
        root_dir / "module" / kTopicRecordLogOutputGroup,
        root_dir / "module" / kStaticMapTopicRecordLogOutputGroup,
    };
    const char* prefixes[] = {
        kTopicRecordLogModuleName,
        kStaticMapTopicRecordLogModuleName,
    };

    for (size_t index = 0; index < std::size(log_roots); ++index) {
        std::error_code ec;
        if (!std::filesystem::exists(log_roots[index], ec)) {
            continue;
        }
        for (const auto& entry :
             std::filesystem::recursive_directory_iterator(log_roots[index], ec)) {
            if (ec) {
                break;
            }
            if (!entry.is_regular_file()) {
                continue;
            }
            const auto path = entry.path();
            const auto name = path.filename().string();
            if (name.rfind(prefixes[index], 0) != 0) {
                continue;
            }
            files->push_back(path);
        }
    }
}

std::string ShellEscapeSingleQuoted(const std::string& value) {
    std::string output;
    output.reserve(value.size() + 8);
    output.push_back('\'');
    for (const char ch : value) {
        if (ch == '\'') {
            output += "'\\''";
        } else {
            output.push_back(ch);
        }
    }
    output.push_back('\'');
    return output;
}

bool CopyFileToBundle(const std::filesystem::path& source_path,
                      const std::filesystem::path& relative_path,
                      const std::filesystem::path& bundle_root) {
    if (source_path.empty() || relative_path.empty() ||
        !std::filesystem::exists(source_path) ||
        !std::filesystem::is_regular_file(source_path)) {
        return false;
    }

    const auto output_path = bundle_root / relative_path;
    std::error_code ec;
    std::filesystem::create_directories(output_path.parent_path(), ec);
    if (ec) {
        return false;
    }

    std::ifstream input(source_path, std::ios::binary);
    std::ofstream output(output_path, std::ios::out | std::ios::trunc | std::ios::binary);
    output << input.rdbuf();
    return input.good() || input.eof();
}

void CollectSegmentGroups(const std::filesystem::path& root_dir,
                          std::vector<SegmentFileGroup>* groups) {
    if (groups == nullptr) {
        return;
    }

    std::error_code ec;
    if (!std::filesystem::exists(root_dir, ec)) {
        return;
    }

    for (const auto& entry : std::filesystem::recursive_directory_iterator(root_dir, ec)) {
        if (ec || !entry.is_regular_file()) {
            continue;
        }

        const auto path = entry.path();
        const auto name = path.filename().string();
        const bool is_gzip = path.extension() == ".gz";
        const auto stem_name = is_gzip ? path.stem().filename().string() : name;
        const auto extension = is_gzip ? path.stem().extension().string()
                                       : path.extension().string();
        if (extension != ".idx") {
            continue;
        }

        std::filesystem::path data_path;
        if (is_gzip) {
            data_path = path.parent_path() / (path.stem().stem().string() + ".data.gz");
        } else {
            data_path = path.parent_path() / (path.stem().string() + ".data");
            if (!std::filesystem::exists(data_path, ec)) {
                data_path = path.parent_path() / (path.stem().string() + ".data.gz");
            }
        }
        if (!std::filesystem::exists(data_path, ec)) {
            continue;
        }

        SegmentFileGroup group;
        group.index_path = path;
        group.data_path = data_path;
        group.relative_index_path = std::filesystem::relative(path, root_dir, ec);
        if (ec) {
            ec.clear();
            continue;
        }
        group.relative_data_path = std::filesystem::relative(data_path, root_dir, ec);
        if (ec) {
            ec.clear();
            continue;
        }
        if (!ReadSegmentTimeRangeFromMaybeCompressedIndex(path, &group.start_time_us,
                                                          &group.end_time_us)) {
            continue;
        }
        groups->push_back(std::move(group));
    }
}

bool OverlapsRange(const SegmentFileGroup& group,
                   int64_t start_time_us,
                   int64_t end_time_us) {
    return group.start_time_us <= end_time_us && group.end_time_us >= start_time_us;
}

std::string BuildDefaultArchiveName(int64_t start_time_us, int64_t end_time_us) {
    return "l2_" + FormatSegmentTime(start_time_us) + "-" +
           FormatSegmentTime(end_time_us) + ".tar.xz";
}

void WritePackageMeta(const std::filesystem::path& bundle_root,
                      const L2PackageOptions& options,
                      const std::vector<SegmentFileGroup>& selected_groups) {
    std::ofstream meta(bundle_root / "package.meta", std::ios::out | std::ios::trunc);
    meta << "root_dir=" << options.root_dir << '\n';
    meta << "start_time=" << FormatSegmentTime(options.start_time_us) << '\n';
    meta << "end_time=" << FormatSegmentTime(options.end_time_us) << '\n';
    meta << "segment_count=" << selected_groups.size() << '\n';
}

std::string BuildArchiveFromBundle(const std::filesystem::path& bundle_root,
                                   const std::filesystem::path& output_path) {
    std::error_code ec;
    const auto parent_path = output_path.parent_path();
    if (!parent_path.empty()) {
        std::filesystem::create_directories(parent_path, ec);
    }
    if (ec) {
        throw std::runtime_error("failed to create output directory: " +
                                 parent_path.string());
    }

    const auto command =
        "tar -C " + ShellEscapeSingleQuoted(bundle_root.parent_path().string()) +
        " -cJf " + ShellEscapeSingleQuoted(output_path.string()) + " " +
        ShellEscapeSingleQuoted(bundle_root.filename().string());
    if (std::system(command.c_str()) != 0) {
        throw std::runtime_error("failed to create tar.xz archive: " +
                                 output_path.string());
    }
    return output_path.string();
}

}  // namespace

const char* TopicRecordLogModuleName() {
    return kTopicRecordLogModuleName;
}

const char* TopicRecordLogModuleNameForTopic(std::string_view topic) {
    if (topic == kStaticMapTopic) {
        return kStaticMapTopicRecordLogModuleName;
    }
    return kTopicRecordLogModuleName;
}

void InitRecorder(const L2RecorderOptions& options) {
    auto& state = State();
    state.options = options;
    state.session_id = options.session_id.empty()
                           ? ("session_" + std::to_string(NowTimestampUs()))
                           : options.session_id;
    state.completed_files.clear();
    state.session_meta_path.clear();
    state.session_start_message_time_us = 0;
    state.session_end_message_time_us = 0;
    state.session_meta_rename_time_bucket_us = -1;
    state.task_id.clear();
    state.task_active = false;
    state.task_idle_deadline_us = 0;
    state.topics.clear();
    state.storage_buckets.clear();
    state.initialized = true;
    const auto replay_root = BuildReplaySessionRoot();
    std::error_code ec;
    std::filesystem::create_directories(replay_root, ec);
    RecoverReplayFilesBeforeStart(replay_root);
    CleanupExpiredReplayFiles();
    for (const auto& descriptor : options.topics) {
        const auto category = StorageBucketCategory(BuildStorageBucketName(descriptor));
        if (category == "static_data") {
            RegisterStaticDataTopic(descriptor);
            continue;
        }
        if (category == "large_data") {
            RegisterLargeDataTopic(descriptor);
            continue;
        }
        RegisterBusinessTopic(descriptor);
    }
}

void SetSampleMode(L2SampleMode mode) {
    auto& state = State();
    if (!state.initialized || state.options.sample_mode == mode) {
        return;
    }

    for (auto& item : state.topics) {
        FlushPendingTopic(&item.second, "sample_mode_switch");
    }
    state.options.sample_mode = mode;
}

void ForceRotateActiveSegments() {
    auto& state = State();
    if (!state.initialized) {
        return;
    }

    for (auto& item : state.topics) {
        FlushPendingTopic(&item.second, "front_control_switch");
    }

    const int64_t switch_time_us = NowTimestampUs();
    for (auto& item : state.storage_buckets) {
        auto& runtime = item.second;
        const bool has_open_segment =
            runtime.data_stream.is_open() || runtime.index_stream.is_open() ||
            !runtime.data_path.empty() || !runtime.index_path.empty();
        if (!has_open_segment) {
            continue;
        }
        SwitchTopicStorage(&runtime, switch_time_us);
    }

    if (state.session_end_message_time_us > 0) {
        RefreshSessionMetaFile(state.session_end_message_time_us, true);
    }
}

void SealActiveSegments() {
    auto& state = State();
    if (!state.initialized) {
        return;
    }

    for (auto& item : state.topics) {
        FlushPendingTopic(&item.second, "front_control_seal");
    }
    for (auto& item : state.storage_buckets) {
        FinalizeTopicStorage(&item.second, true);
    }
    if (state.session_end_message_time_us > 0) {
        RefreshSessionMetaFile(state.session_end_message_time_us, true);
    }
    state.initialized = false;
}

void PackageRecords(const std::string& bundle_dir) {
    auto& state = State();
    FlushAll();
    if (state.session_end_message_time_us > 0) {
        RefreshSessionMetaFile(state.session_end_message_time_us, true);
    }
    state.initialized = false;

    const std::filesystem::path root = bundle_dir.empty()
                                           ? std::filesystem::path(state.options.root_dir) /
                                                 (state.session_id + ".bundle")
                                           : std::filesystem::path(bundle_dir);
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);

    const auto root_dir = std::filesystem::path(state.options.root_dir);
    for (const auto& file_path : state.completed_files) {
        if (file_path.empty() || !std::filesystem::exists(file_path) ||
            !std::filesystem::is_regular_file(file_path)) {
            continue;
        }
        const auto relative_path = std::filesystem::relative(file_path, root_dir);
        const auto output_path = root / relative_path;
        std::filesystem::create_directories(output_path.parent_path());

        std::ifstream input(file_path, std::ios::binary);
        std::ofstream output(output_path, std::ios::out | std::ios::trunc | std::ios::binary);
        output << input.rdbuf();
    }
}

std::string PackageRecords(const L2PackageOptions& options) {
    if (options.root_dir.empty()) {
        throw std::invalid_argument("package root_dir must not be empty");
    }
    if (options.start_time_us <= 0 || options.end_time_us <= 0 ||
        options.start_time_us > options.end_time_us) {
        throw std::invalid_argument("invalid package time range");
    }

    const auto root_dir = std::filesystem::path(options.root_dir);
    std::vector<SegmentFileGroup> groups;
    CollectSegmentGroups(root_dir, &groups);

    std::vector<SegmentFileGroup> selected_groups;
    std::optional<SegmentFileGroup> selected_map_group;
    for (const auto& group : groups) {
        const auto relative_text = group.relative_data_path.generic_string();
        const bool is_map_group =
            relative_text.rfind("static_data/", 0) == 0 ||
            group.relative_index_path.generic_string().rfind("static_data/", 0) == 0;
        if (is_map_group) {
            if (!selected_map_group.has_value() ||
                group.start_time_us >= selected_map_group->start_time_us) {
                selected_map_group = group;
            }
            continue;
        }
        if (OverlapsRange(group, options.start_time_us, options.end_time_us)) {
            selected_groups.push_back(group);
        }
    }

    if (selected_map_group.has_value()) {
        selected_groups.push_back(*selected_map_group);
    }

    if (selected_groups.empty()) {
        throw std::runtime_error("no L2 segments matched the requested time range");
    }

    std::sort(selected_groups.begin(), selected_groups.end(),
              [](const SegmentFileGroup& left, const SegmentFileGroup& right) {
                  return left.start_time_us < right.start_time_us;
              });

    const auto temp_root = std::filesystem::temp_directory_path() /
                           ("l2_package_" + std::to_string(NowTimestampUs()));
    const auto bundle_root = temp_root / "l2";
    std::filesystem::create_directories(bundle_root);

    std::unordered_set<std::string> copied_paths;
    for (const auto& group : selected_groups) {
        if (copied_paths.insert(group.relative_data_path.generic_string()).second) {
            CopyFileToBundle(group.data_path, group.relative_data_path, bundle_root);
        }
        if (copied_paths.insert(group.relative_index_path.generic_string()).second) {
            CopyFileToBundle(group.index_path, group.relative_index_path, bundle_root);
        }
    }

    WritePackageMeta(bundle_root, options, selected_groups);

    const auto output_path = options.output_path.empty()
                                 ? (root_dir / BuildDefaultArchiveName(options.start_time_us,
                                                                      options.end_time_us))
                                 : std::filesystem::path(options.output_path);
    const auto archive_path = BuildArchiveFromBundle(bundle_root, output_path);
    std::filesystem::remove_all(temp_root);
    return archive_path;
}

void FlushAll() {
    auto& state = State();
    for (auto& item : state.topics) {
        FlushPendingTopic(&item.second, "flush");
    }
    for (auto& item : state.storage_buckets) {
        if (item.second.data_stream.is_open()) {
            item.second.data_stream.flush();
        }
        if (item.second.index_stream.is_open()) {
            item.second.index_stream.flush();
        }
    }
}

void ShutdownAll() {
    auto& state = State();
    for (auto& item : state.storage_buckets) {
        FinalizeTopicStorage(&item.second, true);
    }
    if (state.session_end_message_time_us > 0) {
        RefreshSessionMetaFile(state.session_end_message_time_us, true);
    }
    state.initialized = false;
}

}  // namespace naviai::log::l2_log
