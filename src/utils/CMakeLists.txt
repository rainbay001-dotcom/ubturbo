
file(GLOB_RECURSE TURBO_UTILS_SRCS *.cpp *.h)

target_sources(ub_turbo_exec PRIVATE ${TURBO_UTILS_SRCS})