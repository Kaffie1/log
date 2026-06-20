from __future__ import annotations

from typing import Any, Dict

try:
    from ..replay_core import ReplayRecord, RosMessageFactory
except ImportError:
    from replay_core import ReplayRecord, RosMessageFactory


NAVIGATION_STATUS_NAMES = {
    0: "IDLE",
    1: "RUNNING",
    2: "PAUSED",
    3: "COMPLETED",
    4: "ERROR",
}


def parse_navigation_code_info(
    record: ReplayRecord, factory: RosMessageFactory | None = None
) -> Dict[str, Any]:
    summary = record.metadata.get("payload_summary", {})
    message_factory = factory or RosMessageFactory()
    try:
        message = message_factory.deserialize(record.topic_type, record.payload)
        if message is not None:
            faults = list(getattr(message, "faults", []))
            primary_fault = faults[0] if faults else None
            status_value = int(getattr(message, "status", summary.get("status_code", 0)))
            return {
                "status_code": str(status_value),
                "status_name": NAVIGATION_STATUS_NAMES.get(
                    status_value, str(summary.get("status_name", status_value))
                ),
                "fault_count": str(len(faults)),
                "primary_fault_code": str(getattr(primary_fault, "code", "0") if primary_fault else "0"),
                "primary_fault_msg": str(getattr(primary_fault, "msg", "") if primary_fault else ""),
            }
    except Exception:
        pass
    return {
        "status_code": str(summary.get("status_code", "")),
        "status_name": str(summary.get("status_name", "")),
        "fault_count": str(summary.get("fault_count", "0")),
        "primary_fault_code": str(summary.get("primary_fault_code", "0")),
        "primary_fault_msg": str(summary.get("primary_fault_msg", "")),
    }
