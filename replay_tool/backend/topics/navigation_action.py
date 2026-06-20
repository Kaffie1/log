from __future__ import annotations

from typing import Any, Dict, List, Optional, Tuple

try:
    from ..replay_core import GoalState, ReplayRecord, RosMessageFactory, quaternion_to_yaw
except ImportError:
    from replay_core import GoalState, ReplayRecord, RosMessageFactory, quaternion_to_yaw


def parse_goal(record: ReplayRecord, factory: RosMessageFactory) -> Optional[GoalState]:
    message = factory.deserialize(record.topic_type, record.payload)
    task_id = record.metadata.get("task_id", "")
    default_state = str(record.metadata.get("action_state", "Goal") or "Goal")
    if message is not None:
        waypoints: List[Tuple[float, float, float]] = []
        for waypoint in getattr(message.goal, "waypoints", []):
            if hasattr(waypoint, "pose"):
                yaw = quaternion_to_yaw(
                    float(waypoint.pose.orientation.x),
                    float(waypoint.pose.orientation.y),
                    float(waypoint.pose.orientation.z),
                    float(waypoint.pose.orientation.w),
                )
                waypoints.append(
                    (
                        float(waypoint.pose.position.x),
                        float(waypoint.pose.position.y),
                        yaw,
                    )
                )
            elif hasattr(waypoint, "x"):
                waypoints.append(
                    (float(waypoint.x), float(waypoint.y), float(waypoint.yaw))
                )
        return GoalState(task_id=task_id, waypoints=waypoints, state_name=default_state)

    summary = record.metadata.get("payload_summary", {})
    points: List[Tuple[float, float, float]] = []
    for index in range(32):
        x_key = f"waypoint_{index}_x"
        y_key = f"waypoint_{index}_y"
        yaw_key = f"waypoint_{index}_yaw"
        if x_key not in summary or y_key not in summary:
            continue
        points.append(
            (
                float(summary[x_key]),
                float(summary[y_key]),
                float(summary.get(yaw_key, 0.0)),
            )
        )
    if not points and "target_x" in summary and "target_y" in summary:
        points.append(
            (
                float(summary["target_x"]),
                float(summary["target_y"]),
                float(summary.get("target_yaw", 0.0)),
            )
        )
    return GoalState(task_id=task_id, waypoints=points, state_name=default_state)


def parse_feedback_state(record: ReplayRecord) -> Tuple[str, str]:
    task_id = record.metadata.get("task_id", "")
    summary = record.metadata.get("payload_summary", {})
    return task_id, str(summary.get("state_name", record.metadata.get("action_state", "")))


def parse_result_state(record: ReplayRecord) -> Tuple[str, str]:
    task_id = record.metadata.get("task_id", "")
    summary = record.metadata.get("payload_summary", {})
    return task_id, str(summary.get("state_name", record.metadata.get("action_state", "")))


def parse_navigation_feedback_info(
    record: ReplayRecord, factory: Optional[RosMessageFactory] = None
) -> Dict[str, Any]:
    summary = record.metadata.get("payload_summary", {})
    faults: List[Dict[str, str]] = []
    message_factory = factory or RosMessageFactory()
    try:
        message = message_factory.deserialize(record.topic_type, record.payload)
        if message is not None:
            for fault in getattr(message.feedback, "faults", []):
                faults.append(
                    {
                        "code": str(getattr(fault, "code", "0")),
                        "msg": str(getattr(fault, "msg", "")),
                    }
                )
    except Exception:
        faults = []
    if not faults and str(summary.get("primary_fault_code", "0")) != "0":
        faults.append(
            {
                "code": str(summary.get("primary_fault_code", "0")),
                "msg": str(summary.get("primary_fault_msg", "")),
            }
        )
    return {
        "task_id": str(record.metadata.get("task_id", "")),
        "state_name": str(summary.get("state_name", record.metadata.get("action_state", ""))),
        "fault_count": str(summary.get("fault_count", "0")),
        "primary_fault_code": str(summary.get("primary_fault_code", "0")),
        "primary_fault_msg": str(summary.get("primary_fault_msg", "")),
        "faults": faults,
    }


def parse_navigation_result_info(
    record: ReplayRecord, factory: Optional[RosMessageFactory] = None
) -> Dict[str, Any]:
    summary = record.metadata.get("payload_summary", {})
    causes: List[Dict[str, str]] = []
    message_factory = factory or RosMessageFactory()
    try:
        message = message_factory.deserialize(record.topic_type, record.payload)
        if message is not None:
            for cause in getattr(message.result, "causes", []):
                causes.append(
                    {
                        "code": str(getattr(cause, "code", "0")),
                        "msg": str(getattr(cause, "msg", "")),
                    }
                )
    except Exception:
        causes = []
    if not causes and str(summary.get("primary_cause_code", "0")) != "0":
        causes.append(
            {
                "code": str(summary.get("primary_cause_code", "0")),
                "msg": str(summary.get("primary_cause_msg", "")),
            }
        )
    return {
        "task_id": str(record.metadata.get("task_id", "")),
        "state_name": str(summary.get("state_name", record.metadata.get("action_state", ""))),
        "cause_count": str(summary.get("cause_count", "0")),
        "primary_cause_code": str(summary.get("primary_cause_code", "0")),
        "primary_cause_msg": str(summary.get("primary_cause_msg", "")),
        "causes": causes,
    }
