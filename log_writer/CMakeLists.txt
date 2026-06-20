cmake_minimum_required(VERSION 3.16)
project(log_writer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(spdlog REQUIRED)
find_package(fmt REQUIRED)

add_library(log_writer STATIC
    src/error_handler.cpp
    src/log_manager.cpp
    src/logger_registry.cpp
    src/module_registry.cpp
    src/record_builder.cpp
    src/sink_assembler.cpp
    src/level_mapper.cpp
    src/formatter.cpp
)

target_include_directories(log_writer
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_definitions(log_writer PUBLIC SPDLOG_FMT_EXTERNAL)
target_link_libraries(log_writer PUBLIC spdlog::spdlog fmt::fmt)

add_executable(log_writer_demo
    examples/demo.cpp
)

target_link_libraries(log_writer_demo PRIVATE log_writer)
