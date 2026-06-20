#!/usr/bin/env python3
"""Web-based L2 replay viewer for remote development environments."""

from __future__ import annotations

import argparse
import base64
import bisect
import builtins
import gzip
import json
import struct
import threading
import tempfile
import traceback
from datetime import datetime
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple
from urllib.parse import parse_qs, unquote, urlparse

import numpy as np

try:
    from .replay_core import (
        business_replay_root,
        CHASSIS_AGV_STATE_TOPIC,
        CMD_VEL_CALIB_TOPIC,
        GridState,
        LOCAL_MAP_TOPIC,
        local_map_replay_root,
        LOCATION_CODE_TOPIC,
        MAP_TOPIC,
        NAVIGATION_CODE_TOPIC,
        NAV_FEEDBACK_TOPIC,
        NAV_GOAL_TOPIC,
        NAV_RESULT_TOPIC,
        ODOM_TOPIC,
        PERCEPTION_CODE_TOPIC,
        PoseState,
        ReplayRecord,
        RosMessageFactory,
        iter_records,
        iter_replay_files,
        map_replay_root,
        resolve_replay_root,
    )
    from .replay_viewer import build_timeline, collect_replay_records, GoalState, iter_replay_records_merged
    from .topics.cmd_vel import parse_cmd_vel
    from .topics.chassis_agv_state import parse_agv_state_info
    from .topics.local_map import parse_local_map
    from .topics.location_code import parse_location_code_info
    from .topics.map import parse_map
    from .topics.navigation_code import parse_navigation_code_info
    from .topics.navigation_action import (
        parse_feedback_state,
        parse_goal,
        parse_navigation_feedback_info,
        parse_navigation_result_info,
        parse_result_state,
    )
    from .topics.odom_info import parse_odometry
    from .topics.perception_code import parse_perception_code_info
except ImportError:
    from replay_core import (
        business_replay_root,
        CHASSIS_AGV_STATE_TOPIC,
        CMD_VEL_CALIB_TOPIC,
        GridState,
        LOCAL_MAP_TOPIC,
        local_map_replay_root,
        LOCATION_CODE_TOPIC,
        MAP_TOPIC,
        NAVIGATION_CODE_TOPIC,
        NAV_FEEDBACK_TOPIC,
        NAV_GOAL_TOPIC,
        NAV_RESULT_TOPIC,
        ODOM_TOPIC,
        PERCEPTION_CODE_TOPIC,
        PoseState,
        ReplayRecord,
        RosMessageFactory,
        iter_records,
        iter_replay_files,
        map_replay_root,
        resolve_replay_root,
    )
    from replay_viewer import build_timeline, collect_replay_records, GoalState, iter_replay_records_merged
    from topics.cmd_vel import parse_cmd_vel
    from topics.chassis_agv_state import parse_agv_state_info
    from topics.local_map import parse_local_map
    from topics.location_code import parse_location_code_info
    from topics.map import parse_map
    from topics.navigation_code import parse_navigation_code_info
    from topics.navigation_action import (
        parse_feedback_state,
        parse_goal,
        parse_navigation_feedback_info,
        parse_navigation_result_info,
        parse_result_state,
    )
    from topics.odom_info import parse_odometry
    from topics.perception_code import parse_perception_code_info


FRONTEND_DIR = Path(__file__).resolve().parent.parent / "frontend"
HTML_PAGE_PATH = FRONTEND_DIR / "index.html"
UPLOAD_ROOT = Path(tempfile.gettempdir()) / "replay_tool_uploads"


def log_replay_server(message: str) -> None:
    print(f"[replay_server] {message}", flush=True)


def load_html_page() -> str:
    return HTML_PAGE_PATH.read_text(encoding="utf-8")


def encode_grid(grid: Optional[GridState]) -> Optional[Dict[str, Any]]:
    if grid is None:
        return None
    rgba = np.clip(grid.image * 255.0, 0, 255).astype(np.uint8)
    return {
        "width": grid.width,
        "height": grid.height,
        "extent": list(grid.extent),
        "corners": [[point[0], point[1]] for point in grid.corners],
        "origin": [grid.origin_x, grid.origin_y],
        "origin_yaw": grid.origin_yaw,
        "resolution": grid.resolution,
        "rgba_b64": base64.b64encode(rgba.tobytes()).decode("ascii"),
    }


