cmake_minimum_required(VERSION 3.16)
project(log_service LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_library(log_service STATIC
    src/log_service.cpp
)

target_include_directories(log_service
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(log_service
    PUBLIC
        log_writer
)

add_executable(log_service_demo
    examples/demo.cpp
)

target_link_libraries(log_service_demo PRIVATE log_service log_writer)
