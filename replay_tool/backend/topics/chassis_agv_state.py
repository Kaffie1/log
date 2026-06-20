from __future__ import annotations

from typing import Any, Dict

try:
    from ..replay_core import ReplayRecord, RosMessageFactory, deserialize_ros1_agv_state
except ImportError:
    from replay_core import ReplayRecord, RosMessageFactory, deserialize_ros1_agv_state


def parse_agv_state_info(record: ReplayRecord, factory: RosMessageFactory) -> Dict[str, Any]:
    summary = record.metadata.get("payload_summary", {})
    state_value = summary.get("state", 0)
    try:
        state_value = int(state_value)
    except Exception:
        state_value = 0
    try:
        message = factory.deserialize(record.topic_type, record.payload)
        if message is None:
            decoded = deserialize_ros1_agv_state(record.payload)
            return {
                "state": int(decoded.get("state", state_value)),
                "description": str(
                    decoded.get("description", summary.get("description", ""))
                ),
                "battery_voltage": float(
                    decoded.get("battery_voltage", summary.get("battery_voltage", 0.0))
                ),
                "battery_current": float(
                    decoded.get("battery_current", summary.get("battery_current", 0.0))
                ),
                "battery_percentage": float(
                    decoded.get("battery_percentage", summary.get("battery_percentage", 0.0))
                ),
            }
        return {
            "state": int(getattr(message, "state", state_value)),
            "description": str(
                getattr(message, "description", summary.get("description", ""))
            ),
            "battery_voltage": float(
                getattr(message, "battery_voltage", summary.get("battery_voltage", 0.0))
            ),
            "battery_current": float(
                getattr(message, "battery_current", summary.get("battery_current", 0.0))
            ),
            "battery_percentage": float(
                getattr(message, "battery_percentage", summary.get("battery_percentage", 0.0))
            ),
        }
    except Exception:
        return {
            "state": state_value,
            "description": str(summary.get("description", "")),
            "battery_voltage": float(summary.get("battery_voltage", 0.0)),
            "battery_current": float(summary.get("battery_current", 0.0)),
            "battery_percentage": float(summary.get("battery_percentage", 0.0)),
        }
