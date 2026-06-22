#include "log_agent/include/log_agent.hpp"
#include "log_l2/include/business_data.hpp"
#include "log_l2/include/operation.hpp"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>

namespace {

struct FileCounts {
    std::size_t data_files{0};
    std::size_t index_files{0};
    std::size_t gzip_files{0};
};

FileCounts CountFiles(const std::filesystem::path& root_dir) {
    FileCounts counts;
    std::error_code ec;
    if (!std::filesystem::exists(root_dir, ec)) {
        return counts;
    }
    for (const auto& entry :
         std::filesystem::recursive_directory_iterator(root_dir, ec)) {
        if (ec || !entry.is_regular_file()) {
            continue;
        }
        const auto path = entry.path();
        const auto name = path.filename().string();
        if (name.find(".data") != std::string::npos) {
            ++counts.data_files;
        }
        if (name.find(".idx") != std::string::npos) {
            ++counts.index_files;
        }
        if (path.extension() == ".gz") {
            ++counts.gzip_files;
        }
    }
    return counts;
}

naviai::log::L2TopicDescriptor BuildValidationTopic() {
    naviai::log::L2TopicDescriptor descriptor;
    descriptor.topic = "/validation/topic";
    descriptor.topic_type = "validation/Topic";
    descriptor.source_module = "validator";
    descriptor.source_type = "runtime";
    descriptor.segment_size_bytes = 1024;
    return descriptor;
}

naviai::log::L2TopicMessage BuildMessage(int sequence) {
    naviai::log::L2TopicMessage message;
    message.topic = "/validation/topic";
    message.sequence = sequence;
    message.message_time_us = 1761000000000000LL + sequence * 1000LL;
    message.task_id = "validation-task";
    message.frame_id = "map";
    message.action_state = "Running";
    message.payload = std::string(512, static_cast<char>('A' + (sequence % 26)));
    message.replay_payload = message.payload;
    message.payload_summary = {{"sequence", std::to_string(sequence)}};
    return message;
}

bool ArchiveContainsOnlyCompressedSegments(const std::filesystem::path& archive_path) {
    const std::string command =
        "tar -tf \"" + archive_path.string() + "\"";
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

}  // namespace

int main(int argc, char** argv) {
    const std::filesystem::path root_dir =
        argc > 1 ? std::filesystem::path(argv[1])
                 : std::filesystem::path("/tmp/l2_runtime_validation");
    std::error_code ec;
    std::filesystem::remove_all(root_dir, ec);
    std::filesystem::create_directories(root_dir, ec);

    naviai::log::FileGovernPolicy policy;
    policy.scan_interval_ms = 100;
    policy.cleanup_interval_ms = 60 * 1000;
    naviai::log::LogAgent agent(root_dir, policy);
    const auto start_result = agent.Start();
    if (!start_result.success) {
        std::cerr << "failed to start agent: " << start_result.message << '\n';
        return 1;
    }

    naviai::log::L2RecorderOptions options;
    options.root_dir = root_dir.string();
    options.session_id = "validation-session";
    options.topics = {BuildValidationTopic()};
    naviai::log::l2_log::InitRecorder(options);
    naviai::log::l2_log::StartTask("validation-task");

    for (int index = 0; index < 12; ++index) {
        naviai::log::l2_log::RecordBusinessTopic(BuildMessage(index));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(800));
    const auto state_after_runtime = agent.GetState();
    const auto counts_after_runtime = CountFiles(root_dir);

    std::cout << "runtime_compressed_files=" << state_after_runtime.stats.compressed_files
              << '\n';
    std::cout << "runtime_gzip_files=" << counts_after_runtime.gzip_files << '\n';

    naviai::log::L2PackageOptions package_options;
    package_options.root_dir = root_dir.string();
    const auto now_us = std::chrono::duration_cast<std::chrono::microseconds>(
                            std::chrono::system_clock::now().time_since_epoch())
                            .count();
    package_options.start_time_us = now_us - 5LL * 1000000LL;
    package_options.end_time_us = now_us + 5LL * 1000000LL;
    package_options.output_path = (root_dir / "validation_package.tar.xz").string();
    const auto archive_path = naviai::log::l2_log::PackageRecords(package_options);
    const bool archive_valid =
        ArchiveContainsOnlyCompressedSegments(std::filesystem::path(archive_path));

    std::cout << "package_path=" << archive_path << '\n';
    std::cout << "package_only_gz=" << (archive_valid ? 1 : 0) << '\n';

    naviai::log::l2_log::ShutdownAll();
    agent.DrainNow();
    agent.Stop(true);
    return (state_after_runtime.stats.compressed_files > 0 && archive_valid) ? 0 : 2;
}
