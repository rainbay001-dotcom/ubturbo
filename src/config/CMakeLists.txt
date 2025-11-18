
file(GLOB_RECURSE TURBO_CONF_SRCS *.cpp *.h)

target_sources(ub_turbo_exec PRIVATE ${TURBO_CONF_SRCS})