def encode_pose(pose: Optional[PoseState]) -> Optional[Dict[str, Any]]:
    if pose is None:
        return None
    return {
        "x": pose.x,
        "y": pose.y,
        "yaw": pose.yaw,
        "linear_speed": pose.linear_speed,
        "angular_speed": pose.angular_speed,
    }


def encode_goal(goal: Optional[GoalState]) -> Optional[Dict[str, Any]]:
    if goal is None:
        return None
    return {
        "task_id": goal.task_id,
        "state_name": goal.state_name,
        "waypoints": [[point[0], point[1], point[2]] for point in goal.waypoints],
    }


def encode_frame(
    time_us: int,
    message_time_us: int,
    pose: Optional[PoseState],
    local_map: Optional[GridState],
    goal: Optional[GoalState],
    location_code: Optional[Dict[str, Any]] = None,
    perception_code: Optional[Dict[str, Any]] = None,
    navigation_code: Optional[Dict[str, Any]] = None,
    agv_state: Optional[Dict[str, Any]] = None,
    nav_feedback: Optional[Dict[str, Any]] = None,
    nav_result: Optional[Dict[str, Any]] = None,
    cmd_vel: Optional[Dict[str, Any]] = None,
) -> Dict[str, Any]:
    return {
        "time_us": time_us,
        "message_time_us": message_time_us,
        "pose": encode_pose(pose),
        "local_map": encode_grid(local_map),
        "goal": encode_goal(goal),
        "location_code": location_code,
        "perception_code": perception_code,
        "navigation_code": navigation_code,
        "agv_state": agv_state,
        "nav_feedback": nav_feedback,
        "nav_result": nav_result,
        "cmd_vel": cmd_vel,
    }


def format_display_time(time_us: int) -> str:
    return datetime.fromtimestamp(time_us / 1_000_000.0).strftime("%Y-%m-%d %H:%M:%S.%f")[:-3]


def build_web_payload(root: Path) -> Dict[str, Any]:
    records = collect_replay_records(root)
    if not records:
        raise RuntimeError(f"No replay records found under: {root}")
    factory = RosMessageFactory()
    timeline = build_timeline(records, factory)
    return build_web_payload_from_timeline(root, timeline)


def build_web_payload_from_timeline(root: Path, timeline) -> Dict[str, Any]:
    if not timeline.times_us:
        raise RuntimeError(f"No visualizable records found under: {root}")

    static_map = next((grid for grid in timeline.maps if grid is not None), None)
    active_indices = [
        index
        for index, time_us in enumerate(timeline.times_us)
        if timeline.poses[index] is not None
        or timeline.local_maps[index] is not None
        or timeline.goals[index] is not None
    ]
    if not active_indices:
        active_indices = list(range(len(timeline.times_us)))
    start_display = format_display_time(timeline.times_us[active_indices[0]])
    end_display = format_display_time(timeline.times_us[active_indices[-1]])
    frames: List[Dict[str, Any]] = []
    for index in active_indices:
        time_us = timeline.times_us[index]
        frames.append(
            {
                "time_us": time_us,
                "pose": encode_pose(timeline.poses[index]),
                "local_map": encode_grid(timeline.local_maps[index]),
                "goal": encode_goal(timeline.goals[index]),
            }
        )
    return {
        "root": str(root),
        "time_span": f"{start_display} -> {end_display}",
        "start_time_us": timeline.times_us[active_indices[0]],
        "end_time_us": timeline.times_us[active_indices[-1]],
        "static_map": encode_grid(static_map),
        "frames": frames,
    }


