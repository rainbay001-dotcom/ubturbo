file(GLOB_RECURSE SMAP_USER_LOG_SRCS
        ${PROJECT_SOURCE_DIR}/src/user/user_log/*.c
        ${PROJECT_SOURCE_DIR}/src/user/user_log/*.cpp
)
add_library(user_log STATIC ${SMAP_USER_LOG_SRCS})
target_include_directories(user_log PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/ulog/include)
target_include_directories(user_log PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/ulog/src)
target_include_directories(user_log PUBLIC ${PROJECT_3RDPARTY_SRC_DIR}/secure/libboundscheck/include)
TARGET_SPDLOG(user_log)