cmake_minimum_required(VERSION 3.16)
project(log_cli LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_executable(log_cli
    src/main.cpp
)

target_include_directories(log_cli PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(log_cli PRIVATE
    log_agent
    log_service
)

if(NOT CMAKE_DISABLE_FIND_PACKAGE_catkin)
    target_compile_definitions(log_cli PRIVATE LOG_CLI_ENABLE_L2_PACKAGE=1)
    target_link_libraries(log_cli PRIVATE log_l2)
endif()
