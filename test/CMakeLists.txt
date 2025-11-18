cmake_minimum_required(VERSION 3.13)
project(ubturbo_ut  VERSION 1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
message(STATUS, "in test dir...")

add_compile_options(-Wno-pmf-conversions)

set(UBTURBO_ROOT_DIR ${CMAKE_CURRENT_LIST_DIR}/../)
set(CMAKE_CXX_FLAGS " ${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage")
set(CMAKE_C_FLAGS " ${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage ")
add_link_options(-lgcov --coverage)
add_compile_options(-fPIC)
set(CMAKE_EXE_LINKER_FLAGS " -lgcov --coverage ")
set(CMAKE_SHARED_LINKER_FLAGS " -lgcov --coverage ")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
 
add_subdirectory(3rdparty)

set(3RDPARTY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../3rdparty)

# 全局作用域的头文件定义，不依赖具体模块
include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/include              # 内部共享头文件
        ${CMAKE_BINARY_DIR}/include     # 自动生成头文件
        ${3RDPARTY_DIR}/libboundscheck/include
        ${3RDPARTY_DIR}/rapidjson/include
)

# 添加测试cpp文件，这里按照模块划分
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/testcase/demo TESTCASE_DEMO_SRC)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/testcase/config TESTCASE_CONFIG_SRC)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/testcase/log TESTCASE_LOG_SRC)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/testcase/ipc TESTCASE_IPC_SRC)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/testcase/plugin TESTCASE_PLUGIN_SRC)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/testcase/utils TESTCASE_UTILS_SRC)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/testcase/smap/client TESTCASE_SMAP_CLIENT_SRC)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/testcase/smap/server TESTCASE_SMAP_SERVER_SRC)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/testcase/smap/ TESTCASE_SMAP_SRC)


set(TESTCASE_SRC 
            ${TESTCASE_DEMO_SRC}
            ${TESTCASE_CONFIG_SRC}
            ${TESTCASE_LOG_SRC}
            ${TESTCASE_IPC_SRC}
            ${TESTCASE_PLUGIN_SRC}
            ${TESTCASE_UTILS_SRC}
            ${TESTCASE_SMAP_CLIENT_SRC}
            ${TESTCASE_SMAP_SERVER_SRC}
            ${TESTCASE_SMAP_SRC})

