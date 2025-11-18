cmake_minimum_required(VERSION 3.22)
project(UBTurbo VERSION 1.0 LANGUAGES C CXX)

# 默认使用 C11
set(CMAKE_C_STANDARD_DEFAULT 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS NO)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# 默认使用 C++17
set(CMAKE_CXX_STANDARD_DEFAULT 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS NO)

message(STATUS "CMAKE_CXX_STANDARD: c++${CMAKE_CXX_STANDARD}")

set(SRC_DIR ${PROJECT_SOURCE_DIR}/src)
set(RMRS_SRC_DIR ${PROJECT_SOURCE_DIR}/plugins/rmrs/src)
set(3RDPARTY_DIR ${PROJECT_SOURCE_DIR}/3rdparty)

# 安全编译选项控制
set(UBTURBO_COMMON_FLAGS "")
set(SECURE_LD_FLAGS "-Wl,-z,noexecstack -Wl,-z,relro -Wl,-z,now -pie -Wl,--build-id=none")
set(SECURE_C_FLAGS "-fstack-protector-strong")
set(COMMON_FLAGS "-fno-omit-frame-pointer ${SECURE_C_FLAGS} ${SECURE_LD_FLAGS} ${UBTURBO_COMMON_FLAGS}")

if (CMAKE_BUILD_TYPE MATCHES "^[Dd]ebug")
    set(DEBUG_FLAGS "-O0 -ggdb3 -fPIE")
    set(CMAKE_CXX_FLAGS_DEBUG "${COMMON_FLAGS} ${DEBUG_FLAGS} -lrt")
else()
    set(RELEASE_FLAGS "-O2 -D_FORTIFY_SOURCE=2 -DNDEBUG=1 -fPIE -fPIC -s")
    set(CMAKE_CXX_FLAGS_RELEASE "${COMMON_FLAGS} ${RELEASE_FLAGS} -lrt")
endif()


# 设置可执行文件和库的输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 禁用 RPATH
set(CMAKE_SKIP_RPATH ON)

include(GNUInstallDirs)
set(CMAKE_INSTALL_LIBDIR lib)

message(DEBUG "CMAKE_INSTALL_PREFIX: ${CMAKE_INSTALL_PREFIX}")
message(DEBUG "CMAKE_INSTALL_BINDIR: ${CMAKE_INSTALL_BINDIR}")
message(DEBUG "CMAKE_INSTALL_LIBDIR: ${CMAKE_INSTALL_LIBDIR}")
message(DEBUG "CMAKE_INSTALL_INCLUDEDIR: ${CMAKE_INSTALL_INCLUDEDIR}")

set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/build/cmake" ${CMAKE_MODULE_PATH})
include(building)
include(ccache)
include(utils)

# 设置所有共享库的输出目录
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
# 设置所有可执行的输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

