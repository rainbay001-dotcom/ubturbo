file(GLOB_RECURSE RMRS_SERIALIZATION_SRCS *.cpp *.h
        ${CMAKE_CURRENT_LIST_DIR}/../common/*
        ${CMAKE_CURRENT_LIST_DIR}/../include/*
        include/*)

add_library(rmrs_serialization STATIC ${RMRS_SERIALIZATION_SRCS})
target_include_directories(rmrs_serialization PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/../common
        ${CMAKE_CURRENT_LIST_DIR}/../include
        include
)

target_sources(rmrs_ubturbo_plugin PRIVATE ${RMRS_SERIALIZATION_SRCS})