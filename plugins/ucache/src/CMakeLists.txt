add_library(ucache_os_turbo_plugin SHARED turbo_ucache_plugin.cpp)

set_target_properties(ucache_os_turbo_plugin PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

file(GLOB_RECURSE UCACHE_TURBO *.cpp *.h)
target_sources(ucache_os_turbo_plugin PRIVATE ${UCACHE_TURBO})

target_include_directories(ucache_os_turbo_plugin PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/include
        ${CMAKE_CURRENT_LIST_DIR}
        ${DEPS_DIR}/securec/include
        ${CMAKE_CURRENT_LIST_DIR}/common
        ${CMAKE_CURRENT_LIST_DIR}/task_executor
        ${CMAKE_CURRENT_LIST_DIR}/resource_collector
        ${CMAKE_CURRENT_LIST_DIR}/strategy_executor
        )

file(COPY conf/plugin_turbo_ucache.conf DESTINATION ${CMAKE_BINARY_DIR}/conf FILE_PERMISSIONS OWNER_READ OWNER_WRITE)