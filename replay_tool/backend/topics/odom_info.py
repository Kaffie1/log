from __future__ import annotations

from typing import Optional

try:
    from ..replay_core import PoseState, ReplayRecord, RosMessageFactory, quaternion_to_yaw
except ImportError:
    from replay_core import PoseState, ReplayRecord, RosMessageFactory, quaternion_to_yaw


def parse_odometry(record: ReplayRecord, factory: RosMessageFactory) -> Optional[PoseState]:
    message = factory.deserialize(record.topic_type, record.payload)
    if message is not None:
        yaw = quaternion_to_yaw(
            float(message.pose.pose.orientation.x),
            float(message.pose.pose.orientation.y),
            float(message.pose.pose.orientation.z),
            float(message.pose.pose.orientation.w),
        )
        return PoseState(
            x=float(message.pose.pose.position.x),
            y=float(message.pose.pose.position.y),
            yaw=yaw,
            linear_speed=float(message.twist.twist.linear.x),
            angular_speed=float(message.twist.twist.angular.z),
        )

    summary = record.metadata.get("payload_summary", {})
    try:
        return PoseState(
            x=float(summary["x"]),
            y=float(summary["y"]),
            yaw=float(summary.get("yaw", 0.0)),
            linear_speed=float(summary.get("linear_speed", 0.0)),
            angular_speed=float(summary.get("angular_speed", 0.0)),
        )
    except Exception:
        return None
