
file(GLOB_RECURSE TURBO_MAIN_SRCS *.cpp *.h)

target_sources(ub_turbo_exec PRIVATE ${TURBO_MAIN_SRCS})