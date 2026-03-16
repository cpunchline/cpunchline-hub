set(LOCAL_VARIABLE_INIT_APP local_variable_init_app)

add_executable(${LOCAL_VARIABLE_INIT_APP}
    local_variable_init.cpp
)
target_include_directories(${LOCAL_VARIABLE_INIT_APP} PRIVATE .)

install(TARGETS ${LOCAL_VARIABLE_INIT_APP} DESTINATION test)
