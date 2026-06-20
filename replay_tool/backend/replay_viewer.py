#!/usr/bin/env python3
"""Visualize L2 replay logs with a lightweight RViz-style 2D viewer."""

from __future__ import annotations

import argparse
import heapq
import math
from dataclasses import dataclass, field
from pathlib import Path
from typing import Iterable, List, Optional, Sequence, Tuple

try:
    from .replay_core import (
        business_replay_root,
        CHASSIS_AGV_STATE_TOPIC,
        GoalState,
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
        resolve_replay_root,
    )
    from .topics.local_map import parse_local_map
    from .topics.map import parse_map
    from .topics.navigation_action import (
        parse_feedback_state,
        parse_goal,
        parse_result_state,
    )
    from .topics.odom_info import parse_odometry
except ImportError:
    from replay_core import (
        business_replay_root,
        CHASSIS_AGV_STATE_TOPIC,
        GoalState,
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
        resolve_replay_root,
    )
    from topics.local_map import parse_local_map
    from topics.map import parse_map
    from topics.navigation_action import (
        parse_feedback_state,
        parse_goal,
        parse_result_state,
    )
    from topics.odom_info import parse_odometry


@dataclass
class TimelineState:
    times_us: List[int] = field(default_factory=list)
    poses: List[Optional[PoseState]] = field(default_factory=list)
    maps: List[Optional[GridState]] = field(default_factory=list)
    local_maps: List[Optional[GridState]] = field(default_factory=list)
    goals: List[Optional[GoalState]] = field(default_factory=list)


def build_timeline(records: Sequence[ReplayRecord], factory: RosMessageFactory) -> TimelineState:
    ordered = sorted(records, key=lambda item: item.time_us)
    timeline = TimelineState()
    active_goal: Optional[GoalState] = None

    for record in ordered:
        pose: Optional[PoseState] = None
        global_map: Optional[GridState] = None
        local_map: Optional[GridState] = None
        goal_update: Optional[GoalState] = None

        if record.topic == ODOM_TOPIC:
            pose = parse_odometry(record, factory)
        elif record.topic == MAP_TOPIC:
            global_map = parse_map(record, factory)
        elif record.topic == LOCAL_MAP_TOPIC:
            local_map = parse_local_map(record, factory)
        elif record.topic == NAV_GOAL_TOPIC:
            active_goal = parse_goal(record, factory)
            goal_update = active_goal
        elif record.topic == NAV_FEEDBACK_TOPIC and active_goal is not None:
            task_id, state_name = parse_feedback_state(record)
            if not task_id or task_id == active_goal.task_id:
                active_goal.state_name = state_name
                goal_update = active_goal
        elif record.topic == NAV_RESULT_TOPIC and active_goal is not None:
            task_id, state_name = parse_result_state(record)
            if not task_id or task_id == active_goal.task_id:
                active_goal.state_name = state_name
                goal_update = active_goal

        if not any((pose, global_map, local_map, goal_update)):
            continue

        timeline.times_us.append(record.time_us)
        timeline.poses.append(pose)
        timeline.maps.append(global_map)
        timeline.local_maps.append(local_map)
        timeline.goals.append(goal_update)

    return timeline


