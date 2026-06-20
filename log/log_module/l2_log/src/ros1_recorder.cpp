#include "ros1_recorder.hpp"

#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <ros/ros.h>
#include <ros/serialization.h>

#include "../include/log.hpp"
#include "ros_common.hpp"

namespace naviai::log_module {

namespace {

L2SampleMode ParseSampleMode(const std::string& value) {
    if (value == "full") {
        return L2SampleMode::Full;
    }
    if (value == "low" || value == "low_frequency" || value == "1hz") {
        return L2SampleMode::LowFrequency;
    }
    return L2SampleMode::Normal;
}

using namespace l2_ros;

template <typename MsgT>
std::string SerializeMessage(const MsgT& message) {
    const auto size = ros::serialization::serializationLength(message);
    std::string buffer(size, '\0');
    ros::serialization::OStream stream(
        reinterpret_cast<uint8_t*>(buffer.data()), size);
    ros::serialization::serialize(stream, message);
    return buffer;
}

template <typename MsgT, typename = void>
struct HeaderAccessor {
    static int64_t StampNs(const MsgT&) { return 0; }
    static std::string FrameId(const MsgT&) { return {}; }
    static int64_t Sequence(const MsgT&) { return 0; }
};

template <typename MsgT>
struct HeaderAccessor<MsgT, std::void_t<decltype(std::declval<MsgT>().header)>> {
    static int64_t StampNs(const MsgT& msg) {
        return static_cast<int64_t>(msg.header.stamp.sec) * 1000000000LL +
               static_cast<int64_t>(msg.header.stamp.nsec);
    }
    static std::string FrameId(const MsgT& msg) { return msg.header.frame_id; }
    static int64_t Sequence(const MsgT& msg) {
        return static_cast<int64_t>(msg.header.seq);
    }
};

double QuaternionToYaw(double x, double y, double z, double w) {
    const double siny_cosp = 2.0 * (w * z + x * y);
    const double cosy_cosp = 1.0 - 2.0 * (y * y + z * z);
    return std::atan2(siny_cosp, cosy_cosp);
}

std::string MotionStateFromTwist(const CmdVelMsg& msg) {
    if (std::fabs(msg.linear.x) < 1e-6 && std::fabs(msg.linear.y) < 1e-6 &&
        std::fabs(msg.angular.z) < 1e-6) {
        return "stop";
    }
    if (msg.angular.z > 1e-3) {
        return "turn_left";
    }
    if (msg.angular.z < -1e-3) {
        return "turn_right";
    }
    if (msg.linear.x >= 0.0) {
        return "forward";
    }
    return "backward";
}

template <typename T, typename = void>
struct HasValueField : std::false_type {};

template <typename T>
struct HasValueField<T, std::void_t<decltype(std::declval<T>().value)>>
    : std::true_type {};

template <typename T>
int ExtractValue(const T& field) {
    if constexpr (HasValueField<T>::value) {
        return static_cast<int>(field.value);
    } else {
        return static_cast<int>(field);
    }
}

template <typename T, typename = void>
struct HasEnableField : std::false_type {};

template <typename T>
struct HasEnableField<T, std::void_t<decltype(std::declval<T>().enable)>>
    : std::true_type {};

template <typename T, typename = void>
struct HasHeadingField : std::false_type {};

template <typename T>
struct HasHeadingField<T, std::void_t<decltype(std::declval<T>().heading)>>
    : std::true_type {};

template <typename T>
bool ExtractTranslationEnable(const T& value) {
    if constexpr (HasEnableField<T>::value) {
        return static_cast<bool>(value.enable);
    } else {
        return false;
    }
}

template <typename T>
double ExtractTranslationHeading(const T& value) {
    if constexpr (HasHeadingField<T>::value) {
        return static_cast<double>(value.heading);
    } else {
        return 0.0;
    }
}

template <typename T, typename = void>
struct HasTaskTypeNameField : std::false_type {};

template <typename T>
struct HasTaskTypeNameField<T, std::void_t<decltype(std::declval<T>().task_type_name)>>
    : std::true_type {};

template <typename T>
std::string ExtractTaskTypeName(const T& value) {
    if constexpr (HasTaskTypeNameField<T>::value) {
        return value.task_type_name;
    } else {
        return {};
    }
}

template <typename T, typename = void>
struct HasDistanceToleranceField : std::false_type {};

template <typename T>
struct HasDistanceToleranceField<T,
                                 std::void_t<decltype(std::declval<T>().distance_tolerance)>>
    : std::true_type {};

template <typename T>
double ExtractDistanceTolerance(const T& value) {
    if constexpr (HasDistanceToleranceField<T>::value) {
        return static_cast<double>(value.distance_tolerance);
    } else {
        return 0.0;
    }
}

template <typename T, typename = void>
struct HasHeadingToleranceField : std::false_type {};

template <typename T>
struct HasHeadingToleranceField<T,
                                std::void_t<decltype(std::declval<T>().heading_tolerance)>>
    : std::true_type {};

template <typename T>
double ExtractHeadingTolerance(const T& value) {
    if constexpr (HasHeadingToleranceField<T>::value) {
        return static_cast<double>(value.heading_tolerance);
    } else {
        return 0.0;
    }
}

template <typename T>
double DurationToMinutes(const T& value) {
    if constexpr (std::is_arithmetic_v<T>) {
        return static_cast<double>(value) / 60.0;
    } else {
        return value.toSec() / 60.0;
    }
}

std::string FirstErrorCode(const module_common_msgs::ModuleStatus& msg) {
    return msg.error_info.empty() ? "0" : std::to_string(msg.error_info.front().code);
}

std::string FirstErrorMessage(const module_common_msgs::ModuleStatus& msg) {
    return msg.error_info.empty() ? "" : msg.error_info.front().message;
}

std::string NavigationStatusName(int status) {
    switch (status) {
        case 0:
            return "IDLE";
        case 1:
            return "RUNNING";
        case 2:
            return "PAUSED";
        case 3:
            return "COMPLETED";
        case 4:
            return "ERROR";
        default:
            return std::to_string(status);
    }
}

template <typename WaypointT, typename = void>
struct WaypointAccessor {
    static double X(const WaypointT&) { return 0.0; }
    static double Y(const WaypointT&) { return 0.0; }
    static double Yaw(const WaypointT&) { return 0.0; }
};

template <typename WaypointT>
struct WaypointAccessor<
    WaypointT,
    std::void_t<decltype(std::declval<WaypointT>().x),
                decltype(std::declval<WaypointT>().y),
                decltype(std::declval<WaypointT>().yaw)>> {
    static double X(const WaypointT& value) { return value.x; }
    static double Y(const WaypointT& value) { return value.y; }
    static double Yaw(const WaypointT& value) { return value.yaw; }
};

template <typename WaypointT>
struct WaypointAccessor<
    WaypointT,
    std::void_t<decltype(std::declval<WaypointT>().pose.position.x),
                decltype(std::declval<WaypointT>().pose.position.y),
                decltype(std::declval<WaypointT>().pose.orientation.x),
                decltype(std::declval<WaypointT>().pose.orientation.y),
                decltype(std::declval<WaypointT>().pose.orientation.z),
                decltype(std::declval<WaypointT>().pose.orientation.w)>> {
    static double X(const WaypointT& value) { return value.pose.position.x; }
    static double Y(const WaypointT& value) { return value.pose.position.y; }
    static double Yaw(const WaypointT& value) {
        return QuaternionToYaw(value.pose.orientation.x, value.pose.orientation.y,
                               value.pose.orientation.z, value.pose.orientation.w);
    }
};

template <typename WaypointT>
struct WaypointAccessor<
    WaypointT,
    std::void_t<decltype(std::declval<WaypointT>().position.x),
                decltype(std::declval<WaypointT>().position.y),
                decltype(std::declval<WaypointT>().heading)>> {
    static double X(const WaypointT& value) { return value.position.x; }
    static double Y(const WaypointT& value) { return value.position.y; }
    static double Yaw(const WaypointT& value) { return value.heading; }
};

std::string NavigationStateName(int state) {
    switch (state) {
        case 0:
            return "Idle";
        case 1:
            return "Active";
        case 2:
            return "Running";
        case 3:
            return "Arrived";
        case 4:
            return "Canceling";
        case 5:
            return "Cancelled";
        case 6:
            return "Succeeded";
        case 7:
            return "Failed";
        case 8:
            return "Error";
        case 9:
            return "Aborted";
        default:
            return "Unknown";
    }
}

}  // namespace

struct L2Ros1Recorder::Impl {
    ros::NodeHandle* nh{nullptr};
    ros::NodeHandle* pnh{nullptr};
    ros::Timer sample_mode_timer;
    std::vector<ros::Subscriber> subscribers;
    std::string root_dir{"/var/log/robot/l2"};
    std::string session_id;
    std::string sample_mode{"normal"};
    bool package_on_shutdown{false};
    std::string package_dir;
    int64_t task_sequence{0};
    std::string current_task_id;

