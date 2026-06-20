#!/usr/bin/env python3
"""Shared replay core types, constants, decoding and record iteration helpers."""

from __future__ import annotations

import base64
import gzip
import importlib
import json
import math
import os
import shutil
import struct
import tarfile
import tempfile
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Dict, Iterable, List, Optional, Tuple


HEADER_STRUCT = struct.Struct("<IIqqqQ")

ODOM_TOPIC = "/zj_humanoid/navigation/odom_info"
MAP_TOPIC = "/zj_humanoid/navigation/map"
LOCAL_MAP_TOPIC = "/zj_humanoid/navigation/local_map"
LOCATION_CODE_TOPIC = "/zj_humanoid/perception/location_code"
PERCEPTION_CODE_TOPIC = "/zj_humanoid/perception/perception_code"
NAVIGATION_CODE_TOPIC = "/zj_humanoid/navigation/navigation_code"
CHASSIS_AGV_STATE_TOPIC = "/zj_humanoid/chassis/agv_state"
CMD_VEL_CALIB_TOPIC = "/zj_humanoid/cmd_vel/calib"
NAV_GOAL_TOPIC = "/zj_humanoid/navigation/navigation/goal"
NAV_FEEDBACK_TOPIC = "/zj_humanoid/navigation/navigation/feedback"
NAV_RESULT_TOPIC = "/zj_humanoid/navigation/navigation/result"
TOPIC_RECORD_LOG_MODULES = {"l2_topic_record", "l2_topic_record_map"}
ARCHIVE_SUFFIXES = (".tar.xz", ".txz")
EXTRACT_ROOT = Path(tempfile.gettempdir()) / "replay_tool_extract"


def log_replay_core(message: str) -> None:
    print(f"[replay_core] {message}", flush=True)


try:
    import numpy as np
except Exception as exc:  # pragma: no cover - import guard
    raise SystemExit(
        "numpy is required for replay_viewer.py. "
        "Please install python3-numpy first."
    ) from exc


@dataclass
class ReplayRecord:
    topic: str
    topic_type: str
    record_time_us: int
    message_time_us: int
    metadata: Dict[str, Any]
    payload: bytes
    file_path: Path

    @property
    def time_us(self) -> int:
        return self.message_time_us if self.message_time_us > 0 else self.record_time_us


@dataclass
class PoseState:
    x: float
    y: float
    yaw: float
    linear_speed: float = 0.0
    angular_speed: float = 0.0


@dataclass
class GridState:
    image: np.ndarray
    extent: Tuple[float, float, float, float]
    resolution: float
    width: int
    height: int
    origin_x: float
    origin_y: float
    origin_yaw: float
    corners: Tuple[
        Tuple[float, float],
        Tuple[float, float],
        Tuple[float, float],
        Tuple[float, float],
    ]


@dataclass
class GoalState:
    task_id: str
    waypoints: List[Tuple[float, float, float]] = field(default_factory=list)
    state_name: str = "Unknown"


class RosMessageFactory:
    def __init__(self) -> None:
        self._cache: Dict[str, Optional[type]] = {}

    def get_message_class(self, topic_type: str) -> Optional[type]:
        if topic_type in self._cache:
            return self._cache[topic_type]
        try:
            package, msg_name = topic_type.split("/", 1)
            module = importlib.import_module(f"{package}.msg")
            cls = getattr(module, msg_name)
        except Exception:
            cls = None
        self._cache[topic_type] = cls
        return cls

    def deserialize(self, topic_type: str, payload: bytes) -> Optional[Any]:
        cls = self.get_message_class(topic_type)
        if cls is None:
            return None
        try:
            message = cls()
            message.deserialize(payload)
            return message
        except Exception:
            return None


class Ros1BufferReader:
    def __init__(self, payload: bytes) -> None:
        self.payload = payload
        self.offset = 0

    def read_u8(self) -> int:
        value = self.payload[self.offset]
        self.offset += 1
        return value

    def read_i8(self) -> int:
        value = struct.unpack_from("<b", self.payload, self.offset)[0]
        self.offset += 1
        return value

    def read_u32(self) -> int:
        value = struct.unpack_from("<I", self.payload, self.offset)[0]
        self.offset += 4
        return value

    def read_i32(self) -> int:
        value = struct.unpack_from("<i", self.payload, self.offset)[0]
        self.offset += 4
        return value

    def read_f32(self) -> float:
        value = struct.unpack_from("<f", self.payload, self.offset)[0]
        self.offset += 4
        return value

    def read_f64(self) -> float:
        value = struct.unpack_from("<d", self.payload, self.offset)[0]
        self.offset += 8
        return value

    def read_string(self) -> str:
        length = self.read_u32()
        value = self.payload[self.offset : self.offset + length].decode(
            "utf-8", "replace"
        )
        self.offset += length
        return value

    def read_bytes(self, length: int) -> bytes:
        value = self.payload[self.offset : self.offset + length]
        self.offset += length
        return value