class ReplayViewer:
    def __init__(self, timeline: TimelineState) -> None:
        try:
            import matplotlib.pyplot as plt
            from matplotlib.widgets import Button, Slider
        except Exception as exc:  # pragma: no cover - import guard
            raise SystemExit(
                "matplotlib is required for replay_viewer.py. "
                "Please install python3-matplotlib first."
            ) from exc

        self._plt = plt
        if not timeline.times_us:
            raise ValueError("No visualizable records found in replay logs.")
        self.timeline = timeline
        self.index = 0
        self.playing = False

        self.latest_pose: Optional[PoseState] = None
        self.latest_map: Optional[GridState] = None
        self.latest_local_map: Optional[GridState] = None
        self.latest_goal: Optional[GoalState] = None
        self.traj_x: List[float] = []
        self.traj_y: List[float] = []

        self.figure, self.ax = plt.subplots(figsize=(14, 9))
        plt.subplots_adjust(left=0.08, right=0.98, top=0.95, bottom=0.2)

        self.ax.set_title("L2 Replay Viewer")
        self.ax.set_xlabel("X / m")
        self.ax.set_ylabel("Y / m")
        self.ax.set_aspect("equal", adjustable="box")
        self.ax.grid(True, color="#dddddd", linewidth=0.5)

        self.map_artist = None
        self.local_map_artist = None
        (self.path_artist,) = self.ax.plot([], [], color="#1f77b4", linewidth=2.0)
        (self.pose_artist,) = self.ax.plot([], [], marker=(3, 0, 0), markersize=14, color="#2ca02c")
        (self.goal_path_artist,) = self.ax.plot([], [], "--", color="#ff7f0e", linewidth=2.0)
        (self.goal_points_artist,) = self.ax.plot([], [], "o", color="#d62728", markersize=6)
        self.status_text = self.ax.text(
            0.01,
            0.99,
            "",
            transform=self.ax.transAxes,
            va="top",
            ha="left",
            bbox={"facecolor": "white", "alpha": 0.8, "edgecolor": "#cccccc"},
            fontsize=10,
        )

        slider_ax = self.figure.add_axes([0.14, 0.09, 0.72, 0.03])
        self.slider = Slider(
            slider_ax,
            "Time",
            0,
            len(self.timeline.times_us) - 1,
            valinit=0,
            valstep=1,
        )
        self.slider.on_changed(self._on_slider_changed)

        play_ax = self.figure.add_axes([0.14, 0.03, 0.10, 0.04])
        prev_ax = self.figure.add_axes([0.26, 0.03, 0.10, 0.04])
        next_ax = self.figure.add_axes([0.38, 0.03, 0.10, 0.04])
        reset_ax = self.figure.add_axes([0.50, 0.03, 0.10, 0.04])

        self.play_button = Button(play_ax, "Play/Pause")
        self.prev_button = Button(prev_ax, "Prev")
        self.next_button = Button(next_ax, "Next")
        self.reset_button = Button(reset_ax, "Reset")

        self.play_button.on_clicked(self._toggle_play)
        self.prev_button.on_clicked(lambda _event: self.step(-1))
        self.next_button.on_clicked(lambda _event: self.step(1))
        self.reset_button.on_clicked(lambda _event: self.set_index(0))

        self.figure.canvas.mpl_connect("key_press_event", self._on_key_press)
        self.timer = self.figure.canvas.new_timer(interval=80)
        self.timer.add_callback(self._on_timer)
        self.timer.start()

        self.set_index(0, force=True)

    def _on_slider_changed(self, value: float) -> None:
        self.set_index(int(value))

    def _toggle_play(self, _event) -> None:
        self.playing = not self.playing

    def _on_key_press(self, event) -> None:
        if event.key == " ":
            self.playing = not self.playing
        elif event.key == "right":
            self.step(1)
        elif event.key == "left":
            self.step(-1)
        elif event.key == "home":
            self.set_index(0)
        elif event.key == "end":
            self.set_index(len(self.timeline.times_us) - 1)

    def _on_timer(self) -> None:
        if self.playing:
            self.step(1)

    def step(self, delta: int) -> None:
        next_index = max(0, min(len(self.timeline.times_us) - 1, self.index + delta))
        self.set_index(next_index)

    def set_index(self, index: int, force: bool = False) -> None:
        if index == self.index and not force:
            return
        self.index = index
        self.slider.eventson = False
        self.slider.set_val(index)
        self.slider.eventson = True
        self._rebuild_state_up_to(index)
        self._render()

    def _rebuild_state_up_to(self, index: int) -> None:
        self.latest_pose = None
        self.latest_map = None
        self.latest_local_map = None
        self.latest_goal = None
        self.traj_x = []
        self.traj_y = []

        for item in range(index + 1):
            pose = self.timeline.poses[item]
            if pose is not None:
                self.latest_pose = pose
                self.traj_x.append(pose.x)
                self.traj_y.append(pose.y)
            grid = self.timeline.maps[item]
            if grid is not None:
                self.latest_map = grid
            local_grid = self.timeline.local_maps[item]
            if local_grid is not None:
                self.latest_local_map = local_grid
            goal = self.timeline.goals[item]
            if goal is not None:
                self.latest_goal = GoalState(
                    task_id=goal.task_id,
                    waypoints=list(goal.waypoints),
                    state_name=goal.state_name,
                )

    def _render_grid(self, artist_name: str, grid: Optional[GridState], zorder: int) -> None:
        current_artist = getattr(self, artist_name)
        if current_artist is not None:
            current_artist.remove()
            setattr(self, artist_name, None)
        if grid is None:
            return
        artist = self.ax.imshow(
            grid.image,
            extent=grid.extent,
            origin="lower",
            interpolation="nearest",
            zorder=zorder,
        )
        setattr(self, artist_name, artist)

    def _render(self) -> None:
        self._render_grid("map_artist", self.latest_map, zorder=0)
        self._render_grid("local_map_artist", self.latest_local_map, zorder=1)
        self.path_artist.set_data(self.traj_x, self.traj_y)

        if self.latest_pose is not None:
            angle_deg = math.degrees(self.latest_pose.yaw) - 90.0
            self.pose_artist.set_data([self.latest_pose.x], [self.latest_pose.y])
            self.pose_artist.set_marker((3, 0, angle_deg))
        else:
            self.pose_artist.set_data([], [])

        if self.latest_goal is not None and self.latest_goal.waypoints:
            goal_x = [point[0] for point in self.latest_goal.waypoints]
            goal_y = [point[1] for point in self.latest_goal.waypoints]
            self.goal_path_artist.set_data(goal_x, goal_y)
            self.goal_points_artist.set_data(goal_x, goal_y)
        else:
            self.goal_path_artist.set_data([], [])
            self.goal_points_artist.set_data([], [])

        if self.latest_pose is not None:
            margin = 8.0
            self.ax.set_xlim(self.latest_pose.x - margin, self.latest_pose.x + margin)
            self.ax.set_ylim(self.latest_pose.y - margin, self.latest_pose.y + margin)

        current_time = self.timeline.times_us[self.index] / 1_000_000.0
        goal_line = "None"
        if self.latest_goal is not None:
            goal_line = f"{self.latest_goal.task_id or '-'} / {self.latest_goal.state_name}"
        pose_line = "No odometry"
        if self.latest_pose is not None:
            pose_line = (
                f"x={self.latest_pose.x:.2f}  y={self.latest_pose.y:.2f}  "
                f"yaw={math.degrees(self.latest_pose.yaw):.1f}deg  "
                f"v={self.latest_pose.linear_speed:.2f}m/s"
            )
        self.status_text.set_text(
            f"time={current_time:.3f}s\n"
            f"frame={self.index + 1}/{len(self.timeline.times_us)}\n"
            f"pose={pose_line}\n"
            f"goal={goal_line}"
        )
        self.figure.canvas.draw_idle()

    def show(self) -> None:
        self._plt.show()


