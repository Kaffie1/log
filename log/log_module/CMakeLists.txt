cmake_minimum_required(VERSION 3.16)
project(log_module LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(LOG_MODULE_ENABLE_L1 "Build L1 log layer" ON)
option(LOG_MODULE_ENABLE_L2 "Build L2 log layer" ON)
option(LOG_MODULE_ENABLE_L3 "Build L3 log layer" ON)
option(LOG_MODULE_BUILD_L2_ROS1_RECORDER "Build ROS1 L2 recorder process" OFF)

find_package(spdlog REQUIRED)
find_package(fmt REQUIRED)
find_package(ZLIB REQUIRED)
include(CMakePackageConfigHelpers)

set(LOG_MODULE_SHARED_LIB_INSTALL_DIR "/opt/zj_humanoid")
set(LOG_MODULE_RUNTIME_RPATH
    "${LOG_MODULE_SHARED_LIB_INSTALL_DIR};/opt/ros/noetic/lib")

set(LOG_MODULE_SOURCES
    common/src/async_dispatcher.cpp
    common/src/file_lifecycle.cpp
    common/src/log_api.cpp
    common/src/log_config.cpp
    common/src/query_service.cpp
)

set(LOG_MODULE_PRIVATE_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}/l1_log/include
    ${CMAKE_CURRENT_SOURCE_DIR}/l2_log/include
    ${CMAKE_CURRENT_SOURCE_DIR}/l3_log/include
)

set(LOG_MODULE_PUBLIC_INCLUDES
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/common/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include/log_module>
)

if(LOG_MODULE_ENABLE_L1)
    list(APPEND LOG_MODULE_SOURCES
        l1_log/src/log.cpp
    )
endif()

if(LOG_MODULE_ENABLE_L2)
    list(APPEND LOG_MODULE_SOURCES
        l2_log/src/navigation_action.cpp
        l2_log/src/operation.cpp
        l2_log/src/recorder.cpp
        l2_log/src/ros_topic.cpp
        l2_log/src/sensor_data.cpp
        l2_log/src/business_data.cpp
        l2_log/src/topic_input.cpp
        l2_log/src/log.cpp
    )
endif()

if(LOG_MODULE_ENABLE_L3)
    list(APPEND LOG_MODULE_SOURCES
        l3_log/src/log.cpp
    )
endif()

add_library(log_module SHARED ${LOG_MODULE_SOURCES})

target_include_directories(log_module
    PUBLIC
        ${LOG_MODULE_PUBLIC_INCLUDES}
    PRIVATE
        ${LOG_MODULE_PRIVATE_INCLUDES}
)
target_compile_definitions(log_module PUBLIC SPDLOG_FMT_EXTERNAL)
target_link_libraries(log_module PUBLIC spdlog::spdlog fmt::fmt ZLIB::ZLIB)
add_library(log_module::log_module ALIAS log_module)
set_target_properties(log_module PROPERTIES
    OUTPUT_NAME "log_module"
    VERSION 1.0.0
    SOVERSION 1
    INSTALL_RPATH "${LOG_MODULE_RUNTIME_RPATH}"
)

add_executable(log_cli tools/log_cli.cpp)
target_link_libraries(log_cli PRIVATE log_module)
set_target_properties(log_cli PROPERTIES
    INSTALL_RPATH "${LOG_MODULE_RUNTIME_RPATH}"
)

add_executable(trace_cli tools/trace_cli.cpp)
target_link_libraries(trace_cli PRIVATE log_module)
set_target_properties(trace_cli PROPERTIES
    INSTALL_RPATH "${LOG_MODULE_RUNTIME_RPATH}"
)

add_executable(l2_package_cli tools/l2_package_cli.cpp)
target_link_libraries(l2_package_cli PRIVATE log_module)
set_target_properties(l2_package_cli PROPERTIES
    INSTALL_RPATH "${LOG_MODULE_RUNTIME_RPATH}"
)

add_executable(log_cleanup_cli tools/log_cleanup_cli.cpp)
set_target_properties(log_cleanup_cli PROPERTIES
    INSTALL_RPATH "${LOG_MODULE_RUNTIME_RPATH}"
)

if(LOG_MODULE_BUILD_L2_ROS1_RECORDER AND LOG_MODULE_ENABLE_L2)
    find_package(catkin QUIET COMPONENTS
        roscpp
        rosconsole
        topic_tools
        geometry_msgs
        nav_msgs
        actionlib_msgs
        module_common_msgs
        navigation
    )
    if(catkin_FOUND)
        add_executable(l2_ros1_recorder
            l2_log/src/ros1_recorder.cpp
            tools/l2_ros1_recorder_main.cpp
        )
        target_include_directories(l2_ros1_recorder PRIVATE
            ${LOG_MODULE_PRIVATE_INCLUDES}
            ${catkin_INCLUDE_DIRS}
        )
        target_link_libraries(l2_ros1_recorder PRIVATE
            log_module
            ${catkin_LIBRARIES}
        )
        set_target_properties(l2_ros1_recorder PROPERTIES
            INSTALL_RPATH "${LOG_MODULE_RUNTIME_RPATH}"
        )
        install(TARGETS l2_ros1_recorder
            RUNTIME DESTINATION bin
        )
    else()
        message(WARNING "LOG_MODULE_BUILD_L2_ROS1_RECORDER=ON but catkin/ROS1 dependencies were not found; l2_ros1_recorder will not be built.")
    endif()
endif()

install(TARGETS log_module log_cli trace_cli l2_package_cli log_cleanup_cli
    EXPORT log_moduleTargets
    LIBRARY DESTINATION ${LOG_MODULE_SHARED_LIB_INSTALL_DIR}
    ARCHIVE DESTINATION ${LOG_MODULE_SHARED_LIB_INSTALL_DIR}
    RUNTIME DESTINATION bin
)

# Some target systems did not preserve installed symlinks reliably when the deb
# was generated via checkinstall. Install concrete compatibility copies so the
# runtime loader can always resolve liblog_module.so.1 / liblog_module.so.
install(CODE [[
    file(REMOVE "/opt/zj_humanoid/liblog_module.so.1")
    file(REMOVE "/opt/zj_humanoid/liblog_module.so")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "/opt/zj_humanoid/liblog_module.so.1.0.0"
                "/opt/zj_humanoid/liblog_module.so.1")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "/opt/zj_humanoid/liblog_module.so.1.0.0"
                "/opt/zj_humanoid/liblog_module.so")
]])
install(FILES common/include/log_types.hpp
    DESTINATION include/log_module/common/include
)
install(DIRECTORY l1_log/include/
    DESTINATION include/log_module/l1_log/include
    FILES_MATCHING PATTERN "*.hpp"
)
install(DIRECTORY l2_log/include/
    DESTINATION include/log_module/l2_log/include
    FILES_MATCHING PATTERN "*.hpp"
)
install(DIRECTORY l3_log/include/
    DESTINATION include/log_module/l3_log/include
    FILES_MATCHING PATTERN "*.hpp"
)
install(FILES config/log.yaml DESTINATION share/log_module/config)

install(EXPORT log_moduleTargets
    FILE log_moduleTargets.cmake
    NAMESPACE log_module::
    DESTINATION lib/cmake/log_module
)

configure_package_config_file(
    cmake/log_moduleConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/log_moduleConfig.cmake
    INSTALL_DESTINATION lib/cmake/log_module
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/log_moduleConfigVersion.cmake
    VERSION 1.0.0
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/log_moduleConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/log_moduleConfigVersion.cmake
    DESTINATION lib/cmake/log_module
)