def read_ros1_header(reader: Ros1BufferReader) -> Dict[str, Any]:
    return {
        "seq": reader.read_u32(),
        "stamp_sec": reader.read_u32(),
        "stamp_nsec": reader.read_u32(),
        "frame_id": reader.read_string(),
    }


def read_ros1_pose(reader: Ros1BufferReader) -> Dict[str, Any]:
    position = {
        "x": reader.read_f64(),
        "y": reader.read_f64(),
        "z": reader.read_f64(),
    }
    orientation = {
        "x": reader.read_f64(),
        "y": reader.read_f64(),
        "z": reader.read_f64(),
        "w": reader.read_f64(),
    }
    return {"position": position, "orientation": orientation}


def read_ros1_map_meta_data(reader: Ros1BufferReader) -> Dict[str, Any]:
    return {
        "map_load_time_sec": reader.read_u32(),
        "map_load_time_nsec": reader.read_u32(),
        "resolution": reader.read_f32(),
        "width": reader.read_u32(),
        "height": reader.read_u32(),
        "origin": read_ros1_pose(reader),
    }


def deserialize_ros1_occupancy_grid(payload: bytes) -> Dict[str, Any]:
    reader = Ros1BufferReader(payload)
    header = read_ros1_header(reader)
    info = read_ros1_map_meta_data(reader)
    length = reader.read_u32()
    data = np.frombuffer(reader.read_bytes(length), dtype=np.int8).copy()
    return {"header": header, "info": info, "data": data}


def deserialize_ros1_local_map(payload: bytes) -> Dict[str, Any]:
    reader = Ros1BufferReader(payload)
    header = read_ros1_header(reader)
    info = read_ros1_map_meta_data(reader)
    length = reader.read_u32()
    cells: List[Dict[str, Any]] = []
    for _ in range(length):
        cells.append(
            {
                "occupancy": bool(reader.read_u8()),
                "semantic": reader.read_i8(),
                "dynamic": bool(reader.read_u8()),
                "speed": reader.read_f64(),
                "direction": reader.read_f64(),
            }
        )
    return {"header": header, "info": info, "data": cells}


MODULE_STATUS_NAMES = {
    0: "IDLE",
    1: "INITIALIZING",
    2: "RUNNING",
    3: "PAUSED",
    4: "COMPLETED",
    5: "DEGRADED",
    6: "ERROR",
    7: "RECOVERING",
    8: "SYNCING",
}


def deserialize_ros1_module_status(payload: bytes) -> Dict[str, Any]:
    reader = Ros1BufferReader(payload)
    status = reader.read_i32()
    error_count = reader.read_u32()
    errors: List[Dict[str, Any]] = []
    for _ in range(error_count):
        errors.append({"code": reader.read_i32(), "message": reader.read_string()})
    return {
        "status": status,
        "status_name": MODULE_STATUS_NAMES.get(status, f"UNKNOWN_{status}"),
        "error_info": errors,
    }


def deserialize_ros1_agv_state(payload: bytes) -> Dict[str, Any]:
    reader = Ros1BufferReader(payload)
    return {
        "state": reader.read_i32(),
        "description": reader.read_string(),
        "battery_voltage": reader.read_f64(),
        "battery_current": reader.read_f64(),
        "battery_percentage": reader.read_f64(),
    }


def is_replay_data_file(path: Path) -> bool:
    name = path.name
    return name.endswith(".data") or name.endswith(".data.gz")


def is_topic_record_log_file(path: Path) -> bool:
    name = path.name
    return name.endswith(".log") or name.endswith(".log.gz")


def is_replay_archive(path: Path) -> bool:
    return path.is_file() and any(path.name.endswith(suffix) for suffix in ARCHIVE_SUFFIXES)


def _find_archive_candidate(path: Path) -> Optional[Path]:
    if not path.is_dir():
        return None
    preferred = path / "l2.tar.xz"
    if preferred.is_file():
        return preferred
    matches = sorted(
        child
        for child in path.iterdir()
        if child.is_file() and any(child.name.endswith(suffix) for suffix in ARCHIVE_SUFFIXES)
    )
    if len(matches) == 1:
        return matches[0]
    return None


