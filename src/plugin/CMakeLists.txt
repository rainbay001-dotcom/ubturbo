
file(GLOB_RECURSE TURBO_PLUGIN_SRCS *.cpp *.h)

target_sources(ub_turbo_exec PRIVATE ${TURBO_PLUGIN_SRCS})