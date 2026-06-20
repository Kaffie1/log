#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "ros_topic.hpp"

namespace naviai::log {

struct L2NavigationWaypoint {
    double x{0.0};
    double y{0.0};
    double yaw{0.0};
};

struct L2NavigationFault {
    int code{0};
    std::string message;
};

struct L2NavigationCause {
    int code{0};
    std::string message;
};

struct L2NavigationGoalSummaryInput {
    int task_type{0};
    std::string task_type_name;
    std::vector<L2NavigationWaypoint> waypoints;
    bool translation_enable{false};
    double translation_heading{0.0};
    double distance_tolerance{0.0};
    double heading_tolerance{0.0};
    int64_t header_stamp_us{0};
    int64_t header_stamp_ns{0};
};

struct L2NavigationFeedbackSummaryInput {
    int state{0};
    std::string state_name;
    std::vector<L2NavigationFault> faults;
};

struct L2NavigationResultSummaryInput {
    int state{0};
    std::string state_name;
    double duration_min{0.0};
    double distance_deviation{0.0};
    double heading_deviation{0.0};
    std::vector<L2NavigationCause> causes;
};

std::string GenerateNavigationTaskId(const L2NavigationGoalSummaryInput& input,
                                     int64_t local_sequence);
std::map<std::string, std::string> BuildNavigationGoalSummary(
    const L2NavigationGoalSummaryInput& input);
std::map<std::string, std::string> BuildNavigationFeedbackSummary(
    const L2NavigationFeedbackSummaryInput& input);
std::map<std::string, std::string> BuildNavigationResultSummary(
    const L2NavigationResultSummaryInput& input);

L2RosRecordMetadata BuildNavigationGoalMetadata(
    const std::string& task_id,
    const L2NavigationGoalSummaryInput& input,
    const L2RosRecordMetadata& base = {});
L2RosRecordMetadata BuildNavigationFeedbackMetadata(
    const std::string& task_id,
    const L2NavigationFeedbackSummaryInput& input,
    const L2RosRecordMetadata& base = {});
L2RosRecordMetadata BuildNavigationResultMetadata(
    const std::string& task_id,
    const L2NavigationResultSummaryInput& input,
    const L2RosRecordMetadata& base = {});

}  // namespace naviai::log
