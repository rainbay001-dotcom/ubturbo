set(USER_SRC ".")

include_directories(
    ${USER_SRC}/
    ${CMAKE_CURRENT_SOURCE_DIR}/user_log/smap_user_log
)

aux_source_directory(${USER_SRC} LIBS_SRCS)
aux_source_directory(${USER_SRC}/manage LIBS_SRCS)
aux_source_directory(${USER_SRC}/strategy LIBS_SRCS)
aux_source_directory(${USER_SRC}/advanced-strategy LIBS_SRCS)

add_subdirectory(user_log)

add_library(smap SHARED ${LIBS_SRCS})

find_library(BOUNDSCHECK_LIBRARY boundscheck)
if(BOUNDSCHECK_LIBRARY)
    target_link_libraries(smap
        pthread
        user_log
        ${BOUNDSCHECK_LIBRARY}
    )
else()
    message(FATAL_ERROR "Not found boundscheck")
endif()

install(TARGETS smap DESTINATION ${TARGET_INSTALL_LIB_DIR}
        PERMISSIONS ${SMAP_INSTALL_LIB_PERMISSIONS})
install(FILES smap_interface.h DESTINATION ${TARGET_INSTALL_INCLUDE_DIR}
        PERMISSIONS ${SMAP_INSTALL_LIB_PERMISSIONS})
message("USER_SRC: " ${USER_SRC})
message("Tiering path: " ${KERNEL_SRC})