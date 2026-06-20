#include "navigation_action.hpp"

#include <cmath>
#include <iomanip>
#include <sstream>

namespace naviai::log {

namespace {

std::string ToString(double value) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(6) << value;
    return oss.str();
}

std::string ToString(bool value) {
    return value ? "true" : "false";
}

std::string FirstCode(const std::vector<L2NavigationFault>& faults) {
    return faults.empty() ? "0" : std::to_string(faults.front().code);
}

std::string FirstMessage(const std::vector<L2NavigationFault>& faults) {
    return faults.empty() ? "" : faults.front().message;
}

std::string FirstCode(const std::vector<L2NavigationCause>& causes) {
    return causes.empty() ? "0" : std::to_string(causes.front().code);
}

std::string FirstMessage(const std::vector<L2NavigationCause>& causes) {
    return causes.empty() ? "" : causes.front().message;
}

}  // namespace

std::string GenerateNavigationTaskId(const L2NavigationGoalSummaryInput& input,
                                     int64_t local_sequence) {
    const int64_t stamp_us =
        input.header_stamp_us > 0 ? input.header_stamp_us : input.header_stamp_ns / 1000;
    std::ostringstream oss;
    oss << (input.task_type_name.empty() ? "navigation" : input.task_type_name) << '_'
        << stamp_us << '_' << local_sequence;
    return oss.str();
}

std::map<std::string, std::string> BuildNavigationGoalSummary(
    const L2NavigationGoalSummaryInput& input) {
    std::map<std::string, std::string> summary;
    summary["task_type"] = std::to_string(input.task_type);
    summary["task_type_name"] = input.task_type_name;
    summary["waypoint_count"] = std::to_string(input.waypoints.size());
    if (!input.waypoints.empty()) {
        const auto& target = input.waypoints.back();
        summary["target_x"] = ToString(target.x);
        summary["target_y"] = ToString(target.y);
        summary["target_yaw"] = ToString(target.yaw);
    }
    summary["distance_tolerance"] = ToString(input.distance_tolerance);
    summary["heading_tolerance"] = ToString(input.heading_tolerance);
    summary["translation_enable"] = ToString(input.translation_enable);
    summary["translation_heading"] = ToString(input.translation_heading);
    return summary;
}

std::map<std::string, std::string> BuildNavigationFeedbackSummary(
    const L2NavigationFeedbackSummaryInput& input) {
    std::map<std::string, std::string> summary;
    summary["state"] = std::to_string(input.state);
    summary["state_name"] = input.state_name;
    summary["fault_count"] = std::to_string(input.faults.size());
    summary["primary_fault_code"] = FirstCode(input.faults);
    summary["primary_fault_msg"] = FirstMessage(input.faults);
    return summary;
}

std::map<std::string, std::string> BuildNavigationResultSummary(
    const L2NavigationResultSummaryInput& input) {
    std::map<std::string, std::string> summary;
    summary["state"] = std::to_string(input.state);
    summary["state_name"] = input.state_name;
    summary["duration_min"] = ToString(input.duration_min);
    summary["distance_deviation"] = ToString(input.distance_deviation);
    summary["heading_deviation"] = ToString(input.heading_deviation);
    summary["cause_count"] = std::to_string(input.causes.size());
    summary["primary_cause_code"] = FirstCode(input.causes);
    summary["primary_cause_msg"] = FirstMessage(input.causes);
    return summary;
}

L2RosRecordMetadata BuildNavigationGoalMetadata(
    const std::string& task_id,
    const L2NavigationGoalSummaryInput& input,
    const L2RosRecordMetadata& base) {
    L2RosRecordMetadata metadata = base;
    metadata.task_id = task_id;
    if (metadata.message_time_us <= 0 && metadata.message_time_ns <= 0) {
        metadata.message_time_us = input.header_stamp_us;
        metadata.message_time_ns = input.header_stamp_ns;
    }
    metadata.action_phase = "goal";
    metadata.action_state = "Active";
    metadata.payload_summary = BuildNavigationGoalSummary(input);
    return metadata;
}

L2RosRecordMetadata BuildNavigationFeedbackMetadata(
    const std::string& task_id,
    const L2NavigationFeedbackSummaryInput& input,
    const L2RosRecordMetadata& base) {
    L2RosRecordMetadata metadata = base;
    metadata.task_id = task_id;
    metadata.action_phase = "feedback";
    metadata.action_state = input.state_name;
    metadata.payload_summary = BuildNavigationFeedbackSummary(input);
    return metadata;
}

L2RosRecordMetadata BuildNavigationResultMetadata(
    const std::string& task_id,
    const L2NavigationResultSummaryInput& input,
    const L2RosRecordMetadata& base) {
    L2RosRecordMetadata metadata = base;
    metadata.task_id = task_id;
    metadata.action_phase = "result";
    metadata.action_state = input.state_name;
    metadata.payload_summary = BuildNavigationResultSummary(input);
    return metadata;
}

}  // namespace naviai::log