    void InitRecorder() {
        auto default_topics = DefaultReplayRosTopicInput();
        L2RecorderOptions options;
        options.root_dir = root_dir;
        options.session_id = session_id;
        options.sample_mode = ParseSampleMode(sample_mode);
        options.topics = ToL2TopicDescriptors(default_topics.Topics());
        L2::InitRecorder(options);
    }

    void RefreshSampleMode() {
        if (pnh == nullptr) {
            return;
        }

        std::string latest_mode = sample_mode;
        pnh->param<std::string>("sample_mode", latest_mode, latest_mode);
        if (latest_mode == sample_mode) {
            return;
        }

        sample_mode = latest_mode;
        L2::SetSampleMode(ParseSampleMode(sample_mode));
        ROS_INFO_STREAM("L2 recorder sample_mode updated to " << sample_mode);
    }

    template <typename MsgT>
    L2RosRecordMetadata BuildMetadata(const MsgT& msg) const {
        L2RosRecordMetadata metadata;
        metadata.message_time_ns = HeaderAccessor<MsgT>::StampNs(msg);
        metadata.frame_id = HeaderAccessor<MsgT>::FrameId(msg);
        metadata.sequence = HeaderAccessor<MsgT>::Sequence(msg);
        return metadata;
    }

    void SubscribeTopics() {
        subscribers.push_back(nh->subscribe(
            kNavigationGoalTopic, 10, &Impl::OnNavigationGoal, this));
        subscribers.push_back(nh->subscribe(
            kNavigationFeedbackTopic, 20, &Impl::OnNavigationFeedback, this));
        subscribers.push_back(nh->subscribe(
            kNavigationResultTopic, 10, &Impl::OnNavigationResult, this));

        subscribers.push_back(
            nh->subscribe(kOdomInfoTopic, 400, &Impl::OnOdometry, this));
        subscribers.push_back(nh->subscribe(kMapTopic, 2, &Impl::OnMap, this));
        subscribers.push_back(
            nh->subscribe(kLocalMapTopic, 20, &Impl::OnLocalMap, this));
        subscribers.push_back(nh->subscribe(kLocationCodeTopic, 20,
                                            &Impl::OnLocationCode, this));
        subscribers.push_back(nh->subscribe(kPerceptionCodeTopic, 20,
                                            &Impl::OnPerceptionCode, this));
        subscribers.push_back(nh->subscribe(kNavigationCodeTopic, 20,
                                            &Impl::OnNavigationCode, this));
        subscribers.push_back(nh->subscribe(kChassisAgvStateTopic, 20,
                                            &Impl::OnChassisAgvState, this));
        subscribers.push_back(
            nh->subscribe(kCmdVelCalibTopic, 100, &Impl::OnCmdVel, this));
    }

