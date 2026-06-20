FROM ros:noetic-ros-base-focal

ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Etc/UTC
ENV LANG=C.UTF-8
ENV LC_ALL=C.UTF-8
ENV ROS_DISTRO=noetic

SHELL ["/bin/bash", "-lc"]

RUN apt-get update && apt-get install -y --no-install-recommends \
    bash-completion \
    build-essential \
    cmake \
    gdb \
    git \
    iputils-ping \
    nano \
    ninja-build \
    pkg-config \
    python3-catkin-tools \
    python3-rosdep \
    python3-rosinstall \
    python3-rosinstall-generator \
    python3-vcstool \
    ros-noetic-actionlib-msgs \
    ros-noetic-geometry-msgs \
    ros-noetic-nav-msgs \
    ros-noetic-rosconsole \
    ros-noetic-roscpp \
    ros-noetic-topic-tools \
    libfmt-dev \
    libspdlog-dev \
    zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*

RUN rosdep init || true

RUN mkdir -p /workspaces/ros_ws/src /workspaces/log

COPY docker/ros1-noetic/entrypoint.sh /ros_entrypoint.sh
RUN chmod +x /ros_entrypoint.sh

RUN echo "source /opt/ros/noetic/setup.bash" >> /root/.bashrc

WORKDIR /workspaces/log

ENTRYPOINT ["/ros_entrypoint.sh"]
CMD ["bash"]
