cmake_minimum_required(VERSION 3.16)
project(log_l2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(ZLIB REQUIRED)

add_library(log_l2 STATIC
    src/log_l2.cpp
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

target_link_libraries(log_l2
    PUBLIC
        log_service
        ZLIB::ZLIB
)