    void OnOdometry(const OdomInfoMsg::ConstPtr& msg) {
        auto metadata = BuildMetadata(*msg);
        metadata.payload_summary = {
            {"x", std::to_string(msg->pose.pose.position.x)},
            {"y", std::to_string(msg->pose.pose.position.y)},
            {"z", std::to_string(msg->pose.pose.position.z)},
            {"qx", std::to_string(msg->pose.pose.orientation.x)},
            {"qy", std::to_string(msg->pose.pose.orientation.y)},
            {"qz", std::to_string(msg->pose.pose.orientation.z)},
            {"qw", std::to_string(msg->pose.pose.orientation.w)},
            {"yaw", std::to_string(QuaternionToYaw(
                        msg->pose.pose.orientation.x, msg->pose.pose.orientation.y,
                        msg->pose.pose.orientation.z,
                        msg->pose.pose.orientation.w))},
            {"linear_speed", std::to_string(msg->twist.twist.linear.x)},
            {"angular_speed", std::to_string(msg->twist.twist.angular.z)},
        };
        L2::RecordRosSerialized(kOdomInfoTopic, SerializeMessage(*msg), metadata);
    }

    void OnMap(const MapMsg::ConstPtr& msg) {
        auto metadata = BuildMetadata(*msg);
        size_t occupied_count = 0;
        for (const auto value : msg->data) {
            if (value > 0) {
                ++occupied_count;
            }
        }
        metadata.payload_summary = {
            {"resolution", std::to_string(msg->info.resolution)},
            {"width", std::to_string(msg->info.width)},
            {"height", std::to_string(msg->info.height)},
            {"origin_x", std::to_string(msg->info.origin.position.x)},
            {"origin_y", std::to_string(msg->info.origin.position.y)},
            {"occupied_count", std::to_string(occupied_count)},
        };
        L2::RecordRosSerialized(kMapTopic, SerializeMessage(*msg), metadata);
    }

