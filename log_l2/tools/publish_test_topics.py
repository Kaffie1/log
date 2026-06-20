#!/usr/bin/env python3

import math

import rospy
from actionlib_msgs.msg import GoalID, GoalStatus
from chassis_msgs.msg import AGVState
from geometry_msgs.msg import Twist
from module_common_msgs.msg import ErrorInfo as ModuleErrorInfo
from module_common_msgs.msg import ModuleStatus as ModuleCommonStatus
from nav_msgs.msg import OccupancyGrid, Odometry
from navigation.msg import ErrorInfo as NavigationErrorInfo
from navigation.msg import LocalMap
from navigation.msg import LocalMapData
from navigation.msg import ModuleStatus as NavigationModuleStatus
from navigation.msg import NavigationActionFeedback
from navigation.msg import NavigationActionGoal
from navigation.msg import NavigationActionResult
from navigation.msg import Translation
from navigation.msg import Waypoint


def make_goal(seq):
    msg = NavigationActionGoal()
    now = rospy.Time.now()
    msg.header.seq = seq
    msg.header.stamp = now
    msg.header.frame_id = "map"
    msg.goal_id = GoalID(stamp=now, id="demo-goal-{}".format(seq))
    msg.goal.header.seq = seq
    msg.goal.header.stamp = now
    msg.goal.header.frame_id = "map"
    msg.goal.task_type.value = 0

    waypoint = Waypoint()
    waypoint.pose.position.x = 1.0
    waypoint.pose.position.y = 2.0
    waypoint.pose.orientation.w = 1.0
    waypoint.distance_tolerance = 0.2
    waypoint.heading_tolerance = 0.15
    msg.goal.waypoints = [waypoint]

    msg.goal.translation = Translation(enable=True, heading=0.25)
    return msg


def make_feedback(seq):
    msg = NavigationActionFeedback()
    now = rospy.Time.now()
    msg.header.seq = seq
    msg.header.stamp = now
    msg.header.frame_id = "map"
    msg.status = GoalStatus(goal_id=GoalID(stamp=now, id="demo-goal-1"), status=1, text="running")
    msg.feedback.header.seq = seq
    msg.feedback.header.stamp = now
    msg.feedback.header.frame_id = "map"
    msg.feedback.state.value = 2
    msg.feedback.faults = []
    return msg


def make_result(seq):
    msg = NavigationActionResult()
    now = rospy.Time.now()
    msg.header.seq = seq
    msg.header.stamp = now
    msg.header.frame_id = "map"
    msg.status = GoalStatus(goal_id=GoalID(stamp=now, id="demo-goal-1"), status=3, text="done")
    msg.result.header.seq = seq
    msg.result.header.stamp = now
    msg.result.header.frame_id = "map"
    msg.result.duration = rospy.Duration.from_sec(12.0)
    msg.result.distance_deviation = 0.03
    msg.result.heading_deviation = 0.01
    msg.result.state.value = 6
    msg.result.causes = []
    return msg


def make_odom(seq, t):
    msg = Odometry()
    msg.header.seq = seq
    msg.header.stamp = rospy.Time.now()
    msg.header.frame_id = "map"
    msg.child_frame_id = "base_link"
    msg.pose.pose.position.x = math.cos(t)
    msg.pose.pose.position.y = math.sin(t)
    msg.pose.pose.orientation.w = 1.0
    msg.twist.twist.linear.x = 0.3
    msg.twist.twist.angular.z = 0.1
    return msg


def make_map(seq):
    msg = OccupancyGrid()
    msg.header.seq = seq
    msg.header.stamp = rospy.Time.now()
    msg.header.frame_id = "map"
    msg.info.map_load_time = msg.header.stamp
    msg.info.resolution = 0.05
    msg.info.width = 4
    msg.info.height = 4
    msg.info.origin.orientation.w = 1.0
    msg.data = [0, 0, 100, 0,
                0, 100, 100, 0,
                0, 0, 0, 0,
                0, 0, 0, 0]
    return msg


