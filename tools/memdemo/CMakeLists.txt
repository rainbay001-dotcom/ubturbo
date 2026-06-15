# Standalone build for the memdemo ground-truth generator.
#
# This is intentionally self-contained and is NOT wired into the top-level
# ubturbo build, so it never affects the main program. Build it directly:
#
#   cmake -S tools/memdemo -B tools/memdemo/build
#   cmake --build tools/memdemo/build
#
# Output binary: tools/memdemo/build/memdemo
cmake_minimum_required(VERSION 3.16)
project(memdemo LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_executable(memdemo
    src/main.c
    src/arg_parse.c
    src/mem_alloc.c
    src/access_pattern.c
    src/vm_sim.c
    src/groundtruth.c
)

target_include_directories(memdemo PRIVATE include)
target_compile_options(memdemo PRIVATE -Wall -Wextra -fstack-protector-strong)
target_link_libraries(memdemo PRIVATE m)

# memdemo registers KVM memslots; arm64 is the supported/target arch (matching
# the SMAP kvm_pgtable stage-2 code path).
if(NOT CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
    message(WARNING "memdemo targets arm64 (aarch64); VM mode is only validated there.")
endif()
