if(NOT DEFINED CMAKE_BUILD_TYPE OR x${CMAKE_BUILD_TYPE} STREQUAL "x" OR ${CMAKE_BUILD_TYPE} STREQUAL "Debug")
    set(CMAKE_BUILD_TYPE Debug)
else()
    set(CMAKE_BUILD_TYPE Release)
    add_compile_definitions(RELEASE)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 7.3)
        message(FATAL_ERROR "Compiler version is too old (< 7.3). Need at least 7.3")
        set(MF_CXX_STANDARD "-std=c++11")
    else()
        message(STATUS "Compiler version is compatible (>= 7.3)")
        set(MF_CXX_STANDARD "-std=c++17")
        message(STATUS "use c++17")
    endif()
else()
    message(FATAL_ERROR "Not using GNU C++ compiler")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(HUAWEI_SECURE_BUILD_FLAGS "-fstack-protector-strong -Wl,-z,noexecstack -Wl,-z,relro -Wl,-z,now -fPIE -fPIC")
else()
    set(HUAWEI_SECURE_BUILD_FLAGS "-fstack-protector-strong -Wl,-z,noexecstack -Wl,-z,relro -Wl,-z,now -fPIE -fPIC -s")
endif()
set(C_COMMON_FLAGS "${C_COMMON_FLAGS} -fno-omit-frame-pointer ${HUAWEI_SECURE_BUILD_FLAGS}")
set(CXX_COMMON_FLAGS "${CXX_COMMON_FLAGS} ${MF_CXX_STANDARD} -fno-omit-frame-pointer ${HUAWEI_SECURE_BUILD_FLAGS}")

set(CMAKE_C_FLAGS_DEBUG "${C_COMMON_FLAGS} -O0 -ggdb3 -g -lrt -ldl -lm -DDEBUG -DENV_HCCS")
set(CMAKE_C_FLAGS_RELEASE "${C_COMMON_FLAGS} -O2 -D_FORTIFY_SOURCE=2 -g -DNDEBUG=1 -lrt -ldl -lm -DENV_HCCS")

set(CMAKE_CXX_FLAGS_DEBUG "${CXX_COMMON_FLAGS} -O0 -ggdb3 -g -lrt -ldl")
set(CMAKE_CXX_FLAGS_RELEASE "${CXX_COMMON_FLAGS} -O2 -D_FORTIFY_SOURCE=2 -g -DNDEBUG=1 -lrt -ldl")

SET(SMAP_INSTALL_LIB_PERMISSIONS
        OWNER_READ OWNER_WRITE OWNER_EXECUTE
        GROUP_READ GROUP_EXECUTE
        WORLD_READ WORLD_EXECUTE)

SET(SMAP_INSTALL_EXE_PERMISSIONS
        OWNER_READ OWNER_WRITE OWNER_EXECUTE
        GROUP_READ GROUP_EXECUTE
        WORLD_READ WORLD_EXECUTE)
