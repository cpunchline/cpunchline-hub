set(RANDOM_APP random_app)

add_executable(${RANDOM_APP}
    random.cpp
)
target_include_directories(${RANDOM_APP} PRIVATE .)

install(TARGETS ${RANDOM_APP} DESTINATION test)
