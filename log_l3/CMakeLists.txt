cmake_minimum_required(VERSION 3.16)
project(log_l3 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_library(log_l3 STATIC
    src/logger_module.cpp
    src/log_l3.cpp
)

target_include_directories(log_l3
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(log_l3
    PUBLIC
        log_writer
        log_service
        log_agent
)

add_library(log_l3_c_api SHARED
    src/log_l3_c_api.cpp
)

target_include_directories(log_l3_c_api
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(log_l3_c_api
    PUBLIC
        log_l3
)

add_executable(log_l3_demo
    examples/demo.cpp
)

target_link_libraries(log_l3_demo PRIVATE log_l3)
