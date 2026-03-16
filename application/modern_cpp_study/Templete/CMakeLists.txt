set(TEMPLETE_APP templete_app)

aux_source_directory(. TEMPLETE_APP_SRC_LIST)

add_executable(${TEMPLETE_APP} ${TEMPLETE_APP_SRC_LIST})
target_include_directories(${TEMPLETE_APP} PRIVATE .)

install(TARGETS ${TEMPLETE_APP} DESTINATION test)