def _looks_like_replay_root(path: Path) -> bool:
    if not path.is_dir():
        return False
    return (
        (path / "business_data").is_dir()
        or (path / "static_data").is_dir()
        or (path / "large_data").is_dir()
        or (path / "large_data" / "local_map").is_dir()
    )


def _find_replay_root(path: Path) -> Path:
    if not path.is_dir():
        return path
    if _looks_like_replay_root(path):
        return path
    children = [child for child in path.iterdir() if child.is_dir()]
    if len(children) == 1:
        child = children[0]
        if _looks_like_replay_root(child):
            return child
    return path


def _safe_extract_tar(archive_path: Path, target_dir: Path) -> None:
    target_root = target_dir.resolve()
    with tarfile.open(archive_path, "r:*") as tar:
        for member in tar.getmembers():
            member_path = target_dir / member.name
            resolved = member_path.resolve()
            if os.path.commonpath([str(target_root), str(resolved)]) != str(target_root):
                raise RuntimeError(f"Unsafe archive member path: {member.name}")
        tar.extractall(target_dir)


def resolve_replay_root(path: Path) -> Path:
    root = path.expanduser().resolve()
    log_replay_core(f"resolve_replay_root input={root}")
    archive_candidate = _find_archive_candidate(root)
    if archive_candidate is not None:
        log_replay_core(f"using archive candidate {archive_candidate}")
        root = archive_candidate
    if not is_replay_archive(root):
        resolved = _find_replay_root(root)
        log_replay_core(f"resolved non-archive root to {resolved}")
        return resolved

    stat = root.stat()
    cache_key = f"{root.stem}_{stat.st_size}_{int(stat.st_mtime)}"
    extract_dir = EXTRACT_ROOT / cache_key
    marker = extract_dir / ".extract_complete"
    if marker.exists():
        resolved = _find_replay_root(extract_dir)
        log_replay_core(f"reusing extracted archive at {extract_dir}, resolved root {resolved}")
        return resolved

    if extract_dir.exists():
        log_replay_core(f"removing incomplete extract dir {extract_dir}")
        shutil.rmtree(extract_dir)
    extract_dir.mkdir(parents=True, exist_ok=True)
    log_replay_core(f"extracting archive {root} -> {extract_dir}")
    _safe_extract_tar(root, extract_dir)
    marker.touch()
    resolved = _find_replay_root(extract_dir)
    log_replay_core(f"finished extracting archive {root}, resolved root {resolved}")
    return resolved


def map_replay_root(root: Path) -> Path:
    return root / "static_data"


def local_map_replay_root(root: Path) -> Path:
    return root / "large_data" / "local_map"


def business_replay_root(root: Path) -> Path:
    return root / "business_data"


def iter_replay_files(root: Path) -> Iterable[Path]:
    if root.is_file():
        if is_replay_data_file(root) or is_topic_record_log_file(root):
            return [root]
        return []

    patterns = ("*.data", "*.data.gz", "*.log", "*.log.gz")
    files: List[Path] = []
    for pattern in patterns:
        files.extend(root.rglob(pattern))
    return sorted(path for path in files if path.is_file())


def open_maybe_gzip(path: Path):
    if path.suffix == ".gz":
        return gzip.open(path, "rb")
    return path.open("rb")


def decode_payload_bytes(payload: Any, payload_encoding: str) -> Optional[bytes]:
    if isinstance(payload, str):
        if payload_encoding in {"ros_serialized", "ros_serialized_base64", "base64"}:
            try:
                return base64.b64decode(payload, validate=True)
            except Exception:
                return None
        return payload.encode("utf-8", "replace")
    if isinstance(payload, bytes):
        return payload
    if payload is None:
        return b""
    try:
        return json.dumps(payload, ensure_ascii=False).encode("utf-8")
    except Exception:
        return None