    void OnLocalMap(const LocalMapMsg::ConstPtr& msg) {
        auto metadata = BuildMetadata(*msg);
        size_t occupied_count = 0;
        size_t dynamic_count = 0;
        for (const auto& cell : msg->data) {
            if (cell.occupancy) {
                ++occupied_count;
            }
            if (cell.dynamic) {
                ++dynamic_count;
            }
        }
        metadata.payload_summary = {
            {"resolution", std::to_string(msg->info.resolution)},
            {"width", std::to_string(msg->info.width)},
            {"height", std::to_string(msg->info.height)},
            {"origin_x", std::to_string(msg->info.origin.position.x)},
            {"origin_y", std::to_string(msg->info.origin.position.y)},
            {"occupied_count", std::to_string(occupied_count)},
            {"dynamic_count", std::to_string(dynamic_count)},
        };
        L2::RecordRosSerialized(kLocalMapTopic, SerializeMessage(*msg), metadata);
    }

    void OnLocationCode(const LocationCodeMsg::ConstPtr& msg) {
        auto metadata = BuildMetadata(*msg);
        metadata.payload_summary = {
            {"status_code", std::to_string(msg->status)},
            {"status_name", std::to_string(msg->status)},
            {"fault_count", std::to_string(msg->error_info.size())},
            {"primary_fault_code", FirstErrorCode(*msg)},
            {"primary_fault_msg", FirstErrorMessage(*msg)},
        };
        L2::RecordRosSerialized(kLocationCodeTopic, SerializeMessage(*msg),
                                metadata);
    }

    void OnPerceptionCode(const PerceptionCodeMsg::ConstPtr& msg) {
        auto metadata = BuildMetadata(*msg);
        metadata.payload_summary = {
            {"status_code", std::to_string(msg->status)},
            {"status_name", std::to_string(msg->status)},
            {"fault_count", std::to_string(msg->error_info.size())},
            {"primary_fault_code", FirstErrorCode(*msg)},
            {"primary_fault_msg", FirstErrorMessage(*msg)},
        };
        L2::RecordRosSerialized(kPerceptionCodeTopic, SerializeMessage(*msg),
                                metadata);
    }

    void OnNavigationCode(const NavigationCodeMsg::ConstPtr& msg) {
        auto metadata = BuildMetadata(*msg);
        metadata.payload_summary = {
            {"status_code", std::to_string(msg->status)},
            {"status_name", NavigationStatusName(msg->status)},
            {"fault_count", std::to_string(msg->faults.size())},
            {"primary_fault_code", msg->faults.empty() ? "0" : std::to_string(msg->faults.front().code)},
            {"primary_fault_msg", msg->faults.empty() ? "" : msg->faults.front().msg},
        };
        L2::RecordRosSerialized(kNavigationCodeTopic, SerializeMessage(*msg),
                                metadata);
    }

    void OnCmdVel(const CmdVelMsg::ConstPtr& msg) {
        L2RosRecordMetadata metadata;
        metadata.payload_summary = {
            {"linear_x", std::to_string(msg->linear.x)},
            {"linear_y", std::to_string(msg->linear.y)},
            {"angular_z", std::to_string(msg->angular.z)},
            {"motion_state", MotionStateFromTwist(*msg)},
        };
        L2::RecordRosSerialized(kCmdVelCalibTopic, SerializeMessage(*msg),
                                metadata);
    }

    void OnChassisAgvState(const ChassisAgvStateMsg::ConstPtr& msg) {
        auto metadata = BuildMetadata(*msg);
        metadata.payload_summary = {
            {"state", std::to_string(msg->state)},
            {"description", msg->description},
            {"battery_voltage", std::to_string(msg->battery_voltage)},
            {"battery_current", std::to_string(msg->battery_current)},
            {"battery_percentage", std::to_string(msg->battery_percentage)},
        };
        L2::RecordRosSerialized(kChassisAgvStateTopic, SerializeMessage(*msg),
                                metadata);
    }

