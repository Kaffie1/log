cmake_minimum_required(VERSION 3.16)
project(log_test LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(ZLIB REQUIRED)

add_library(log_test_support STATIC
    l2/tc_support.cpp
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

add_library(log_l3_test_support STATIC
    l3/l3_support.cpp
)

target_include_directories(log_l3_test_support
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(log_l3_test_support
    PUBLIC
        log_l3
        log_service
        ZLIB::ZLIB
)

add_library(log_l1_test_support STATIC
    l1/l1_support.cpp
)

target_include_directories(log_l1_test_support
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(log_l1_test_support
    PUBLIC
        log_l1
        log_service
        ZLIB::ZLIB
)

add_executable(l2_runtime_validation
    l2/l2_runtime_validation.cpp
)

target_include_directories(l2_runtime_validation PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(l2_runtime_validation PRIVATE
    log_l2
    log_agent
)

add_executable(l2_package_cli
    l2/l2_package_cli.cpp
)

target_include_directories(l2_package_cli PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(l2_package_cli PRIVATE
    log_l2
)

set(LOG_L3_TEST_CASE_IDS
    01 02 03 04 05 06 07 08
    09 10 11 12 13 14 15 16
)

set(LOG_L1_TEST_CASE_IDS
    01 02 03 04 05 06 07 08 09 10
    11 12 13 14 15 16 17 18 19 20
)

set(LOG_TEST_CASE_IDS
    01 02 03 04 05 06 07 08 09 10
    11 12 13 14 15 16 17 18 19 20
    21 22 23 24 25 26 27 28 29 30
)

foreach(case_id IN LISTS LOG_TEST_CASE_IDS)
    add_executable(tc${case_id}_test l2/tc${case_id}_test.cpp)
    target_link_libraries(tc${case_id}_test PRIVATE log_test_support)
endforeach()

foreach(case_id IN LISTS LOG_L3_TEST_CASE_IDS)
    add_executable(l3_tc${case_id}_test l3/tc${case_id}_test.cpp)
    target_link_libraries(l3_tc${case_id}_test PRIVATE log_l3_test_support)
endforeach()

foreach(case_id IN LISTS LOG_L1_TEST_CASE_IDS)
    add_executable(l1_tc${case_id}_test l1/tc${case_id}_test.cpp)
    target_link_libraries(l1_tc${case_id}_test PRIVATE log_l1_test_support)
endforeach()