# 需要的代码源文件写这里，按照模块划分
file(GLOB UBTURBO_DEMO_FILES ${UBTURBO_ROOT_DIR}/src/demo/*.cpp)
file(GLOB UBTURBO_CONFIG_FILES ${UBTURBO_ROOT_DIR}/src/config/*.cpp)
file(GLOB UBTURBO_LOG_FILES ${UBTURBO_ROOT_DIR}/src/log/*.cpp)
file(GLOB UBTURBO_LOG_CLIENT_FILES ${UBTURBO_ROOT_DIR}/src/log/client/*.cpp)
file(GLOB UBTURBO_IPC_CLIENT_FILES ${UBTURBO_ROOT_DIR}/src/ipc/client/*.cpp)
file(GLOB UBTURBO_IPC_SERVER_FILES ${UBTURBO_ROOT_DIR}/src/ipc/server/*.cpp)
file(GLOB UBTURBO_UTILS_FILES ${UBTURBO_ROOT_DIR}/src/utils/*.cpp)
file(GLOB UBTURBO_PLUGIN_FILES ${UBTURBO_ROOT_DIR}/src/plugin/*.cpp)
file(GLOB UBTURBO_SMAP_CLIENT_FILES ${UBTURBO_ROOT_DIR}/src/smap/client/*.cpp)
file(GLOB UBTURBO_SMAP_SERVER_FILES ${UBTURBO_ROOT_DIR}/src/smap/server/*.cpp)
file(GLOB UBTURBO_SMAP_PUBLIC_FILES ${UBTURBO_ROOT_DIR}/src/smap/*.cpp)


set(UBTURBO_SRC 
            ${UBTURBO_DEMO_FILES}
            ${UBTURBO_CONFIG_FILES}
            ${UBTURBO_LOG_FILES}
            ${UBTURBO_IPC_CLIENT_FILES}
            ${UBTURBO_IPC_SERVER_FILES}
            ${UBTURBO_UTILS_FILES}
            ${UBTURBO_PLUGIN_FILES}
            ${UBTURBO_SMAP_CLIENT_FILES}
            ${UBTURBO_SMAP_SERVER_FILES}
            ${UBTURBO_SMAP_PUBLIC_FILES}
            ${UBTURBO_LOG_CLIENT_FILES})

# MOCK文件加这里，目前将mock目录下递归添加所有cpp，应该只用加文件，不用修改这里
file(GLOB_RECURSE TESTCASE_MOCK_SRC ${CMAKE_CURRENT_SOURCE_DIR}/mock/*.cpp)

# 这里不用改
set(TEST_SRC_UBTURBO ${UBTURBO_SRC} ${TESTCASE_SRC} ${TESTCASE_MOCK_SRC})
add_executable(ubturbo_ut ${TEST_SRC_UBTURBO})

TARGET_MOCKCPP(ubturbo_ut)
TARGET_GTEST(ubturbo_ut)

# 需要的头文件写这里面
target_include_directories(ubturbo_ut BEFORE PUBLIC
    ${UBTURBO_ROOT_DIR}/test/testcase/mock/include
    ${UBTURBO_ROOT_DIR}/src/demo
    ${UBTURBO_ROOT_DIR}/src/log
    ${UBTURBO_ROOT_DIR}/src/log/client
    ${UBTURBO_ROOT_DIR}/src/log/include
    ${UBTURBO_ROOT_DIR}/src/include
    ${UBTURBO_ROOT_DIR}/src/utils
    ${UBTURBO_ROOT_DIR}/src/ipc/client
    ${UBTURBO_ROOT_DIR}/src/ipc/server
    ${UBTURBO_ROOT_DIR}/include
    ${UBTURBO_ROOT_DIR}/src/plugin
    ${UBTURBO_ROOT_DIR}/src/config
    ${UBTURBO_ROOT_DIR}/src/smap
    ${UBTURBO_ROOT_DIR}/src/smap/client
    ${UBTURBO_ROOT_DIR}/src/smap/server
)
 
# 传到源码、测试代码、打桩代码中的宏
target_compile_definitions(ubturbo_ut PUBLIC DT_CONFIG
)

target_link_libraries(ubturbo_ut
      -Wl,--start-group
      pthread dl
      -Wl,--end-group
)

find_library(BOUNDSCHECK_LIB boundscheck)
if(BOUNDSCHECK_LIB)
    target_link_libraries(ubturbo_ut ${BOUNDSCHECK_LIB})
else()
    message(FATAL_ERROR "Not found boundscheck")
endif()

# print compile and link flags
get_target_property(COMPILE_FLAGS ubturbo_ut COMPILE_OPTIONS)
get_target_property(LINK_FLAGS ubturbo_ut LINK_OPTIONS)
message(STATUS "Compiler id: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "Compile flags: ${COMPILE_FLAGS}")
message(STATUS "Link flags: ${LINK_FLAGS}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")


# rmrs插件

macro(FIND_INCLUDE_DIR result curdir)
  file(GLOB_RECURSE children "${curdir}/*.hpp" "${curdir}/*.h")
  set(dirlist "")
  foreach (child ${children})
    string(REGEX REPLACE "(.*)/.*" "\\1" LIB_NAME ${child})
    if (IS_DIRECTORY ${LIB_NAME})
      list(FIND dirlist ${LIB_NAME} list_index)
      if (${list_index} LESS 0)
        LIST(APPEND dirlist ${LIB_NAME})
      endif ()
    endif ()
  endforeach ()
  set(${result} ${dirlist})
endmacro()

set(RMRS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../plugins/rmrs/)

file(GLOB_RECURSE MOCK_UBTURBO_SRC "${CMAKE_CURRENT_SOURCE_DIR}/testcase/rmrs/mock/ubturbo_plugin/*.cpp")

file(GLOB_RECURSE UBTURBO_PLUGIN_COMMON_SRC "${RMRS_DIR}/src/ubturbo_plugin/common/*.cpp")
file(GLOB_RECURSE UBTURBO_PLUGIN_COMMON_SMAP_SRC "${RMRS_DIR}/src/ubturbo_plugin/common/smap/*.cpp")
file(GLOB_RECURSE UBTURBO_PLUGIN_EXPORT_LIBVIRT_SRC "${RMRS_DIR}/src/ubturbo_plugin/export/libvirt/*.cpp")
file(GLOB_RECURSE UBTURBO_PLUGIN_EXPORT_LIBVIRT_HELPER_SRC "${RMRS_DIR}/src/ubturbo_plugin/export/libvirt_helper/*.cpp")
file(GLOB_RECURSE UBTURBO_PLUGIN_EXPORT_OS_HELPER_SRC "${RMRS_DIR}/src/ubturbo_plugin/export/os_helper/*.cpp")
file(GLOB_RECURSE UBTURBO_PLUGIN_EXPORT_UTIL_SRC "${RMRS_DIR}/src/ubturbo_plugin/export/util/*.cpp")
file(GLOB_RECURSE UBTURBO_PLUGIN_EXPORT_SRC "${RMRS_DIR}/src/ubturbo_plugin/export/*.cpp")
file(GLOB_RECURSE UBTURBO_PLUGIN_INTRANODE_SRC "${RMRS_DIR}/src/ubturbo_plugin/intranode_strategy/*.cpp")
file(GLOB_RECURSE UBTURBO_PLUGIN_SERIALIZATION_SRC "${RMRS_DIR}/src/ubturbo_plugin/serialization/*.cpp")
file(GLOB_RECURSE UBTURBO_PLUGIN_SRC "${RMRS_DIR}/src/ubturbo_plugin/*.cpp")
file(GLOB_RECURSE UBTURBO_PLUGIN_MESSAGE_SRC "${RMRS_DIR}/src/ubturbo_plugin/message/*.cpp")
file(GLOB_RECURSE SRC "${RMRS_DIR}/src/*.cpp")

set(MEMPOOLING_SRC

    ${UBTURBO_PLUGIN_COMMON_SRC}
    ${UBTURBO_PLUGIN_COMMON_SMAP_SRC}
    ${UBTURBO_PLUGIN_EXPORT_LIBVIRT_SRC}
    ${UBTURBO_PLUGIN_EXPORT_LIBVIRT_HELPER_SRC}
    ${UBTURBO_PLUGIN_EXPORT_OS_HELPER_SRC}
    ${UBTURBO_PLUGIN_EXPORT_UTIL_SRC}
    ${UBTURBO_PLUGIN_EXPORT_SRC}
    ${UBTURBO_PLUGIN_INTRANODE_SRC}
    ${UBTURBO_PLUGIN_SERIALIZATION_SRC}
    ${UBTURBO_PLUGIN_SRC}
    ${UBTURBO_PLUGIN_MESSAGE_SRC}
    ${MOCK_UBTURBO_SRC}
)

add_library(mempooling_src SHARED
    ${MEMPOOLING_SRC}
)
target_compile_options(mempooling_src PRIVATE -fno-access-control)

FIND_INCLUDE_DIR(INCLUDE ${RMRS_DIR}/src/include)

FIND_INCLUDE_DIR(UBTURBO_INCLUDE ${RMRS_DIR}/src/ubturbo_plugin)
FIND_INCLUDE_DIR(UBTURBO_SUB_INCLUDE ${RMRS_DIR}/src/ubturbo_plugin/include)
FIND_INCLUDE_DIR(UBTURBO_MESSAGE_INCLUDE ${RMRS_DIR}/src/ubturbo_plugin/message)
FIND_INCLUDE_DIR(MOCK_INCLUDE ${CMAKE_CURRENT_SOURCE_DIR}/testcase/mock/include)

target_include_directories(mempooling_src PUBLIC
    ${MOCK_INCLUDE}
    ${INCLUDE}
    ${UBTURBO_INCLUDE}
    ${UBTURBO_SUB_INCLUDE}
    ${UBTURBO_MESSAGE_INCLUDE}

)

set(RMRS_TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/testcase/rmrs)

file(GLOB_RECURSE TEST_MOCK_SRC "${RMRS_TEST_DIR}/mock/*.cpp")
file(GLOB_RECURSE TEST_OS_TURBO_PLUGIN_SRC "${RMRS_TEST_DIR}/ubturbo_plugin/*.cpp")
file(GLOB_RECURSE TEST_OS_TURBO_PLUGIN_MESSAGE_SRC "${RMRS_TEST_DIR}/ubturbo_plugin/message/*.cpp")

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} TEST_RMRS_SRC)
list(APPEND TEST_RMRS_SRC ${TEST_OS_TURBO_PLUGIN_SRC})
list(APPEND TEST_RMRS_SRC ${TEST_OS_TURBO_PLUGIN_MESSAGE_SRC})

list(APPEND TEST_RMRS_SRC ${TEST_MOCK_SRC})

add_executable(rmrs_ut ${TEST_RMRS_SRC})

TARGET_MOCKCPP(rmrs_ut)
TARGET_GTEST(rmrs_ut)

target_compile_options(rmrs_ut PRIVATE -fno-access-control)

target_link_libraries(rmrs_ut mempooling_src)
#target_link_libraries(rmrs_ut /usr/local/lib)
target_link_libraries(rmrs_ut
      -Wl,--start-group
      pthread dl
      -Wl,--end-group
)
set(BOUNDSCHECK_LIB /usr/lib64/libboundscheck.so)
target_link_libraries(rmrs_ut ${BOUNDSCHECK_LIB})

FIND_INCLUDE_DIR(TEST_INCLUDE ${RMRS_DIR}/src/include)
FIND_INCLUDE_DIR(TEST_OS_TURBO_PLUGIN_INCLUDE ${RMRS_DIR}/src/ubturbo_plugin)
FIND_INCLUDE_DIR(TEST_OS_TURBO_PLUGIN_SUB_INCLUDE ${RMRS_DIR}/src/ubturbo_plugin/include)
FIND_INCLUDE_DIR(TEST_OS_TURBO_MESSAGE_PLUGIN_INCLUDE ${RMRS_DIR}/src/ubturbo_plugin/message)

# include headers
target_include_directories(rmrs_ut PUBLIC

    ${TEST_INCLUDE}
    ${TEST_OS_TURBO_PLUGIN_INCLUDE}
    ${TEST_OS_TURBO_PLUGIN_SUB_INCLUDE}
    ${TEST_OS_TURBO_MESSAGE_PLUGIN_INCLUDE}

)

# 传到源码、测试代码、打桩代码中的宏
target_compile_definitions(rmrs_ut PUBLIC
)

# print compile and link flags
get_target_property(COMPILE_FLAGS rmrs_ut COMPILE_OPTIONS)
get_target_property(LINK_FLAGS rmrs_ut LINK_OPTIONS)
message(STATUS "Compiler id: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "Compile flags: ${COMPILE_FLAGS}")
message(STATUS "Link flags: ${LINK_FLAGS}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")