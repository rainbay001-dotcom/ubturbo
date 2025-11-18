find_program(Flatc NAMES flatc PATHS ${DEPS_DIR}/flatbuffers/bin NO_DEFAULT_PATH)

# RPC 桩代码生成
macro(gen_rpc_base_msg)
    file(GLOB_RECURSE idl_file rack_base.fbs)
    if (NOT idl_file)
        message(WARNING "[base] No IDL files have been provided!")
    else ()
        message(STATUS "[base] Found IDL files:")
        print_list("${idl_file}")

        set(gen_files "")
        # 生成文件列表
        list(APPEND gen_files
                "${CMAKE_BINARY_DIR}/rpc/rack_base_generated.h"
        )

        # 添加自定义命令来生成文件
        add_custom_command(
                OUTPUT ${gen_files}
                COMMAND mkdir -p ${CMAKE_BINARY_DIR}/rpc
                COMMAND ${Flatc} -o ${CMAKE_BINARY_DIR}/rpc --cpp ${idl_file}
                COMMENT "[base] Generating rpc interface based on IDL files"
                VERBATIM
                DEPENDS ${idl_file}  # 依赖于 IDL 文件
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )

        # 创建一个自定义目标来生成文件
        add_custom_target(gen_rpc_base ALL DEPENDS ${gen_files})
    endif ()
endmacro()


macro(gen_rpc module)
    file(GLOB_RECURSE idl_file rack_${module}.fbs)
    if (NOT idl_file)
        message(WARNING "[${module}] No IDL files have been provided!")
    else ()
        message(STATUS "[${module}] Found IDL files:")
        print_list("${idl_file}")

        set(gen_files "")
        # 生成文件列表
        list(APPEND gen_files
                "${CMAKE_BINARY_DIR}/rpc/rack_${module}_generated.h"
                "${CMAKE_BINARY_DIR}/rpc/rack_${module}.grpc.fb.h"
                "${CMAKE_BINARY_DIR}/rpc/rack_${module}.grpc.fb.cc"
        )

        # 添加自定义命令来生成文件
        add_custom_command(
                OUTPUT ${gen_files}
                COMMAND mkdir -p ${CMAKE_BINARY_DIR}/rpc
                COMMAND ${Flatc} -o ${CMAKE_BINARY_DIR}/rpc --cpp --grpc ${idl_file}
                COMMAND sed -i "s@#include <grpcpp/impl/codegen/proto_utils.h>@@" ${CMAKE_BINARY_DIR}/rpc/rack_${module}.grpc.fb.h
                COMMENT "[${module}] Generating rpc interface based on IDL files"
                VERBATIM
                DEPENDS ${idl_file}  # 依赖于 IDL 文件
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )

        # 创建一个自定义目标来生成文件
        add_custom_target(gen_rpc_${module} ALL DEPENDS ${gen_files})
        add_dependencies(gen_rpc_${module} gen_rpc_base)
    endif ()
endmacro()

macro(add_remote_module module)
    gen_rpc(${module})

    # 生成客户端模块编译目标
    add_library(rack_sdk_${module} STATIC ${REMOTE_DIR}/client/rack_${module}_client.cpp ${REMOTE_DIR}/public/rack_${module}_public.cpp ${CMAKE_BINARY_DIR}/rpc/rack_${module}.grpc.fb.cc)
    target_include_directories(rack_sdk_${module} PUBLIC ${REMOTE_DIR}/include ${REMOTE_DIR} ${REMOTE_DIR}/client ${CMAKE_BINARY_DIR}/rpc)
    target_compile_options(rack_sdk_${module} PUBLIC -fPIC)
    target_link_libraries(rack_sdk_${module} PUBLIC flatbuffers grpc securec)
    add_dependencies(rack_sdk_${module} gen_rpc_${module})
    target_link_libraries(rack_sdk PUBLIC rack_sdk_${module})
    set(module_static_path "$<TARGET_FILE:rack_sdk_${module}>")
    target_link_options(rack_sdk PRIVATE -Wl,--whole-archive ${module_static_path} -Wl,--no-whole-archive )

    # 生成服务端模块编译目标
    add_library(rack_remote_${module}_service STATIC ${REMOTE_DIR}/service/rack_${module}_service.cpp ${CMAKE_BINARY_DIR}/rpc/rack_${module}.grpc.fb.cc)
    target_include_directories(rack_remote_${module}_service PUBLIC ${REMOTE_DIR} ${REMOTE_DIR}/service ${CMAKE_BINARY_DIR}/rpc)
    target_link_libraries(rack_remote_${module}_service PUBLIC flatbuffers grpc)
    add_dependencies(rack_remote_${module}_service gen_rpc_${module})
    target_link_libraries(rack_remote PUBLIC rack_remote_${module}_service)
endmacro()