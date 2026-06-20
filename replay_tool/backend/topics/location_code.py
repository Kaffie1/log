from __future__ import annotations

from typing import Any, Dict

try:
    from ..replay_core import ReplayRecord, deserialize_ros1_module_status
except ImportError:
    from replay_core import ReplayRecord, deserialize_ros1_module_status


def parse_location_code_info(record: ReplayRecord) -> Dict[str, Any]:
    summary = record.metadata.get("payload_summary", {})
    try:
        decoded = deserialize_ros1_module_status(record.payload)
        primary_error = decoded["error_info"][0] if decoded["error_info"] else {}
        return {
            "status_code": str(decoded.get("status", summary.get("status_code", ""))),
            "status_name": str(
                decoded.get("status_name", summary.get("status_name", ""))
            ),
            "fault_count": str(len(decoded.get("error_info", []))),
            "primary_fault_code": str(primary_error.get("code", "0")),
            "primary_fault_msg": str(primary_error.get("message", "")),
        }
    except Exception:
        return {
            "status_code": str(summary.get("status_code", "")),
            "status_name": str(summary.get("status_name", "")),
            "fault_count": str(summary.get("fault_count", "0")),
            "primary_fault_code": str(summary.get("primary_fault_code", "0")),
            "primary_fault_msg": str(summary.get("primary_fault_msg", "")),
        }
