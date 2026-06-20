cmake_minimum_required(VERSION 3.16)
project(log_l2 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(ZLIB REQUIRED)

add_library(log_l2 STATIC
    src/ros_topic.cpp
    src/navigation_action.cpp
    src/business_data.cpp
    src/static_data.cpp
    src/large_data.cpp
    src/operation.cpp
    src/recorder.cpp
)

target_include_directories(log_l2
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
        $<INSTALL_INTERFACE:include>
)

find_package(catkin QUIET COMPONENTS
    roscpp
    rosconsole
    topic_tools
    geometry_msgs
    nav_msgs
    actionlib_msgs
    module_common_msgs
    navigation
    chassis_msgs
)

if(catkin_FOUND)
    target_include_directories(log_l2
        PUBLIC
            ${catkin_INCLUDE_DIRS}
    )
endif()

target_link_libraries(log_l2
    PUBLIC
        log_service
        ZLIB::ZLIB
)

if(catkin_FOUND)
    add_executable(l2_ros1_recorder
        src/main.cpp
        src/ros1_recorder.cpp
    )

    target_include_directories(l2_ros1_recorder PRIVATE
        ${catkin_INCLUDE_DIRS}
    )

    target_link_libraries(l2_ros1_recorder PRIVATE
        log_l2
        log_agent
        ${catkin_LIBRARIES}
    )
else()
    message(WARNING "catkin/ROS1 dependencies were not found; l2_ros1_recorder will not be built.")
endif()
