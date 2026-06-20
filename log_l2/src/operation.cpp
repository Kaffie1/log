#include "operation.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <iomanip>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <unordered_set>

#include <zlib.h>

#include "business_data.hpp"
#include "large_data.hpp"
#include "log_service_naming.hpp"
#include "recorder.hpp"
#include "static_data.hpp"

namespace naviai::log::l2_log {

namespace {

constexpr char kTopicRecordLogModuleName[] = "l2_topic_record";
constexpr char kTopicRecordLogOutputGroup[] = "replay/topic_record";
constexpr char kStaticMapTopicRecordLogModuleName[] = "l2_topic_record_map";
constexpr char kStaticMapTopicRecordLogOutputGroup[] = "replay/static_map";
constexpr char kStaticMapTopic[] = "/zj_humanoid/navigation/map";
constexpr std::int64_t kUtcPlus8OffsetSeconds = 8LL * 60LL * 60LL;
bool g_recovery_checked_once = false;

struct SegmentFileGroup {
    std::filesystem::path data_path;
    std::filesystem::path index_path;
    std::filesystem::path relative_data_path;
    std::filesystem::path relative_index_path;
    int64_t start_time_us{0};
    int64_t end_time_us{0};
};

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

void RenameRecoveredSegmentPair(const std::filesystem::path& data_path,
                                const std::filesystem::path& index_path) {
    const auto replay_root = BuildReplaySessionRoot();
    LogService naming_service(replay_root);
    if (!naming_service.IsActiveFilePath(data_path) ||
        !naming_service.IsActiveFilePath(index_path)) {
        return;
    }

    int64_t start_message_time_us = 0;
    int64_t end_message_time_us = 0;
    if (!ReadSegmentTimeRangeFromIndex(index_path, &start_message_time_us,
                                       &end_message_time_us) ||
        start_message_time_us <= 0 || end_message_time_us <= 0) {
        return;
    }

    naming_service.RecoverActiveFiles({data_path, index_path}, end_message_time_us);
}

void RecoverReplayFilesBeforeStart(const std::filesystem::path& replay_root) {
    std::error_code ec;
    if (replay_root.empty() || !std::filesystem::exists(replay_root, ec)) {
        return;
    }

    for (const auto& entry : std::filesystem::directory_iterator(replay_root, ec)) {
        if (ec) {
            break;
        }
        const auto name = entry.path().filename().string();
        if (name.find("shutdown") == std::string::npos ||
            name.find("bundle") == std::string::npos) {
            continue;
        }
        std::filesystem::remove_all(entry.path(), ec);
        ec.clear();
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

std::optional<std::int64_t> ParseUtcPlus8Timestamp(std::string_view value) {
    if (value.size() < 15) {
        return std::nullopt;
    }
    std::tm tm_value {};
    std::istringstream iss(std::string(value.substr(0, 15)));
    iss >> std::get_time(&tm_value, "%Y%m%d_%H%M%S");
    if (iss.fail()) {
        return std::nullopt;
    }
#if defined(_WIN32)
    const auto utc_seconds = _mkgmtime(&tm_value);
#else
    const auto utc_seconds = timegm(&tm_value);
#endif
    if (utc_seconds == static_cast<std::time_t>(-1)) {
        return std::nullopt;
    }
    return static_cast<std::int64_t>(utc_seconds - kUtcPlus8OffsetSeconds) * 1000000;
}

std::optional<std::pair<std::int64_t, std::int64_t>> ParseTimeRangeFromFileName(
    const std::filesystem::path& path) {
    std::string name = path.filename().string();
    while (true) {
        const auto dot_pos = name.rfind('.');
        if (dot_pos == std::string::npos) {
            break;
        }
        name.resize(dot_pos);
    }
    const auto delimiter = name.find('-');
    if (delimiter == std::string::npos) {
        return std::nullopt;
    }

    const auto start = ParseUtcPlus8Timestamp(name.substr(0, delimiter));
    if (!start.has_value()) {
        return std::nullopt;
    }
    const auto end_text = name.substr(delimiter + 1);
    if (end_text.empty()) {
        return std::make_pair(*start, std::numeric_limits<std::int64_t>::max());
    }
    const auto end = ParseUtcPlus8Timestamp(end_text);
    if (!end.has_value()) {
        return std::nullopt;
    }
    return std::make_pair(*start, *end);
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
        const auto time_range = ParseTimeRangeFromFileName(path);
        if (!time_range.has_value()) {
            continue;
        }
        group.start_time_us = time_range->first;
        group.end_time_us = time_range->second;
        groups->push_back(std::move(group));
    }
}

bool OverlapsRange(const SegmentFileGroup& group,
                   int64_t start_time_us,
                   int64_t end_time_us) {
    return group.start_time_us <= end_time_us && group.end_time_us >= start_time_us;
}

std::vector<SegmentFileGroup> SelectPackageGroups(
    const std::vector<SegmentFileGroup>& groups,
    const L2PackageOptions& options) {
    std::vector<SegmentFileGroup> selected_groups;
    for (const auto& group : groups) {
        if (OverlapsRange(group, options.start_time_us, options.end_time_us)) {
            selected_groups.push_back(group);
        }
    }
    return selected_groups;
}

void RemoveActiveGroups(const LogService& service,
                        std::vector<SegmentFileGroup>* groups) {
    if (groups == nullptr) {
        return;
    }
    groups->erase(
        std::remove_if(groups->begin(), groups->end(), [&](const SegmentFileGroup& group) {
            return service.IsActiveFilePath(group.data_path) ||
                   service.IsActiveFilePath(group.index_path);
        }),
        groups->end());
}

bool IsCurrentRecorderRoot(const std::filesystem::path& root_dir) {
    auto& state = State();
    if (!state.initialized) {
        return false;
    }
    return std::filesystem::path(state.options.root_dir).lexically_normal() ==
           root_dir.lexically_normal();
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
    state.initialized = false;
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
    const auto replay_root = BuildReplaySessionRoot();
    if (!g_recovery_checked_once) {
        RecoverReplayFilesBeforeStart(replay_root);
        g_recovery_checked_once = true;
    }
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
    state.initialized = true;
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
        FinalizeTopicStorage(&item.second, false);
    }
    if (state.session_end_message_time_us > 0) {
        RefreshSessionMetaFile(state.session_end_message_time_us, true);
    }
    state.initialized = false;
}

void PackageRecords(const std::string& bundle_dir) {
    auto& state = State();
    FlushAll();
    for (auto& item : state.storage_buckets) {
        FinalizeTopicStorage(&item.second, false);
    }
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
    LogService service(root_dir);
    std::vector<SegmentFileGroup> groups;
    CollectSegmentGroups(root_dir, &groups);

    L2PackageOptions effective_options = options;
    auto selected_groups = SelectPackageGroups(groups, effective_options);
    std::vector<std::filesystem::path> selected_paths;
    selected_paths.reserve(selected_groups.size() * 2);
    for (const auto& group : selected_groups) {
        selected_paths.push_back(group.data_path);
        selected_paths.push_back(group.index_path);
    }

    auto preparation = service.PrepareFilesForPackaging(selected_paths);
    if (preparation.has_active_files) {
        if (!IsCurrentRecorderRoot(root_dir)) {
            throw std::runtime_error(
                "matched active L2 segments owned by another running recorder; "
                "cannot safely package them");
        }
        effective_options.end_time_us =
            std::min<std::int64_t>(effective_options.end_time_us, NowTimestampUs());
        ForceRotateActiveSegments();
        groups.clear();
        CollectSegmentGroups(root_dir, &groups);
        selected_groups = SelectPackageGroups(groups, effective_options);
        RemoveActiveGroups(service, &selected_groups);
        selected_paths.clear();
        selected_paths.reserve(selected_groups.size() * 2);
        for (const auto& group : selected_groups) {
            selected_paths.push_back(group.data_path);
            selected_paths.push_back(group.index_path);
        }
        preparation = service.PrepareFilesForPackaging(selected_paths);
    }
    if (!preparation.success) {
        throw std::runtime_error(preparation.message);
    }

    if (selected_groups.empty()) {
        throw std::runtime_error("no L2 segments matched the requested time range");
    }

    std::sort(selected_groups.begin(), selected_groups.end(),
              [](const SegmentFileGroup& left, const SegmentFileGroup& right) {
                  return left.start_time_us < right.start_time_us;
              });
    for (std::size_t index = 0; index < selected_groups.size(); ++index) {
        selected_groups[index].data_path = preparation.prepared_files[index * 2];
        selected_groups[index].index_path = preparation.prepared_files[index * 2 + 1];
        selected_groups[index].relative_data_path =
            std::filesystem::relative(selected_groups[index].data_path, root_dir);
        selected_groups[index].relative_index_path =
            std::filesystem::relative(selected_groups[index].index_path, root_dir);
    }

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

    WritePackageMeta(bundle_root, effective_options, selected_groups);

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
    FlushAll();
    for (auto& item : state.storage_buckets) {
        FinalizeTopicStorage(&item.second, false);
    }
    if (state.session_end_message_time_us > 0) {
        RefreshSessionMetaFile(state.session_end_message_time_us, true);
    }
    state.initialized = false;
}

}  // namespace naviai::log::l2_log