    void OnNavigationGoal(const NavigationGoalMsg::ConstPtr& msg) {
        L2NavigationGoalSummaryInput summary;
        summary.task_type = ExtractValue(msg->goal.task_type);
        summary.task_type_name = ExtractTaskTypeName(msg->goal);
        summary.translation_enable = ExtractTranslationEnable(msg->goal.translation);
        summary.translation_heading =
            ExtractTranslationHeading(msg->goal.translation);
        summary.distance_tolerance = ExtractDistanceTolerance(msg->goal);
        summary.heading_tolerance = ExtractHeadingTolerance(msg->goal);
        summary.header_stamp_ns = HeaderAccessor<NavigationGoalMsg>::StampNs(*msg);
        for (const auto& waypoint : msg->goal.waypoints) {
            summary.waypoints.push_back(
                {WaypointAccessor<decltype(waypoint)>::X(waypoint),
                 WaypointAccessor<decltype(waypoint)>::Y(waypoint),
                 WaypointAccessor<decltype(waypoint)>::Yaw(waypoint)});
        }

        current_task_id = GenerateNavigationTaskId(summary, ++task_sequence);
        auto metadata = BuildMetadata(*msg);
        L2::RecordNavigationGoal(SerializeMessage(*msg), current_task_id, summary,
                                 metadata);
    }

    void OnNavigationFeedback(const NavigationFeedbackMsg::ConstPtr& msg) {
        L2NavigationFeedbackSummaryInput summary;
        summary.state = ExtractValue(msg->feedback.state);
        summary.state_name = NavigationStateName(summary.state);
        for (const auto& fault : msg->feedback.faults) {
            summary.faults.push_back({fault.code, fault.msg});
        }

        auto metadata = BuildMetadata(*msg);
        const std::string task_id =
            current_task_id.empty() ? "navigation_feedback" : current_task_id;
        L2::RecordNavigationFeedback(SerializeMessage(*msg), task_id, summary,
                                     metadata);
    }

    void OnNavigationResult(const NavigationResultMsg::ConstPtr& msg) {
        L2NavigationResultSummaryInput summary;
        summary.state = ExtractValue(msg->result.state);
        summary.state_name = NavigationStateName(summary.state);
        summary.duration_min = DurationToMinutes(msg->result.duration);
        summary.distance_deviation = msg->result.distance_deviation;
        summary.heading_deviation = msg->result.heading_deviation;
        for (const auto& cause : msg->result.causes) {
            summary.causes.push_back({cause.code, cause.msg});
        }

        auto metadata = BuildMetadata(*msg);
        const std::string task_id =
            current_task_id.empty() ? "navigation_result" : current_task_id;
        L2::RecordNavigationResult(SerializeMessage(*msg), task_id, summary,
                                   metadata);
        if (summary.state_name == "Succeeded") {
            current_task_id.clear();
        }
    }
};

L2Ros1Recorder::L2Ros1Recorder() : impl_(std::make_unique<Impl>()) {}

L2Ros1Recorder::~L2Ros1Recorder() = default;

int L2Ros1Recorder::Run(int argc, char** argv, const std::string& node_name) {
    ros::init(argc, argv, node_name);
    ros::NodeHandle nh;
    ros::NodeHandle pnh("~");

    impl_->nh = &nh;
    impl_->pnh = &pnh;
    pnh.param<std::string>("root_dir", impl_->root_dir, impl_->root_dir);
    pnh.param<std::string>("session_id", impl_->session_id, impl_->session_id);
    pnh.param<std::string>("sample_mode", impl_->sample_mode, impl_->sample_mode);
    pnh.param<bool>("package_on_shutdown", impl_->package_on_shutdown,
                    impl_->package_on_shutdown);
    pnh.param<std::string>("package_dir", impl_->package_dir, impl_->package_dir);

    impl_->InitRecorder();
    impl_->SubscribeTopics();
    impl_->sample_mode_timer =
        nh.createTimer(ros::Duration(0.5),
                       [impl = impl_.get()](const ros::TimerEvent&) {
                           impl->RefreshSampleMode();
                       });
    ros::spin();

    if (impl_->package_on_shutdown) {
        L2::PackageRecords(impl_->package_dir);
    } else {
        L2::Flush();
        L2::Shutdown();
    }
    return 0;
}

}  // namespace naviai::log_module
