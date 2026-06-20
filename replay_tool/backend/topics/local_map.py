from __future__ import annotations

from typing import Optional

try:
    from ..replay_core import (
        GridState,
        ReplayRecord,
        RosMessageFactory,
        deserialize_ros1_local_map,
        local_map_to_image,
    )
except ImportError:
    from replay_core import (
        GridState,
        ReplayRecord,
        RosMessageFactory,
        deserialize_ros1_local_map,
        local_map_to_image,
    )


def parse_local_map(record: ReplayRecord, factory: RosMessageFactory) -> Optional[GridState]:
    message = factory.deserialize(record.topic_type, record.payload)
    if message is None:
        try:
            message = deserialize_ros1_local_map(record.payload)
        except Exception:
            message = None
    if message is None:
        return None
    return local_map_to_image(message)