def iter_binary_records(path: Path) -> Iterable[ReplayRecord]:
    records: List[ReplayRecord] = []
    with open_maybe_gzip(path) as handle:
        while True:
            header_bytes = handle.read(HEADER_STRUCT.size)
            if not header_bytes or len(header_bytes) != HEADER_STRUCT.size:
                break
            metadata_size, payload_size, record_time_us, message_time_us, _, _ = HEADER_STRUCT.unpack(
                header_bytes
            )
            metadata_raw = handle.read(metadata_size)
            payload = handle.read(payload_size)
            if len(metadata_raw) != metadata_size or len(payload) != payload_size:
                break
            try:
                metadata = json.loads(metadata_raw.decode("utf-8"))
            except Exception:
                continue
            topic = metadata.get("topic", "")
            topic_type = metadata.get("topic_type", "")
            if not topic or not topic_type:
                continue
            payload_encoding = str(metadata.get("payload_encoding", ""))
            if payload_encoding in {"ros_serialized_base64", "base64"}:
                try:
                    payload = base64.b64decode(payload, validate=True)
                except Exception:
                    continue
            records.append(
                ReplayRecord(
                    topic=topic,
                    topic_type=topic_type,
                    record_time_us=record_time_us,
                    message_time_us=message_time_us,
                    metadata=metadata,
                    payload=payload,
                    file_path=path,
                )
            )
    records.sort(key=lambda record: record.time_us)
    yield from records


def iter_topic_record_log_records(path: Path) -> Iterable[ReplayRecord]:
    records: List[ReplayRecord] = []
    with open_maybe_gzip(path) as handle:
        for raw_line in handle:
            try:
                line = raw_line.decode("utf-8").strip()
            except Exception:
                continue
            if not line:
                continue
            try:
                outer = json.loads(line)
            except Exception:
                continue
            if not isinstance(outer, dict) or outer.get("module") not in TOPIC_RECORD_LOG_MODULES:
                continue

            payload_raw = outer.get("payload")
            if not isinstance(payload_raw, str) or not payload_raw:
                continue
            try:
                metadata = json.loads(payload_raw)
            except Exception:
                continue
            if not isinstance(metadata, dict):
                continue

            topic = str(metadata.get("topic", ""))
            topic_type = str(metadata.get("topic_type", ""))
            if not topic or not topic_type:
                continue

            payload_encoding = str(metadata.get("payload_encoding", ""))
            payload = decode_payload_bytes(metadata.get("payload"), payload_encoding)
            if payload is None:
                continue

            try:
                record_time_us = int(metadata.get("record_time_us", 0))
            except Exception:
                record_time_us = 0
            try:
                message_time_us = int(metadata.get("message_time_us", 0))
            except Exception:
                message_time_us = 0
            if record_time_us <= 0:
                record_time_us = message_time_us
            if message_time_us <= 0:
                message_time_us = record_time_us
            if record_time_us <= 0 and message_time_us <= 0:
                continue

            records.append(
                ReplayRecord(
                    topic=topic,
                    topic_type=topic_type,
                    record_time_us=record_time_us,
                    message_time_us=message_time_us,
                    metadata=metadata,
                    payload=payload,
                    file_path=path,
                )
            )

    records.sort(key=lambda record: record.time_us)
    yield from records


def iter_records(path: Path) -> Iterable[ReplayRecord]:
    if is_replay_data_file(path):
        yield from iter_binary_records(path)
        return
    if is_topic_record_log_file(path):
        yield from iter_topic_record_log_records(path)
        return


def quaternion_to_yaw(x: float, y: float, z: float, w: float) -> float:
    siny_cosp = 2.0 * (w * z + x * y)
    cosy_cosp = 1.0 - 2.0 * (y * y + z * z)
    return math.atan2(siny_cosp, cosy_cosp)


def build_grid_geometry(
    origin_x: float,
    origin_y: float,
    origin_yaw: float,
    width: int,
    height: int,
    resolution: float,
    center_origin: bool = False,
) -> Tuple[
    Tuple[Tuple[float, float], Tuple[float, float], Tuple[float, float], Tuple[float, float]],
    Tuple[float, float, float, float],
]:
    span_x = width * resolution
    span_y = height * resolution
    cos_yaw = math.cos(origin_yaw)
    sin_yaw = math.sin(origin_yaw)
    offset_x = -span_x / 2.0 if center_origin else 0.0
    offset_y = -span_y / 2.0 if center_origin else 0.0

    def transform(local_x: float, local_y: float) -> Tuple[float, float]:
        shifted_x = local_x + offset_x
        shifted_y = local_y + offset_y
        world_x = origin_x + shifted_x * cos_yaw - shifted_y * sin_yaw
        world_y = origin_y + shifted_x * sin_yaw + shifted_y * cos_yaw
        return (world_x, world_y)

    corners = (
        transform(0.0, 0.0),
        transform(span_x, 0.0),
        transform(span_x, span_y),
        transform(0.0, span_y),
    )
    xs = [point[0] for point in corners]
    ys = [point[1] for point in corners]
    extent = (min(xs), max(xs), min(ys), max(ys))
    return corners, extent