message(STATUS "current platform: ${CMAKE_SYSTEM_NAME}")
message(STATUS "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
message(DEBUG "CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}")
message(DEBUG "CXX_FLAGS_DEBUG: ${CXX_FLAGS_DEBUG}")
message(DEBUG "CXX_FLAGS_RELEASE: ${CXX_FLAGS_RELEASE}")
message(DEBUG "CMAKE_CXX_FLAGS_DEBUG: ${CMAKE_CXX_FLAGS_DEBUG}")
message(DEBUG "CMAKE_CXX_FLAGS_RELWITHDEBINFO: ${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
message(DEBUG "CMAKE_CXX_FLAGS_RELEASE: ${CMAKE_CXX_FLAGS_RELEASE}")
message(DEBUG "CMAKE_CXX_FLAGS_MINSIZEREL: ${CMAKE_CXX_FLAGS_MINSIZEREL}")

# 加上构建过程日志的颜色输出
option (FORCE_COLORED_OUTPUT "Always produce ANSI-colored output (GNU/Clang only)." OFF)
if (${FORCE_COLORED_OUTPUT})
    if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
        add_compile_options(-fcolor-diagnostics)
    elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
        add_compile_options(-fdiagnostics-color=always)
    endif ()
else ()
    if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
        add_compile_options(-fdiagnostics-color=auto)
    endif ()
endif ()

# coverage option
option(ENABLE_COVERAGE "Use gcov" OFF)
message(DEBUG ENABLE_COVERAGE=${ENABLE_COVERAGE})
if (ENABLE_COVERAGE)
    setup_coverage()
endif ()

# 全局作用域的头文件定义，不依赖具体模块
include_directories(
        ${PROJECT_SOURCE_DIR}/include   # 外部共享头文件
        ${SRC_DIR}/include              # 内部共享头文件
        ${RMRS_SRC_DIR}/include         # rmrs插件共享头文件
        ${CMAKE_BINARY_DIR}/include     # 自动生成头文件
        ${3RDPARTY_DIR}/libboundscheck/include
        ${3RDPARTY_DIR}/rapidjson/include
)

add_subdirectory(src)
add_subdirectory(plugins/rmrs)

option(BUILD_TESTS "Build tests" OFF)
if (BUILD_TESTS)
    add_subdirectory(test)
endif ()

file(COPY ${PROJECT_SOURCE_DIR}/conf/
        DESTINATION ${CMAKE_BINARY_DIR}/conf
        FILE_PERMISSIONS OWNER_READ OWNER_WRITE
        PATTERN "README.md" EXCLUDE
)

# ------------------------------
# 安装配置（组件化）
# ------------------------------
# 组件 : rmrs
install(
  TARGETS ub_turbo_exec
  DESTINATION bin
  COMPONENT rmrs
)
install(
  DIRECTORY ${CMAKE_BINARY_DIR}/lib/
  DESTINATION lib
  COMPONENT rmrs
  PATTERN "libcli_agent.so" EXCLUDE
  PATTERN "libcli_ubturbo_plugin.so" EXCLUDE
)
install(
  DIRECTORY ${CMAKE_BINARY_DIR}/conf/
  DESTINATION conf
  COMPONENT rmrs
)
install(
  FILES ${PROJECT_SOURCE_DIR}/build/rpm/ubturbo.service
  DESTINATION scripts
  COMPONENT rmrs
)
# 安装脚本文件
install(
  PROGRAMS ${PROJECT_SOURCE_DIR}/build/rpm/cat.sh
  DESTINATION bin
  COMPONENT rmrs
)
# ------------------------------
# CPack 配置
# ------------------------------

file(READ "/etc/openEuler-release" OS_RELEASE_CONTENT)
 
# 匹配 openEuler release 22.03 (LTS-SP1)
string(REGEX MATCH "([0-9]+)\\.([0-9]+).*-?SP-?([0-9]+)" _ "${OS_RELEASE_CONTENT}")
 
# 拼接为 oe2203sp1
set(OS_VERSION "oe${CMAKE_MATCH_1}${CMAKE_MATCH_2}sp${CMAKE_MATCH_3}")
 
message(STATUS "OS_VERSION = ${OS_VERSION}")

set(CPACK_GENERATOR "RPM")
set(CPACK_PACKAGE_NAME "ubturbo")
set(CPACK_PACKAGE_VERSION "1.1.1")
set(CPACK_PACKAGE_RELEASE "1")
set(CPACK_OUTPUT_FILE_PREFIX "${CMAKE_BINARY_DIR}/output")
set(CPACK_PACKAGING_INSTALL_PREFIX "/opt/ubturbo")
set(CPACK_COMPONENTS_ALL rmrs)

# ------------------------------
# 配置 RPM
# ------------------------------
# 指定不同组件的打包规则
set(CPACK_RPM_COMPONENT_INSTALL ON)
set(CPACK_RPM_rmrs_FILE_NAME "ubturbo-rmrs-${CPACK_PACKAGE_VERSION}-${CPACK_PACKAGE_RELEASE}.${OS_VERSION}.aarch64.rpm")

set(CPACK_RPM_PRE_INSTALL_SCRIPT_FILE "${CMAKE_SOURCE_DIR}/build/rpm/pre_install.sh")
set(CPACK_RPM_POST_INSTALL_SCRIPT_FILE "${CMAKE_SOURCE_DIR}/build/rpm/post_install.sh")
set(CPACK_RPM_PRE_UNINSTALL_SCRIPT_FILE "${CMAKE_SOURCE_DIR}/build/rpm/pre_uninstall.sh")
set(CPACK_RPM_POST_UNINSTALL_SCRIPT_FILE "${CMAKE_SOURCE_DIR}/build/rpm/post_uninstall.sh")

include(CPack)