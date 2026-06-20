from __future__ import annotations

from typing import Any, Dict

try:
    from ..replay_core import ReplayRecord, RosMessageFactory
except ImportError:
    from replay_core import ReplayRecord, RosMessageFactory


def parse_cmd_vel(record: ReplayRecord, factory: RosMessageFactory) -> Dict[str, Any]:
    summary = record.metadata.get("payload_summary", {})
    try:
        message = factory.deserialize(record.topic_type, record.payload)
        if message is not None:
            linear = getattr(message, "linear", None)
            angular = getattr(message, "angular", None)
            return {
                "linear_x": float(getattr(linear, "x", summary.get("linear_x", 0.0))),
                "linear_y": float(getattr(linear, "y", summary.get("linear_y", 0.0))),
                "angular_z": float(getattr(angular, "z", summary.get("angular_z", 0.0))),
            }
    except Exception:
        pass
    return {
        "linear_x": float(summary.get("linear_x", 0.0)),
        "linear_y": float(summary.get("linear_y", 0.0)),
        "angular_z": float(summary.get("angular_z", 0.0)),
    }
