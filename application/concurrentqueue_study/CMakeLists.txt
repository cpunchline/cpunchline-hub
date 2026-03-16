set(CONCURRENTQUEUE_STUDY concurrentqueue_study)

if(NOT CPUNCHLINE_BUILD_THIRDPARTY)
    find_package(concurrentqueue REQUIRED)
endif()

add_executable(${CONCURRENTQUEUE_STUDY}
    concurrentqueue_study.cpp
)
if(NOT CPUNCHLINE_BUILD_THIRDPARTY)
    target_link_libraries(${CONCURRENTQUEUE_STUDY} PRIVATE Threads::Threads concurrentqueue::concurrentqueue)
else()
    target_include_directories(${CONCURRENTQUEUE_STUDY} PRIVATE ${CPUNCHLINE_PRE_BUILT_DIR}/include/concurrentqueue)
endif()
target_include_directories(${CONCURRENTQUEUE_STUDY} PRIVATE .)

install(TARGETS ${CONCURRENTQUEUE_STUDY} DESTINATION ${CMAKE_INSTALL_BINDIR})
