file(GLOB_RECURSE TURBO_IPC_SERVER_SRCS *.cpp *.h)
add_library(ubturbo_ipc_server STATIC ${TURBO_IPC_SERVER_SRCS})
target_sources(ub_turbo_exec PRIVATE ${TURBO_IPC_SERVER_SRCS})