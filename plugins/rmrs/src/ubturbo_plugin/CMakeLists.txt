cmake_minimum_required(VERSION 3.10)
project(rmrs_ubturbo_plugin)

# 递归查找所有 .cpp 文件（不包含头文件）
file(GLOB RMRS_SRCS 
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/intranode_strategy/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/export/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/export/libvirt/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/export/libvirt_helper/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/export/os_helper/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/export/util/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/common/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/message/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ucache/*.cpp
)

# 创建共享库
add_library(rmrs_ubturbo_plugin SHARED ${RMRS_SRCS})
add_subdirectory(serialization)

# 设置头文件路径（PRIVATE 避免污染其他目标）
target_include_directories(rmrs_ubturbo_plugin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/common
    ${CMAKE_CURRENT_SOURCE_DIR}/common/smap
    ${CMAKE_CURRENT_SOURCE_DIR}/intranode_strategy
    ${CMAKE_CURRENT_SOURCE_DIR}/export
    ${CMAKE_CURRENT_SOURCE_DIR}/export/libvirt
    ${CMAKE_CURRENT_SOURCE_DIR}/export/util
    ${CMAKE_CURRENT_SOURCE_DIR}/export/libvirt_helper
    ${CMAKE_CURRENT_SOURCE_DIR}/export/os_helper
    ${CMAKE_CURRENT_SOURCE_DIR}/ucache
    ${CMAKE_CURRENT_SOURCE_DIR}/serialization
    ${CMAKE_CURRENT_SOURCE_DIR}/smap
    ${CMAKE_CURRENT_SOURCE_DIR}/message
    ${OS_TURBO_INCLUDE_DIR}
)

# 设置库文件输出路径
set_target_properties(rmrs_ubturbo_plugin PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

set(BOUNDSCHECK_LIB /usr/lib64/libboundscheck.so)
target_link_libraries(rmrs_ubturbo_plugin PRIVATE ${BOUNDSCHECK_LIB})

# 复制配置文件，确保所有用户可读
file(COPY conf/plugin_rmrs.conf DESTINATION ${CMAKE_BINARY_DIR}/conf 
     FILE_PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)

