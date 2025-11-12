cmake_minimum_required(VERSION 3.14.1)
project(SMAP VERSION 6.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 11)

message(STATUS "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")

set(CMAKE_INSTALL_PREFIX ${PROJECT_BINARY_DIR}/output)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXE_LINKER_FLAGS "-Wl,-z,noexecstack -Wl,-z,relro -Wl,-z,now -pie -Wl,--build-id=none")
set(CMAKE_SKIP_RPATH TRUE)

set(PROJECT_BUILD_PATH ${PROJECT_SOURCE_DIR}/build)
set(OUTPUT_PATH ${PROJECT_SOURCE_DIR}/output)
set(PROJECT_3RDPARTY_SRC_DIR ${PROJECT_SOURCE_DIR}/3rdparty)
set(PROJECT_3RDPARTY_BIN_DIR ${OUTPUT_PATH}/3rdparty)

set(TARGET_INSTALL_DIR ${OUTPUT_PATH}/smap)
set(TARGET_INSTALL_LIB_DIR ${TARGET_INSTALL_DIR}/lib)
set(TARGET_INSTALL_BIN_DIR ${TARGET_INSTALL_DIR}/bin)
set(TARGET_INSTALL_INCLUDE_DIR ${TARGET_INSTALL_DIR}/include)

message(STATUS "3rdparty src dir ${PROJECT_3RDPARTY_SRC_DIR}")
message(STATUS "3rdparty bin dir ${PROJECT_3RDPARTY_BIN_DIR}")
message(STATUS "target install dir ${TARGET_INSTALL_DIR}")

include_directories(
    ${PROJECT_3RDPARTY_SRC_DIR}/secure/libboundscheck/include
)
#################################
# include common cmake modules
#################################
include("cmake/building.cmake")

#################################
# build src
#################################
add_subdirectory(3rdparty)
add_subdirectory(src/user)