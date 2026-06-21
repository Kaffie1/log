cmake_minimum_required(VERSION 3.16)
project(log_l1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_library(log_l1 STATIC
    src/monitor_module.cpp
    src/log_l1.cpp
)

target_include_directories(log_l1
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(log_l1
    PUBLIC
        log_writer
        log_service
        log_agent
)

add_executable(log_l1_demo
    examples/demo.cpp
)

target_link_libraries(log_l1_demo PRIVATE log_l1)
