#include "tc_support.hpp"

#include "log_l2/include/business_data.hpp"
#include "log_l2/include/large_data.hpp"
#include "log_l2/include/operation.hpp"
#include "log_l2/include/static_data.hpp"

#include <zlib.h>

#include <chrono>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <thread>

namespace naviai::log::test {

namespace {

bool ContainsSequenceField(const std::string& field) {
    return field.find("\"sequence\"") != std::string::npos;
}

void ParseIndexLine(const std::string& line, Inspection* inspection) {
    if (inspection == nullptr || line.empty()) {
        return;
    }
    ++inspection->idx_records;
    if (!ContainsSequenceField(line)) {
        return;
    }

    const std::string key = "\"sequence\":\"";
    auto pos = line.find(key);
    if (pos == std::string::npos) {
        return;
    }
    pos += key.size();
    const auto end = line.find('"', pos);
    if (end == std::string::npos) {
        return;
    }
    try {
        inspection->sequences.insert(std::stoll(line.substr(pos, end - pos)));
    } catch (...) {
    }
}

void ReadIndexFile(const std::filesystem::path& path, Inspection* inspection) {
    std::ifstream input(path);
    std::string line;
    while (std::getline(input, line)) {
        ParseIndexLine(line, inspection);
    }
}

void ReadGzipIndexFile(const std::filesystem::path& path, Inspection* inspection) {
    gzFile input = gzopen(path.string().c_str(), "rb");
    if (input == nullptr) {
        return;
    }
    char buffer[8192];
    while (gzgets(input, buffer, static_cast<int>(sizeof(buffer))) != nullptr) {
        std::string line(buffer);
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
            line.pop_back();
        }
        ParseIndexLine(line, inspection);
    }
    gzclose(input);
}

}  // namespace

std::filesystem::path DefaultRootForCase(int case_id) {
    std::ostringstream oss;
    oss << "/tmp/tc";
    if (case_id < 10) {
        oss << '0';
    }
    oss << case_id << "_test";
    return std::filesystem::path(oss.str());
}

std::string ReadFile(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    std::ostringstream oss;
    oss << input.rdbuf();
    return oss.str();
}

Inspection InspectRoot(const std::filesystem::path& root_dir) {
    Inspection inspection;
    LogService service(root_dir);
    std::error_code ec;
    if (!std::filesystem::exists(root_dir, ec)) {
        return inspection;
    }

    for (const auto& entry :
         std::filesystem::recursive_directory_iterator(root_dir, ec)) {
        if (ec || !entry.is_regular_file()) {
            continue;
        }
        const auto path = entry.path();
        const auto text = path.generic_string();
        if (service.IsActiveFilePath(path)) {
            ++inspection.counts.active_files;
        }
        if (text.find("/business_data/") != std::string::npos) {
            ++inspection.counts.business_files;
        } else if (text.find("/static_data/") != std::string::npos) {
            ++inspection.counts.static_files;
        } else if (text.find("/large_data/") != std::string::npos) {
            ++inspection.counts.large_files;
        }
        if (path.extension() == ".gz") {
            if (text.find(".data") != std::string::npos) {
                ++inspection.counts.gz_data;
            }
            if (text.find(".idx") != std::string::npos) {
                ++inspection.counts.gz_idx;
                ReadGzipIndexFile(path, &inspection);
            }
            continue;
        }
        if (text.find(".data") != std::string::npos) {
            ++inspection.counts.raw_data;
        }
        if (text.find(".idx") != std::string::npos) {
            ++inspection.counts.raw_idx;
            ReadIndexFile(path, &inspection);
        }
    }
    return inspection;
}

L2TopicDescriptor BuildTopicDescriptor(const std::string& topic,
                                       const std::string& type,
                                       const std::string& module,
                                       const std::string& source_type,
                                       std::uint64_t segment_size_bytes) {
    L2TopicDescriptor descriptor;
    descriptor.topic = topic;
    descriptor.topic_type = type;
    descriptor.source_module = module;
    descriptor.source_type = source_type;
    descriptor.segment_size_bytes = segment_size_bytes;
    descriptor.target_compressed_segment_size_bytes = segment_size_bytes;
    return descriptor;
}

L2TopicMessage BuildMessage(const std::string& topic,
                            std::int64_t sequence,
                            int payload_size) {
    L2TopicMessage message;
    message.topic = topic;
    message.sequence = sequence;
    message.message_time_us = kBaseTimeUs + sequence * 1000LL;
    message.task_id = "validation-task";
    message.frame_id = "map";
    message.action_phase = "topic";
    message.action_state = "Running";
    message.payload = std::string(payload_size,
                                  static_cast<char>('A' + (sequence % 26)));
    message.replay_payload = message.payload;
    message.payload_summary = {{"sequence", std::to_string(sequence)}};
    return message;
}

void RemoveRoot(const std::filesystem::path& root_dir) {
    std::error_code ec;
    std::filesystem::remove_all(root_dir, ec);
    std::filesystem::create_directories(root_dir, ec);
}

std::vector<L2TopicDescriptor> BuildStandardTopics(std::uint64_t segment_size_bytes,
                                                   bool include_idle) {
    std::vector<L2TopicDescriptor> topics;
    topics.push_back(BuildTopicDescriptor(
        kBusinessTopic, "validation/Business", "validator", "business",
        segment_size_bytes));
    topics.push_back(BuildTopicDescriptor(
        kStaticTopic, "validation/StaticMap", "validator", "static_map",
        segment_size_bytes));
    topics.push_back(BuildTopicDescriptor(
        kLargeTopic, "validation/LargeMap", "validator", "local_map",
        segment_size_bytes));
    if (include_idle) {
        topics.push_back(BuildTopicDescriptor(
            kIdleTopic, "validation/Idle", "validator", "business",
            segment_size_bytes));
    }
    return topics;
}

