#!/usr/bin/env bash
set -e

source /opt/ros/noetic/setup.bash

if [ -f /workspaces/ros_ws/devel/setup.bash ]; then
    source /workspaces/ros_ws/devel/setup.bash
fi

exec "$@"
