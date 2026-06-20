cmake_minimum_required(VERSION 3.16)
project(log_test LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(ZLIB REQUIRED)

add_library(log_test_support STATIC
    tc_support.cpp
)

target_include_directories(log_test_support
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(log_test_support
    PUBLIC
        log_l2
        log_agent
        ZLIB::ZLIB
)

add_executable(l2_runtime_validation
    l2_runtime_validation.cpp
)

target_include_directories(l2_runtime_validation PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(l2_runtime_validation PRIVATE
    log_l2
    log_agent
)

add_executable(l2_package_cli
    l2_package_cli.cpp
)

target_include_directories(l2_package_cli PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(l2_package_cli PRIVATE
    log_l2
)

set(LOG_TEST_CASE_IDS
    01 02 03 04 05 06 07 08 09 10
    11 12 13 14 15 16 17 18 19 20
    21 22 23 24 25 26 27 28 29 30
)

foreach(case_id IN LISTS LOG_TEST_CASE_IDS)
    add_executable(tc${case_id}_test tc${case_id}_test.cpp)
    target_link_libraries(tc${case_id}_test PRIVATE log_test_support)
endforeach()
