add_library(TurboLog STATIC rack_logger.cpp rack_logger_manager.cpp rack_logger_filesink.cpp rack_logger_ringbuffer.cpp rack_logger_filter.cpp)
target_include_directories(TurboLog PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    include
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils
)
target_link_libraries(TurboLog PRIVATE)
target_compile_options(TurboLog PRIVATE
        $<$<CONFIG:Release>: -O3>
        $<$<CONFIG:RelWithDebInfo>: -O3>
        $<$<CONFIG:MinSizeRel>: -O3>
)

add_library(turbo_log STATIC turbo_module_logger.cpp)
target_include_directories(turbo_log PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} include)
target_link_libraries(turbo_log PRIVATE TurboLog)
target_compile_options(turbo_log PRIVATE
        $<$<CONFIG:Release>: -O3>
        $<$<CONFIG:RelWithDebInfo>: -O3>
        $<$<CONFIG:MinSizeRel>: -O3>
)