def occupancy_grid_to_image(message: Any) -> GridState:
    if isinstance(message, dict):
        width = int(message["info"]["width"])
        height = int(message["info"]["height"])
        resolution = float(message["info"]["resolution"])
        origin_x = float(message["info"]["origin"]["position"]["x"])
        origin_y = float(message["info"]["origin"]["position"]["y"])
        origin_yaw = quaternion_to_yaw(
            float(message["info"]["origin"]["orientation"]["x"]),
            float(message["info"]["origin"]["orientation"]["y"]),
            float(message["info"]["origin"]["orientation"]["z"]),
            float(message["info"]["origin"]["orientation"]["w"]),
        )
        data = np.asarray(message["data"], dtype=np.int16).reshape((height, width))
    else:
        width = int(message.info.width)
        height = int(message.info.height)
        resolution = float(message.info.resolution)
        origin_x = float(message.info.origin.position.x)
        origin_y = float(message.info.origin.position.y)
        origin_yaw = quaternion_to_yaw(
            float(message.info.origin.orientation.x),
            float(message.info.origin.orientation.y),
            float(message.info.origin.orientation.z),
            float(message.info.origin.orientation.w),
        )
        data = np.asarray(message.data, dtype=np.int16).reshape((height, width))

    image = np.empty((height, width, 4), dtype=np.float32)
    unknown_mask = data < 0
    known = np.clip(data, 0, 100).astype(np.float32)
    grayscale = (100.0 - known) / 100.0

    image[..., 0] = grayscale
    image[..., 1] = grayscale
    image[..., 2] = grayscale
    image[..., 3] = np.where(unknown_mask, 0.0, 1.0)

    image = np.flipud(image)
    corners, extent = build_grid_geometry(
        origin_x, origin_y, origin_yaw, width, height, resolution
    )
    return GridState(
        image=image,
        extent=extent,
        resolution=resolution,
        width=width,
        height=height,
        origin_x=origin_x,
        origin_y=origin_y,
        origin_yaw=origin_yaw,
        corners=corners,
    )


def local_map_to_image(message: Any) -> GridState:
    if isinstance(message, dict):
        width = int(message["info"]["width"])
        height = int(message["info"]["height"])
        resolution = float(message["info"]["resolution"])
        origin_x = float(message["info"]["origin"]["position"]["x"])
        origin_y = float(message["info"]["origin"]["position"]["y"])
        origin_yaw = quaternion_to_yaw(
            float(message["info"]["origin"]["orientation"]["x"]),
            float(message["info"]["origin"]["orientation"]["y"]),
            float(message["info"]["origin"]["orientation"]["z"]),
            float(message["info"]["origin"]["orientation"]["w"]),
        )
        cells = list(message["data"])
    else:
        width = int(message.info.width)
        height = int(message.info.height)
        resolution = float(message.info.resolution)
        origin_x = float(message.info.origin.position.x)
        origin_y = float(message.info.origin.position.y)
        origin_yaw = quaternion_to_yaw(
            float(message.info.origin.orientation.x),
            float(message.info.origin.orientation.y),
            float(message.info.origin.orientation.z),
            float(message.info.origin.orientation.w),
        )
        cells = list(message.data)

    image = np.zeros((height, width, 4), dtype=np.float32)
    if len(cells) < width * height:
        cells = cells + [None] * (width * height - len(cells))

    for index, cell in enumerate(cells[: width * height]):
        row = index // width
        col = index % width
        if cell is None:
            continue
        if isinstance(cell, dict):
            occupied = bool(cell.get("occupancy", False))
            dynamic = bool(cell.get("dynamic", False))
        else:
            occupied = bool(getattr(cell, "occupancy", False))
            dynamic = bool(getattr(cell, "dynamic", False))
        if dynamic:
            image[row, col] = (0.82, 0.25, 0.92, 0.95)
        elif occupied:
            image[row, col] = (0.72, 0.32, 0.88, 0.72)

    image = np.flipud(image)
    corners, extent = build_grid_geometry(
        origin_x,
        origin_y,
        origin_yaw,
        width,
        height,
        resolution,
        center_origin=True,
    )
    return GridState(
        image=image,
        extent=extent,
        resolution=resolution,
        width=width,
        height=height,
        origin_x=origin_x,
        origin_y=origin_y,
        origin_yaw=origin_yaw,
        corners=corners,
    )