void RecordStandardMessages(const ScenarioConfig& config) {
    std::int64_t sequence = 1;
    for (int index = 0; index < config.business_messages; ++index) {
        l2_log::RecordBusinessTopic(
            BuildMessage(kBusinessTopic, sequence++, config.payload_size));
    }
    for (int index = 0; index < config.static_messages; ++index) {
        l2_log::RecordStaticDataTopic(
            BuildMessage(kStaticTopic, sequence++, config.payload_size));
    }
    for (int index = 0; index < config.large_messages; ++index) {
        l2_log::RecordLargeDataTopic(
            BuildMessage(kLargeTopic, sequence++, config.payload_size));
    }
}

ScenarioRun RunScenario(const ScenarioConfig& config) {
    RemoveRoot(config.root_dir);

    ScenarioRun run;
    if (config.enable_agent) {
        FileGovernPolicy policy;
        policy.scan_interval_ms = config.agent_scan_interval_ms;
        policy.cleanup_interval_ms = 60 * 1000;
        run.agent = std::make_unique<LogAgent>(config.root_dir, policy);
        run.start_result = run.agent->Start();
    }

    L2RecorderOptions options;
    options.root_dir = config.root_dir.string();
    options.session_id = "validation-session";
    options.topics =
        BuildStandardTopics(config.segment_size_bytes, config.register_idle_topic);
    l2_log::InitRecorder(options);
    l2_log::StartTask("validation-task");
    RecordStandardMessages(config);

    if (config.post_record_sleep_ms > 0) {
        std::this_thread::sleep_for(
            std::chrono::milliseconds(config.post_record_sleep_ms));
    }

    if (config.shutdown_all) {
        l2_log::FlushAll();
        l2_log::ShutdownAll();
        if (run.agent != nullptr) {
            run.agent->DrainNow();
            run.agent->Stop(true);
        }
    } else {
        l2_log::FlushAll();
    }

    run.inspection = InspectRoot(config.root_dir);
    return run;
}

std::vector<std::filesystem::path> CollectActivePaths(
    const std::filesystem::path& root_dir) {
    std::vector<std::filesystem::path> paths;
    LogService service(root_dir);
    std::error_code ec;
    if (!std::filesystem::exists(root_dir, ec)) {
        return paths;
    }
    for (const auto& entry :
         std::filesystem::recursive_directory_iterator(root_dir, ec)) {
        if (ec || !entry.is_regular_file()) {
            continue;
        }
        if (service.IsActiveFilePath(entry.path())) {
            paths.push_back(entry.path());
        }
    }
    return paths;
}

bool RecoverActiveWithService(const std::filesystem::path& root_dir,
                              std::int64_t end_time_us,
                              RecoveryTask* task) {
    LogService service(root_dir);
    const auto active_paths = CollectActivePaths(root_dir);
    if (active_paths.empty()) {
        return false;
    }
    const auto result = service.RecoverActiveFiles(active_paths, end_time_us);
    if (task != nullptr) {
        *task = result;
    }
    return result.success;
}

bool ArchiveContainsOnlyCompressedSegments(const std::filesystem::path& archive_path) {
    const std::string command = "tar -tf \"" + archive_path.string() + "\"";
    FILE* pipe = popen(command.c_str(), "r");
    if (pipe == nullptr) {
        return false;
    }
    char buffer[4096];
    bool saw_segment = false;
    bool valid = true;
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        std::string line(buffer);
        if (!line.empty() && line.back() == '\n') {
            line.pop_back();
        }
        if (line.find(".data") != std::string::npos ||
            line.find(".idx") != std::string::npos) {
            saw_segment = true;
            if (line.size() < 3 || line.substr(line.size() - 3) != ".gz") {
                valid = false;
                break;
            }
        }
    }
    pclose(pipe);
    return saw_segment && valid;
}

bool AnyFileNameContains(const std::filesystem::path& root_dir,
                         const std::string& token) {
    std::error_code ec;
    if (!std::filesystem::exists(root_dir, ec)) {
        return false;
    }
    for (const auto& entry :
         std::filesystem::recursive_directory_iterator(root_dir, ec)) {
        if (ec || !entry.is_regular_file()) {
            continue;
        }
        if (entry.path().filename().string().find(token) != std::string::npos) {
            return true;
        }
    }
    return false;
}

int ReportResult(int case_id, bool passed, const std::string& summary,
                 const Inspection& inspection) {
    std::cout << "tc="
              << (case_id < 10 ? "0" : "") << case_id
              << " result=" << (passed ? "pass" : "fail") << '\n';
    std::cout << summary << '\n';
    std::cout << "raw_data=" << inspection.counts.raw_data
              << " raw_idx=" << inspection.counts.raw_idx
              << " gz_data=" << inspection.counts.gz_data
              << " gz_idx=" << inspection.counts.gz_idx
              << " active_files=" << inspection.counts.active_files
              << " unique_sequences=" << inspection.sequences.size()
              << " idx_records=" << inspection.idx_records << '\n';
    return passed ? 0 : 1;
}

}  // namespace naviai::log::test
