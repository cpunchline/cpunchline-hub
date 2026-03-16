set(LOCKFREE_MPMC_QUEUE_STUDY lockfree_mpmc_queue_study)

add_executable(${LOCKFREE_MPMC_QUEUE_STUDY}
    lockfree_mpmc_queue_study.cpp
)
if(NOT CPUNCHLINE_BUILD_THIRDPARTY)
    target_include_directories(${LOCKFREE_MPMC_QUEUE_STUDY} PRIVATE ${CPUNCHLINE_PRE_BUILT_DIR}/include/lockfree_mpmc_queue)
else()
    target_include_directories(${LOCKFREE_MPMC_QUEUE_STUDY} PRIVATE ${CPUNCHLINE_THIRDPARTY_DIR}/lockfree_mpmc_queue)
endif()
target_compile_options(${LOCKFREE_MPMC_QUEUE_STUDY} PRIVATE "-mcx16")
install(TARGETS ${LOCKFREE_MPMC_QUEUE_STUDY} DESTINATION ${CMAKE_INSTALL_BINDIR})
