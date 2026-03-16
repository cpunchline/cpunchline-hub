set(CZMQ_STUDY czmq_study)

if(NOT CPUNCHLINE_BUILD_THIRDPARTY)
    find_package(ZeroMQ REQUIRED)
endif()


add_executable(${CZMQ_STUDY}
    czmq_study.c
)
target_link_libraries(${CZMQ_STUDY} PRIVATE libzmq czmq)
target_link_directories(${CZMQ_STUDY} PRIVATE ${CPUNCHLINE_PRE_BUILT_DIR}/lib)

install(TARGETS ${CZMQ_STUDY} DESTINATION ${CMAKE_INSTALL_BINDIR})