def make_local_map(seq):
    msg = LocalMap()
    msg.header.seq = seq
    msg.header.stamp = rospy.Time.now()
    msg.header.frame_id = "map"
    msg.info.map_load_time = msg.header.stamp
    msg.info.resolution = 0.1
    msg.info.width = 2
    msg.info.height = 2
    msg.info.origin.orientation.w = 1.0

    cells = []
    for i in range(4):
        cell = LocalMapData()
        cell.occupancy = (i % 2 == 0)
        cell.semantic = i
        cell.dynamic = (i == 1)
        cell.speed = 0.2 * i
        cell.direction = 0.1 * i
        cells.append(cell)
    msg.data = cells
    return msg


def make_module_status(seq, status_value, code, text):
    msg = ModuleCommonStatus()
    msg.status = status_value
    if code != 0:
        item = ModuleErrorInfo()
        item.code = code
        item.message = text
        msg.error_info = [item]
    else:
        msg.error_info = []
    return msg


def make_navigation_status(seq):
    msg = NavigationModuleStatus()
    msg.header.seq = seq
    msg.header.stamp = rospy.Time.now()
    msg.header.frame_id = "map"
    msg.status = 1
    msg.faults = []
    return msg


def make_agv_state():
    msg = AGVState()
    msg.state = 1
    msg.description = "demo"
    msg.battery_voltage = 52.1
    msg.battery_current = 1.3
    msg.battery_percentage = 88.0
    msg.x = 1.2
    msg.y = 2.3
    msg.theta = 0.2
    msg.vx = 0.3
    msg.vy = 0.0
    msg.omega = 0.1
    return msg


def make_cmd_vel():
    msg = Twist()
    msg.linear.x = 0.2
    msg.angular.z = 0.1
    return msg


def main():
    rospy.init_node("l2_test_topic_publisher")

    pub_goal = rospy.Publisher("/zj_humanoid/navigation/navigation/goal", NavigationActionGoal, queue_size=10)
    pub_feedback = rospy.Publisher("/zj_humanoid/navigation/navigation/feedback", NavigationActionFeedback, queue_size=10)
    pub_result = rospy.Publisher("/zj_humanoid/navigation/navigation/result", NavigationActionResult, queue_size=10)
    pub_odom = rospy.Publisher("/zj_humanoid/navigation/odom_info", Odometry, queue_size=20)
    pub_map = rospy.Publisher("/zj_humanoid/navigation/map", OccupancyGrid, queue_size=1, latch=True)
    pub_local_map = rospy.Publisher("/zj_humanoid/navigation/local_map", LocalMap, queue_size=5)
    pub_location = rospy.Publisher("/zj_humanoid/perception/location_code", ModuleCommonStatus, queue_size=5)
    pub_perception = rospy.Publisher("/zj_humanoid/perception/perception_code", ModuleCommonStatus, queue_size=5)
    pub_navigation_code = rospy.Publisher("/zj_humanoid/navigation/navigation_code", NavigationModuleStatus, queue_size=5)
    pub_agv = rospy.Publisher("/zj_humanoid/chassis/agv_state", AGVState, queue_size=5)
    pub_cmd_vel = rospy.Publisher("/zj_humanoid/cmd_vel/calib", Twist, queue_size=10)

    rospy.sleep(1.0)
    pub_map.publish(make_map(1))
    pub_goal.publish(make_goal(1))

    rate = rospy.Rate(5)
    seq = 2
    start = rospy.Time.now().to_sec()

    while not rospy.is_shutdown():
        t = rospy.Time.now().to_sec() - start
        pub_feedback.publish(make_feedback(seq))
        pub_odom.publish(make_odom(seq, t))
        pub_local_map.publish(make_local_map(seq))
        pub_location.publish(make_module_status(seq, 2, 0, ""))
        pub_perception.publish(make_module_status(seq, 2, 1001, "perception warning"))
        pub_navigation_code.publish(make_navigation_status(seq))
        pub_agv.publish(make_agv_state())
        pub_cmd_vel.publish(make_cmd_vel())

        if seq % 20 == 0:
            pub_result.publish(make_result(seq))
            pub_goal.publish(make_goal(seq))

        seq += 1
        rate.sleep()


if __name__ == "__main__":
    main()
