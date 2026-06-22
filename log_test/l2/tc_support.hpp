#pragma once

#include "log_agent/include/log_agent.hpp"
#include "log_l2/include/business_data.hpp"
#include "log_l2/include/large_data.hpp"
#include "log_l2/include/operation.hpp"
#include "log_l2/include/static_data.hpp"
#include "log_l2/include/log_l2_types.hpp"
#include "log_service/include/log_service_naming.hpp"

#include <cstdint>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

namespace naviai::log::test {

inline constexpr char kBusinessTopic[] = "/validation/business";
inline constexpr char kIdleTopic[] = "/validation/idle";
inline constexpr char kStaticTopic[] = "/zj_humanoid/navigation/map";
inline constexpr char kLargeTopic[] = "/zj_humanoid/navigation/local_map";
inline constexpr std::int64_t kBaseTimeUs = 1760918400000000LL;

struct Counts {
    std::size_t raw_data{0};
    std::size_t raw_idx{0};
    std::size_t gz_data{0};
    std::size_t gz_idx{0};
    std::size_t active_files{0};
    std::size_t business_files{0};
    std::size_t static_files{0};
    std::size_t large_files{0};
};

struct Inspection {
    Counts counts;
    std::set<std::int64_t> sequences;
    std::size_t idx_records{0};
};

struct ScenarioConfig {
    std::filesystem::path root_dir;
    std::uint64_t segment_size_bytes{1024 * 1024};
    int business_messages{0};
    int static_messages{0};
    int large_messages{0};
    int payload_size{512};
    bool register_idle_topic{true};
    bool enable_agent{false};
    std::int64_t agent_scan_interval_ms{100};
    int post_record_sleep_ms{0};
    bool shutdown_all{true};
};

struct ScenarioRun {
    Inspection inspection;
    std::unique_ptr<LogAgent> agent;
    LogAgentResult start_result;
};

std::filesystem::path DefaultRootForCase(int case_id);
std::string ReadFile(const std::filesystem::path& path);
Inspection InspectRoot(const std::filesystem::path& root_dir);
L2TopicDescriptor BuildTopicDescriptor(const std::string& topic,
                                       const std::string& type,
                                       const std::string& module,
                                       const std::string& source_type,
                                       std::uint64_t segment_size_bytes);
L2TopicMessage BuildMessage(const std::string& topic,
                            std::int64_t sequence,
                            int payload_size);
void RemoveRoot(const std::filesystem::path& root_dir);
std::vector<L2TopicDescriptor> BuildStandardTopics(std::uint64_t segment_size_bytes,
                                                   bool include_idle);
void RecordStandardMessages(const ScenarioConfig& config);
ScenarioRun RunScenario(const ScenarioConfig& config);
std::vector<std::filesystem::path> CollectActivePaths(
    const std::filesystem::path& root_dir);
bool RecoverActiveWithService(const std::filesystem::path& root_dir,
                              std::int64_t end_time_us,
                              RecoveryTask* task = nullptr);
bool ArchiveContainsOnlyCompressedSegments(const std::filesystem::path& archive_path);
bool AnyFileNameContains(const std::filesystem::path& root_dir,
                         const std::string& token);
int ReportResult(int case_id, bool passed, const std::string& summary,
                 const Inspection& inspection);

}  // namespace naviai::log::test