def collect_replay_records(root: Path) -> List[ReplayRecord]:
    interested_topics = {
        ODOM_TOPIC,
        MAP_TOPIC,
        LOCAL_MAP_TOPIC,
        LOCATION_CODE_TOPIC,
        PERCEPTION_CODE_TOPIC,
        NAVIGATION_CODE_TOPIC,
        CHASSIS_AGV_STATE_TOPIC,
        NAV_GOAL_TOPIC,
        NAV_FEEDBACK_TOPIC,
        NAV_RESULT_TOPIC,
    }
    roots = [business_replay_root(root)]
    local_map_root = local_map_replay_root(root)
    if local_map_root not in roots:
        roots.append(local_map_root)
    return list(iter_replay_records_merged(roots, interested_topics))


def iter_replay_records_merged(
    roots: Path | Sequence[Path], interested_topics: Optional[set[str]] = None
) -> Iterable[ReplayRecord]:
    heap: List[Tuple[int, int, ReplayRecord, Iterable[ReplayRecord]]] = []
    next_id = 0

    if isinstance(roots, Path):
        replay_roots: Sequence[Path] = [roots]
    else:
        replay_roots = roots

    seen_files = set()
    for replay_root in replay_roots:
        for file_path in iter_replay_files(replay_root):
            if file_path in seen_files:
                continue
            seen_files.add(file_path)
            iterator = (
                record
                for record in iter_records(file_path)
                if interested_topics is None or record.topic in interested_topics
            )
            try:
                first_record = next(iterator)
            except StopIteration:
                continue
            heapq.heappush(
                heap, (first_record.time_us, next_id, first_record, iterator)
            )
            next_id += 1

    while heap:
        _, _, record, iterator = heapq.heappop(heap)
        yield record
        try:
            next_record = next(iterator)
        except StopIteration:
            continue
        heapq.heappush(heap, (next_record.time_us, next_id, next_record, iterator))
        next_id += 1


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="L2 replay visualizer")
    parser.add_argument(
        "root",
        help="Replay root directory or unified topic log file, e.g. /var/log/robot/l2 or /var/log/robot/l2_topic_record.log",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    root = resolve_replay_root(Path(args.root))
    if not root.exists():
        raise SystemExit(f"Replay root does not exist: {root}")

    records = collect_replay_records(root)
    if not records:
        raise SystemExit(f"No replay records found under: {root}")

    factory = RosMessageFactory()
    timeline = build_timeline(records, factory)
    viewer = ReplayViewer(timeline)
    viewer.show()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
