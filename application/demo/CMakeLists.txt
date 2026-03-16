set(DEMO demo)

add_executable(${DEMO}
    src/demo.c
)
target_include_directories(${DEMO} PRIVATE inc)
target_link_libraries(${DEMO} PRIVATE base)

install(TARGETS ${DEMO} DESTINATION ${CMAKE_INSTALL_BINDIR})
