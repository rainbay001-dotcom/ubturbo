# 收集 ipc/client 目录下的源文件
file(GLOB_RECURSE TURBO_IPC_CLIENT_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/ipc/client/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/ipc/client/*.h)

# 收集 smap/client 目录下的源文件
file(GLOB_RECURSE SMAP_CLIENT_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/smap/client/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/smap/client/*.h)

# 收集 sdk 目录下的源文件
file(GLOB_RECURSE SDK_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/sdk/*.cpp)

# 创建共享库 libubturbo_client.so，包含 ipc/client 和 smap/client 目录下的所有源文件
add_library(ubturbo_client SHARED 
    ${TURBO_IPC_CLIENT_SRCS}  # ipc/client 目录下的源文件
    ${SMAP_CLIENT_SRCS}       # smap/client 目录下的源文件
    ${SDK_SRCS}               # sdk 目录下的源文件
    ${CMAKE_CURRENT_SOURCE_DIR}/smap/smap_handler_msg.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/smap/smap_handler_msg.h
    ${CMAKE_CURRENT_SOURCE_DIR}/log/client/ulog.h
    ${CMAKE_CURRENT_SOURCE_DIR}/log/client/ulog.cpp
)

# 设置 libubturbo_client.so 的输出目录
set_target_properties(ubturbo_client PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    POSITION_INDEPENDENT_CODE ON  # 确保生成的共享库支持 PIE
)

# 为 libubturbo_client.so 设置 include 目录
target_include_directories(ubturbo_client PUBLIC
    ${PROJECT_SOURCE_DIR}/include    # turbo_ipc_client.h 等
    ${CMAKE_CURRENT_LIST_DIR}/smap/   # smap_xxx.h 等
    ${CMAKE_CURRENT_LIST_DIR}/smap/client   # smap_xxx.h 等
    ${CMAKE_CURRENT_LIST_DIR}/log/client
    ${CMAKE_CURRENT_LIST_DIR}/sdk
    ${CMAKE_CURRENT_LIST_DIR}/sdk/include
)

set(BOUNDSCHECK_LIB /usr/lib64/libboundscheck.so)

# 链接库到可执行文件
target_link_libraries(ubturbo_client PRIVATE
    ${BOUNDSCHECK_LIB}
)

# 不需要将 libubturbo_client.so 链接到 ub_turbo_exec，因为它是对外提供的共享库

# 创建 ub_turbo_exec 可执行文件
add_executable(ub_turbo_exec main.cpp)

# 可执行文件默认不导出符号，这里需要导出给 so 使用
target_link_options(ub_turbo_exec PRIVATE "-Wl,-E")

# 设置 ub_turbo_exec 的 include 目录
target_include_directories(ub_turbo_exec PUBLIC
    ${INCLUDE_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/log/include
    ${CMAKE_CURRENT_LIST_DIR}/../include
    ${CMAKE_CURRENT_LIST_DIR}/include
    ${CMAKE_CURRENT_LIST_DIR}/config
    ${CMAKE_CURRENT_LIST_DIR}/plugin
    ${CMAKE_CURRENT_LIST_DIR}/utils
    ${CMAKE_CURRENT_LIST_DIR}/ipc/client
    ${CMAKE_CURRENT_LIST_DIR}/ipc/client
    ${CMAKE_CURRENT_LIST_DIR}/ipc/server
    ${CMAKE_CURRENT_LIST_DIR}/smap
    ${CMAKE_CURRENT_LIST_DIR}/smap/server
)

# 链接库到可执行文件
target_link_libraries(ub_turbo_exec PRIVATE
    turbo_log
    ${BOUNDSCHECK_LIB}
)

# 添加子目录
add_subdirectory(config)
add_subdirectory(plugin)
add_subdirectory(utils)
add_subdirectory(main)
add_subdirectory(log)
add_subdirectory(ipc)
add_subdirectory(smap)