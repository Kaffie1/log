# log_module

```cpp
#include "l2_log/include/log.hpp"
#include "l3_log/include/log.hpp"

int main() {
    naviai::log_module::L3::Init(
        naviai::log_module::LogLevel::Info, "/tmp/robot_logs");

    naviai::log_module::L3::RegisterModule("navigation");

    LOG_L3_INFO("navigation", "planner started, map={}", "B1");
    LOG_L3_WARN("navigation", "costmap update delayed by {} ms", 120);

    naviai::log_module::L2TopicInput topic_input;
    topic_input.AddBusinessTopic("/zj_humanoid/navigation/odom_info",
                                 "nav_msgs/Odometry",
                                 "localization",
                                 "localization");
    naviai::log_module::L2::InitRecorder({
        .root_dir = "/tmp/robot_l2",
        .session_id = "session_demo",
        .topics = topic_input.Topics(),
    });
    naviai::log_module::L2::RecordTopic({
        .topic = "/zj_humanoid/navigation/odom_info",
        .message_time_us = 1781576130123456,
        .payload = "{\"x\":1.0,\"y\":2.0}",
    });
    naviai::log_module::L2::PackageRecords();

    naviai::log_module::L3::Flush();
    naviai::log_module::L3::Shutdown();
    return 0;
}
```

ROS topic recording can use the adapter layer directly:

```cpp
#include "l2_log/include/log.hpp"

int main() {
    naviai::log_module::L2RosTopicInput ros_topics;
    ros_topics.AddSensorTopic("/mid360/points",
                              "sensor_msgs/PointCloud2",
                              "mid360_driver",
                              "mid360");

    naviai::log_module::L2::InitRecorder({
        .root_dir = "/tmp/robot_l2",
        .session_id = "ros_session_demo",
    });
    naviai::log_module::L2::RegisterRosTopics(ros_topics);

    naviai::log_module::L2::RecordRosTopic({
        .topic = "/mid360/points",
        .message_time_ns = 1781576130123456789LL,
        .frame_id = "mid360",
        .sequence = 42,
        .serialized_payload = std::string("\x01\x02\x03\x04", 4),
        .payload_summary = {{"point_count", "128000"}},
    });
}
```

Default replay topics live in `L2`:

```cpp
#include "l2_log/include/log.hpp"

int main() {
    auto replay_topics = naviai::log_module::DefaultReplayRosTopicInput();
    naviai::log_module::L2::InitRecorder({
        .root_dir = "/tmp/robot_l2",
        .session_id = "default_replay_session",
        .topics = naviai::log_module::ToL2TopicDescriptors(replay_topics.Topics()),
    });
}
```

Callback code can write to `L2` in one line:

```cpp
#include "l2_log/include/log.hpp"

struct PointCloudMsg {
    int64_t stamp_ns;
    std::string frame_id;
    int64_t seq;
};

int main() {
    auto callback = naviai::log_module::MakeRosTopicCallback<PointCloudMsg>(
        "/mid360/points",
        [](const PointCloudMsg& msg) {
            return std::string(reinterpret_cast<const char*>(&msg.seq), sizeof(msg.seq));
        },
        [](const PointCloudMsg& msg) {
            naviai::log_module::L2RosRecordMetadata metadata;
            metadata.message_time_ns = msg.stamp_ns;
            metadata.frame_id = msg.frame_id;
            metadata.sequence = msg.seq;
            metadata.payload_summary = {{"demo_seq", std::to_string(msg.seq)}};
            return metadata;
        });

    callback(PointCloudMsg{1781576130123456789LL, "mid360", 42});
}
```

Navigation action summary helpers are fixed to the replay design fields:

```cpp
#include "l2_log/include/log.hpp"

int main() {
    naviai::log_module::L2NavigationGoalSummaryInput goal;
    goal.task_type = 2;
    goal.task_type_name = "Charge";
    goal.waypoints = {{1.0, 2.0, 0.0}, {3.0, 4.0, 1.57}};
    goal.translation_enable = true;
    goal.translation_heading = 1.57;
    goal.distance_tolerance = 0.1;
    goal.heading_tolerance = 0.2;
    goal.header_stamp_ns = 1781576130123456789LL;

    const auto task_id =
        naviai::log_module::GenerateNavigationTaskId(goal, 1);

    naviai::log_module::L2::RecordNavigationGoal(
        "serialized_goal_payload",
        task_id,
        goal);

    naviai::log_module::L2NavigationFeedbackSummaryInput feedback;
    feedback.state = 2;
    feedback.state_name = "Running";
    feedback.faults = {{1001, "planner_degraded"}};
    naviai::log_module::L2::RecordNavigationFeedback(
        "serialized_feedback_payload",
        task_id,
        feedback);

    naviai::log_module::L2NavigationResultSummaryInput result;
    result.state = 7;
    result.state_name = "Succeeded";
    result.duration_min = 3.5;
    result.distance_deviation = 0.03;
    result.heading_deviation = 0.05;
    result.causes = {{0, "none"}};
    naviai::log_module::L2::RecordNavigationResult(
        "serialized_result_payload",
        task_id,
        result);
}
```