class ReplayDataLoader:
    def __init__(self, root: Path) -> None:
        self.root = root
        self.resolved_root: Optional[Path] = None
        self._lock = threading.Lock()
        self._thread: Optional[threading.Thread] = None
        self.state = "idle"
        self.message = "Waiting to load replay data..."
        self.error: Optional[str] = None
        self.frames: List[Dict[str, Any]] = []
        self.static_map: Optional[Dict[str, Any]] = None
        self.start_time_us: Optional[int] = None
        self.end_time_us: Optional[int] = None
        self.record_start_time_us: Optional[int] = None
        self.record_end_time_us: Optional[int] = None
        self.complete = False
        self._frame_times_us: List[int] = []
        self._frame_message_times_us: List[int] = []
        self._local_map_records: List[Tuple[int, Path, int]] = []
        self._local_map_record_times_us: List[int] = []
        self._local_map_total = 0
        self._local_map_done = 0

    def ensure_started(self) -> None:
        with self._lock:
            if self._thread is not None:
                return
            self.state = "loading"
            self.message = "Preparing replay package..."
            log_replay_server(f"starting loader thread for root={self.root}")
            self._thread = threading.Thread(target=self._load, name="replay-loader", daemon=True)
            self._thread.start()

    def _load(self) -> None:
        try:
            with self._lock:
                self.message = "Resolving replay input..."
            log_replay_server(f"resolving replay input root={self.root}")
            resolved_root = resolve_replay_root(self.root)
            with self._lock:
                self.resolved_root = resolved_root
                self.message = "Loading latest static map..."
            log_replay_server(f"resolved replay root={resolved_root}")
            self._preload_latest_static_map(resolved_root)
            with self._lock:
                self.message = "Streaming replay frames..."
            log_replay_server(f"starting frame stream for root={resolved_root}")
            factory = RosMessageFactory()
            active_goal: Optional[GoalState] = None
            interested_topics = {
                ODOM_TOPIC,
                LOCATION_CODE_TOPIC,
                PERCEPTION_CODE_TOPIC,
                NAVIGATION_CODE_TOPIC,
                CHASSIS_AGV_STATE_TOPIC,
                CMD_VEL_CALIB_TOPIC,
                NAV_GOAL_TOPIC,
                NAV_FEEDBACK_TOPIC,
                NAV_RESULT_TOPIC,
            }
            business_root = business_replay_root(resolved_root)
            log_replay_server(f"stream business root={business_root}")
            ordered = iter_replay_records_merged([business_root], interested_topics)
            scanned_records = 0
            loaded_any = False

            for scanned_records, record in enumerate(ordered, start=1):
                loaded_any = True
                pose: Optional[PoseState] = None
                global_map: Optional[GridState] = None
                goal_update: Optional[GoalState] = None
                location_code: Optional[Dict[str, Any]] = None
                perception_code: Optional[Dict[str, Any]] = None
                navigation_code: Optional[Dict[str, Any]] = None
                agv_state: Optional[Dict[str, Any]] = None
                nav_feedback: Optional[Dict[str, Any]] = None
                nav_result: Optional[Dict[str, Any]] = None
                cmd_vel: Optional[Dict[str, Any]] = None

                if record.topic == ODOM_TOPIC:
                    pose = parse_odometry(record, factory)
                elif record.topic == MAP_TOPIC:
                    global_map = parse_map(record, factory)
                elif record.topic == LOCATION_CODE_TOPIC:
                    location_code = parse_location_code_info(record)
                elif record.topic == PERCEPTION_CODE_TOPIC:
                    perception_code = parse_perception_code_info(record)
                elif record.topic == NAVIGATION_CODE_TOPIC:
                    navigation_code = parse_navigation_code_info(record, factory)
                elif record.topic == CHASSIS_AGV_STATE_TOPIC:
                    agv_state = parse_agv_state_info(record, factory)
                elif record.topic == CMD_VEL_CALIB_TOPIC:
                    cmd_vel = parse_cmd_vel(record, factory)
                elif record.topic == NAV_GOAL_TOPIC:
                    active_goal = parse_goal(record, factory)
                    goal_update = active_goal
                elif record.topic == NAV_FEEDBACK_TOPIC:
                    task_id, state_name = parse_feedback_state(record)
                    if active_goal is not None and (
                        not task_id or task_id == active_goal.task_id
                    ):
                        active_goal.state_name = state_name
                        goal_update = active_goal
                    nav_feedback = parse_navigation_feedback_info(record, factory)
                elif record.topic == NAV_RESULT_TOPIC:
                    task_id, state_name = parse_result_state(record)
                    if active_goal is not None and (
                        not task_id or task_id == active_goal.task_id
                    ):
                        active_goal.state_name = state_name
                        goal_update = active_goal
                    nav_result = parse_navigation_result_info(record, factory)

                if global_map is not None:
                    with self._lock:
                        if self.static_map is None:
                            self.static_map = encode_grid(global_map)
                    if not any(
                        (
                            pose,
                            goal_update,
                            location_code,
                            perception_code,
                            navigation_code,
                            agv_state,
                            nav_feedback,
                            nav_result,
                            cmd_vel,
                        )
                    ):
                        continue

                if not any(
                    (
                        pose,
                        goal_update,
                        location_code,
                        perception_code,
                        navigation_code,
                        agv_state,
                        nav_feedback,
                        nav_result,
                        cmd_vel,
                    )
                ):
                    if scanned_records % 100 == 0:
                        with self._lock:
                            self.message = (
                                "Streaming replay frames... "
                                f"scanned {scanned_records} records, "
                                f"loaded {len(self.frames)} frames"
                            )
                    continue

                frame = encode_frame(
                    record.time_us,
                    record.message_time_us,
                    pose,
                    None,
                    goal_update,
                    location_code,
                    perception_code,
                    navigation_code,
                    agv_state,
                    nav_feedback,
                    nav_result,
                    cmd_vel,
                )
                with self._lock:
                    self.frames.append(frame)
                    if self.start_time_us is None:
                        self.start_time_us = record.time_us
                    self.end_time_us = record.time_us
                    if self.record_start_time_us is None:
                        self.record_start_time_us = record.record_time_us
                    self.record_end_time_us = record.record_time_us
                    if len(self.frames) % 20 == 0:
                        self.message = (
                            f"Streaming replay frames... loaded {len(self.frames)} frames"
                        )

            if not loaded_any:
                log_replay_server(f"no replay records found under root={self.root}, resolved_root={resolved_root}")
                raise RuntimeError(f"No replay records found under: {self.root}")

            with self._lock:
                self.frames.sort(key=lambda frame: int(frame.get("time_us") or 0))
                self._frame_times_us = [int(frame["time_us"]) for frame in self.frames]
                self._frame_message_times_us = [
                    int(frame.get("message_time_us") or frame["time_us"]) for frame in self.frames
                ]
                if self._frame_times_us:
                    self.start_time_us = self._frame_times_us[0]
                    self.end_time_us = self._frame_times_us[-1]
                self.complete = True
                self.state = "ready"
                self.message = f"Replay data loaded. {len(self.frames)} frames ready."
            log_replay_server(
                f"replay data ready root={resolved_root}, business_frames={len(self.frames)}"
            )
            self._index_local_maps(resolved_root)
        except Exception as exc:
            with self._lock:
                self.state = "error"
                self.error = str(exc)
                self.message = f"Replay loading failed: {exc}"
            log_replay_server(f"loader failed root={self.root}: {exc}")
            traceback.print_exc()

    def _find_frame_index_for_time(self, time_us: int, use_message_time: bool = False) -> Optional[int]:
        with self._lock:
            frame_times_source = self._frame_message_times_us if use_message_time else self._frame_times_us
            if not frame_times_source:
                return None
            frame_times = list(frame_times_source)
        index = bisect.bisect_right(frame_times, time_us) - 1
        if index < 0:
            index = 0
        if index >= len(frame_times):
            index = len(frame_times) - 1
        return index

    def _iter_local_map_refs(self, idx_path: Path) -> List[Tuple[int, int]]:
        refs: List[Tuple[int, int]] = []
        opener = gzip.open if idx_path.suffix == ".gz" else builtins.open
        with opener(idx_path, "rt", encoding="utf-8", errors="replace") as handle:
            for line in handle:
                line = line.strip()
                if not line:
                    continue
                parts = line.split("\t")
                if len(parts) < 2:
                    continue
                try:
                    offset = int(parts[0])
                    time_us = int(parts[1])
                except Exception:
                    continue
                refs.append((offset, time_us))
        return refs

    def _find_local_map_data_path(self, idx_path: Path) -> Optional[Path]:
        if idx_path.suffix == ".gz":
            data_path = idx_path.parent / (idx_path.stem.replace(".idx", ".data") + ".gz")
        else:
            data_path = idx_path.parent / (idx_path.stem + ".data")
        if data_path.exists():
            return data_path
        fallback = Path(str(data_path).replace(".idx", ".data"))
        return fallback if fallback.exists() else None

    def _index_local_maps(self, root: Path) -> None:
        local_map_root = local_map_replay_root(root)
        log_replay_server(f"indexing local_map refs from root={local_map_root}")
        idx_paths = sorted(local_map_root.rglob("*.idx")) + sorted(local_map_root.rglob("*.idx.gz"))
        for idx_path in idx_paths:
            data_path = self._find_local_map_data_path(idx_path)
            if data_path is None:
                continue
            refs = self._iter_local_map_refs(idx_path)
            log_replay_server(
                f"local_map idx={idx_path.name} aligns_by=message_time_us_from_idx"
            )
            for offset, effective_time_us in refs:
                if effective_time_us <= 0:
                    continue
                with self._lock:
                    self._local_map_total += 1
                    self._local_map_records.append((effective_time_us, data_path, offset))
        with self._lock:
            self._local_map_records.sort(key=lambda item: item[0])
            self._local_map_record_times_us = [item[0] for item in self._local_map_records]
        log_replay_server(
            f"local_map index ready root={local_map_root}, refs={len(self._local_map_records)}, "
            f"records={self._local_map_total}"
        )

    def _load_local_map_at_offset(self, data_path: Path, offset: int) -> Optional[Dict[str, Any]]:
        factory = RosMessageFactory()
        handle = gzip.open(data_path, "rb") if data_path.suffix == ".gz" else data_path.open("rb")
        with handle:
            handle.seek(offset)
            header_struct = struct.Struct("<IIqqqQ")
            header_bytes = handle.read(header_struct.size)
            if not header_bytes or len(header_bytes) != header_struct.size:
                return None
            metadata_size, payload_size, record_time_us, message_time_us, _, _ = header_struct.unpack(header_bytes)
            metadata_raw = handle.read(metadata_size)
            payload = handle.read(payload_size)
            if len(metadata_raw) != metadata_size or len(payload) != payload_size:
                return None
            try:
                metadata = json.loads(metadata_raw.decode("utf-8"))
            except Exception:
                return None
            record = ReplayRecord(
                topic=str(metadata.get("topic", "")),
                topic_type=str(metadata.get("topic_type", "")),
                record_time_us=record_time_us,
                message_time_us=message_time_us,
                metadata=metadata,
                payload=payload,
                file_path=data_path,
            )
            try:
                grid = parse_local_map(record, factory)
            except Exception:
                return None
            return encode_grid(grid)

    def get_local_map(self, index: int) -> Dict[str, Any]:
        self.ensure_started()
        with self._lock:
            if index < 0 or index >= len(self.frames):
                return {"index": index, "local_map": None}
            existing = self.frames[index].get("local_map")
            frame_time_us = int(self.frames[index].get("time_us") or 0)
            record_times = list(self._local_map_record_times_us)
            records = list(self._local_map_records)
        if existing is not None:
            return {"index": index, "local_map": existing}
        if not record_times or frame_time_us <= 0:
            return {"index": index, "local_map": None}
        right_pos = bisect.bisect_left(record_times, frame_time_us)
        candidate_positions: List[int] = []
        if right_pos > 0:
            candidate_positions.append(right_pos - 1)
        if right_pos < len(records):
            candidate_positions.append(right_pos)
        if not candidate_positions:
            return {"index": index, "local_map": None}
        nearest_pos = min(
            candidate_positions,
            key=lambda pos: abs(record_times[pos] - frame_time_us),
        )
        _, data_path, offset = records[nearest_pos]
        try:
            encoded = self._load_local_map_at_offset(data_path, offset)
        except Exception as exc:
            log_replay_server(f"local_map load failed index={index}, path={data_path}, offset={offset}: {exc}")
            traceback.print_exc()
            encoded = None
        with self._lock:
            if 0 <= index < len(self.frames):
                self.frames[index]["local_map"] = encoded
                if encoded is not None:
                    self._local_map_done += 1
        return {"index": index, "local_map": encoded}

    def _preload_latest_static_map(self, root: Path) -> None:
        latest_record: Optional[ReplayRecord] = None
        file_count = 0
        for file_path in iter_replay_files(map_replay_root(root)):
            file_count += 1
            for record in iter_records(file_path):
                if record.topic != MAP_TOPIC:
                    continue
                if latest_record is None or record.time_us >= latest_record.time_us:
                    latest_record = record
        log_replay_server(f"scanned {file_count} map files under {map_replay_root(root)}")
        if latest_record is None:
            log_replay_server(f"no static map record found under {map_replay_root(root)}")
            return
        grid = parse_map(latest_record, RosMessageFactory())
        if grid is None:
            log_replay_server(f"failed to parse static map record from {latest_record.file_path}")
            return
        with self._lock:
            self.static_map = encode_grid(grid)
        log_replay_server(f"loaded static map from {latest_record.file_path}")

    def get_status(self) -> Dict[str, Any]:
        self.ensure_started()
        with self._lock:
            return {
                "state": self.state,
                "message": self.message,
                "error": self.error,
                "root": str(self.resolved_root or self.root),
                "available_frames": len(self.frames),
                "complete": self.complete,
                "static_map_available": self.static_map is not None,
                "local_map_total": self._local_map_total,
                "local_map_done": self._local_map_done,
                "time_span": self._format_time_span(),
                "start_time_us": self.start_time_us,
                "end_time_us": self.end_time_us,
                "duration_us": self._duration_us(),
                "record_start_time_us": self.record_start_time_us,
                "record_end_time_us": self.record_end_time_us,
            }

    def get_meta(self) -> Dict[str, Any]:
        self.ensure_started()
        with self._lock:
            return {
                "root": str(self.resolved_root or self.root),
                "available_frames": len(self.frames),
                "complete": self.complete,
                "time_span": self._format_time_span(),
                "start_time_us": self.start_time_us,
                "end_time_us": self.end_time_us,
                "duration_us": self._duration_us(),
                "record_start_time_us": self.record_start_time_us,
                "record_end_time_us": self.record_end_time_us,
                "static_map": self.static_map,
            }

    def get_frames(self, start: int, limit: int) -> List[Dict[str, Any]]:
        self.ensure_started()
        with self._lock:
            end = min(len(self.frames), start + limit)
            return list(self.frames[start:end])

    def _format_time_span(self) -> str:
        if self.start_time_us is None:
            return "Loading..."
        start_display = format_display_time(self.start_time_us)
        if self.end_time_us is None:
            return start_display
        end_display = format_display_time(self.end_time_us)
        if self.complete:
            return f"{start_display} -> {end_display}"
        return f"{start_display} -> {end_display} (loading...)"

    def _duration_us(self) -> Optional[int]:
        if self.start_time_us is None or self.end_time_us is None:
            return None
        return max(0, self.end_time_us - self.start_time_us)


