set(REFLECTION_APP reflection_app)

add_executable(${REFLECTION_APP}
    enum_reflection.cpp
)
target_include_directories(${REFLECTION_APP} PRIVATE .)

install(TARGETS ${REFLECTION_APP} DESTINATION test)
