cmake_minimum_required(VERSION 3.16)
project(log_agent LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(ZLIB REQUIRED)
find_package(Threads REQUIRED)

add_library(log_agent STATIC
    src/log_agent.cpp
    src/log_agent_scan.cpp
    src/log_agent_recovery.cpp
    src/log_agent_compressor.cpp
    src/log_agent_cleanup.cpp
)

target_include_directories(log_agent
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(log_agent
    PUBLIC
        ZLIB::ZLIB
        Threads::Threads
)

add_executable(log_agent_demo
    examples/demo.cpp
)

target_link_libraries(log_agent_demo
    PRIVATE
        log_agent
)
