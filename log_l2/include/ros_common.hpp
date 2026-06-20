#pragma once

#include <chassis_msgs/AGVState.h>
#include <geometry_msgs/Twist.h>
#include <module_common_msgs/ModuleStatus.h>
#include <nav_msgs/Odometry.h>
#include <nav_msgs/OccupancyGrid.h>
#include <navigation/LocalMap.h>
#include <navigation/NavigationActionFeedback.h>
#include <navigation/NavigationActionGoal.h>
#include <navigation/NavigationActionResult.h>
#include <navigation/ModuleStatus.h>

namespace naviai::log::l2_ros {

using NavigationGoalMsg = navigation::NavigationActionGoal;
using NavigationFeedbackMsg = navigation::NavigationActionFeedback;
using NavigationResultMsg = navigation::NavigationActionResult;

using OdomInfoMsg = nav_msgs::Odometry;
using MapMsg = nav_msgs::OccupancyGrid;
using LocalMapMsg = navigation::LocalMap;
using LocationCodeMsg = module_common_msgs::ModuleStatus;
using PerceptionCodeMsg = module_common_msgs::ModuleStatus;
using NavigationCodeMsg = navigation::ModuleStatus;
using ChassisAgvStateMsg = chassis_msgs::AGVState;
using CmdVelMsg = geometry_msgs::Twist;

inline constexpr const char* kNavigationGoalTopic = "/zj_humanoid/navigation/navigation/goal";
inline constexpr const char* kNavigationFeedbackTopic = "/zj_humanoid/navigation/navigation/feedback";
inline constexpr const char* kNavigationResultTopic = "/zj_humanoid/navigation/navigation/result";
inline constexpr const char* kOdomInfoTopic = "/zj_humanoid/navigation/odom_info";
inline constexpr const char* kMapTopic = "/zj_humanoid/navigation/map";
inline constexpr const char* kLocalMapTopic = "/zj_humanoid/navigation/local_map";
inline constexpr const char* kLocationCodeTopic = "/zj_humanoid/perception/location_code";
inline constexpr const char* kPerceptionCodeTopic = "/zj_humanoid/perception/perception_code";
inline constexpr const char* kNavigationCodeTopic = "/zj_humanoid/navigation/navigation_code";
inline constexpr const char* kChassisAgvStateTopic = "/zj_humanoid/chassis/agv_state";
inline constexpr const char* kCmdVelCalibTopic = "/zj_humanoid/cmd_vel/calib";
}  // namespace naviai::log::l2_ros