class ReplayRequestHandler(BaseHTTPRequestHandler):
    loader: Optional[ReplayDataLoader] = None
    loader_lock = threading.Lock()

    def do_GET(self) -> None:  # noqa: N802
        parsed = urlparse(self.path)
        if parsed.path == "/":
            self._write_html(load_html_page())
            return
        if parsed.path == "/api/status":
            if type(self).loader is None:
                self._write_json(
                    {
                        "state": "idle",
                        "message": "Waiting to select replay archive...",
                        "error": None,
                        "root": "-",
                        "available_frames": 0,
                        "complete": False,
                        "static_map_available": False,
                        "time_span": "-",
                        "start_time_us": None,
                        "end_time_us": None,
                        "duration_us": None,
                    }
                )
                return
            self._write_json(self._require_loader().get_status())
            return
        if parsed.path == "/api/meta":
            if type(self).loader is None:
                self._write_json(
                    {
                        "root": "-",
                        "available_frames": 0,
                        "complete": False,
                        "time_span": "-",
                        "start_time_us": None,
                        "end_time_us": None,
                        "duration_us": None,
                        "static_map": None,
                    }
                )
                return
            self._write_json(self._require_loader().get_meta())
            return
        if parsed.path == "/api/frames":
            if type(self).loader is None:
                self._write_json({"start": 0, "frames": []})
                return
            params = parse_qs(parsed.query)
            start = int(params.get("start", ["0"])[0])
            limit = int(params.get("limit", ["100"])[0])
            self._write_json(
                {"start": start, "frames": self._require_loader().get_frames(start, limit)}
            )
            return
        if parsed.path == "/api/local-map":
            if type(self).loader is None:
                self._write_json({"index": 0, "local_map": None})
                return
            params = parse_qs(parsed.query)
            index = int(params.get("index", ["0"])[0])
            self._write_json(self._require_loader().get_local_map(index))
            return
        self.send_error(HTTPStatus.NOT_FOUND, "Not found")

    def do_POST(self) -> None:  # noqa: N802
        parsed = urlparse(self.path)
        if parsed.path == "/api/load":
            self._handle_load_upload()
            return
        self.send_error(HTTPStatus.NOT_FOUND, "Not found")

    def log_message(self, format: str, *args) -> None:  # noqa: A003
        return

    def _write_html(self, html: str) -> None:
        body = html.encode("utf-8")
        self.send_response(HTTPStatus.OK)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def _write_json(self, payload: Dict[str, Any]) -> None:
        body = json.dumps(payload).encode("utf-8")
        self.send_response(HTTPStatus.OK)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def _require_loader(self) -> ReplayDataLoader:
        loader = type(self).loader
        if loader is None:
            raise RuntimeError("Replay loader is not initialized")
        return loader

    def _handle_load_upload(self) -> None:
        filename_header = self.headers.get("X-File-Name", "").strip()
        if not filename_header:
            self.send_error(HTTPStatus.BAD_REQUEST, "Missing replay archive file")
            return

        try:
            content_length = int(self.headers.get("Content-Length", "0"))
        except ValueError:
            content_length = 0
        if content_length <= 0:
            self.send_error(HTTPStatus.BAD_REQUEST, "Missing replay archive content")
            return

        filename = Path(unquote(filename_header)).name
        if not filename:
            self.send_error(HTTPStatus.BAD_REQUEST, "Invalid replay archive file name")
            return
        if not (filename.endswith(".tar.xz") or filename.endswith(".txz")):
            self.send_error(HTTPStatus.BAD_REQUEST, "Replay archive must be .tar.xz or .txz")
            return
        log_replay_server(f"receiving upload filename={filename}, content_length={content_length}")

        UPLOAD_ROOT.mkdir(parents=True, exist_ok=True)
        target = UPLOAD_ROOT / filename
        suffix = 1
        while target.exists():
            stem = filename
            if filename.endswith(".tar.xz"):
                stem = filename[: -len(".tar.xz")]
                target = UPLOAD_ROOT / f"{stem}_{suffix}.tar.xz"
            elif filename.endswith(".txz"):
                stem = filename[: -len(".txz")]
                target = UPLOAD_ROOT / f"{stem}_{suffix}.txz"
            suffix += 1

        remaining = content_length
        with target.open("wb") as handle:
            while remaining > 0:
                chunk = self.rfile.read(min(1024 * 1024, remaining))
                if not chunk:
                    break
                handle.write(chunk)
                remaining -= len(chunk)
        if remaining > 0:
            try:
                target.unlink()
            except OSError:
                pass
            self.send_error(HTTPStatus.BAD_REQUEST, "Replay archive upload was incomplete")
            return
        log_replay_server(f"upload complete target={target}, size={content_length}")

        with type(self).loader_lock:
            type(self).loader = ReplayDataLoader(target)
            type(self).loader.ensure_started()
        log_replay_server(f"loader initialized for uploaded archive {target}")

        self._write_json({"ok": True, "root": str(target)})


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Web-based L2 replay viewer")
    parser.add_argument(
        "root",
        nargs="?",
        default="",
        help="Replay root directory or replay archive, e.g. /var/log/robot/l2 or /path/to/l2.tar.xz",
    )
    parser.add_argument("--host", default="0.0.0.0", help="HTTP bind host")
    parser.add_argument("--port", default=8765, type=int, help="HTTP bind port")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    root_arg = (args.root or "").strip()
    if root_arg:
        root = Path(root_arg).expanduser().resolve()
        if root.exists():
            ReplayRequestHandler.loader = ReplayDataLoader(root)
        else:
            print(
                f"Replay input does not exist yet: {root}. "
                "Server will start and wait for an uploaded replay archive."
            )
            ReplayRequestHandler.loader = None
    else:
        ReplayRequestHandler.loader = None
    server = ThreadingHTTPServer((args.host, args.port), ReplayRequestHandler)
    print(f"L2 replay web viewer listening on http://{args.host}:{args.port}")
    print(f"Open from your browser: http://<machine-ip>:{args.port}")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
