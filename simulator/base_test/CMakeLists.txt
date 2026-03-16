set(ARENA_TEST arena_test)
set(AVL_TEST avl_test)
set(AVL_UID_TEST avl_uid_test)
set(DARRAY_TEST darray_test)
set(DEBUG_BACKTRACE_TEST debug_backtrace_test)
set(DEQUE_TEST deque_test)
set(FSM_TEST fsm_test)
set(HEAP_TEST heap_test)
set(LIST_TEST list_test)
set(LRUCACHE_TEST LRUCache_test)
set(LSTACK_TEST lstack_test)
set(OBJECTPOOL_C_TEST objectpool_test)
set(OBJECTPOOL_TEST ObjectPool_test)
set(RBTREE_TEST rbtree_test)
set(RINGBUFFER_TEST ringbuffer_test)
set(SHM_TEST shm_test)
set(SINGLETON_TEST Singleton_test)
set(THREAD_TEST Thread_test)
set(THREADPOOL_C_TEST threadpool_test)
set(THREADPOOL_CPP_TEST ThreadPool_test)
set(THREADQUEUE_C_TEST threadqueue_test)
set(THREADQUEUE_CPP_TEST ThreadQueue_test)
set(TIMERCLUSTER_C_TEST timercluster_test)
set(TIMERCLUSTER_CPP_TEST TimerCluster_test)
set(XSTRING_TEST xstring_test)
set(TEST_TEST test_test)

add_executable(${TEST_TEST} ${TEST_TEST}.c)
target_link_libraries(${TEST_TEST} PRIVATE base)

add_executable(${ARENA_TEST} ${ARENA_TEST}.c)
target_link_libraries(${ARENA_TEST} PRIVATE base)

add_executable(${AVL_TEST} ${AVL_TEST}.c)
target_link_libraries(${AVL_TEST} PRIVATE base)

add_executable(${AVL_UID_TEST} ${AVL_UID_TEST}.c)
target_link_libraries(${AVL_UID_TEST} PRIVATE base)

add_executable(${DARRAY_TEST} ${DARRAY_TEST}.c)
target_link_libraries(${DARRAY_TEST} PRIVATE base)

add_executable(${DEBUG_BACKTRACE_TEST} ${DEBUG_BACKTRACE_TEST}.c)
target_link_libraries(${DEBUG_BACKTRACE_TEST} PRIVATE base)

add_executable(${DEQUE_TEST} ${DEQUE_TEST}.c)
target_link_libraries(${DEQUE_TEST} PRIVATE base)

add_executable(${FSM_TEST} ${FSM_TEST}.c)
target_link_libraries(${FSM_TEST} PRIVATE base)

add_executable(${HEAP_TEST} ${HEAP_TEST}.c)
target_link_libraries(${HEAP_TEST} PRIVATE base)

add_executable(${LIST_TEST} ${LIST_TEST}.c)
target_link_libraries(${LIST_TEST} PRIVATE base)

add_executable(${LSTACK_TEST} ${LSTACK_TEST}.c)
target_link_libraries(${LSTACK_TEST} PRIVATE base)

add_executable(${LRUCACHE_TEST} ${LRUCACHE_TEST}.cpp)
target_link_libraries(${LRUCACHE_TEST} PRIVATE base)

add_executable(${OBJECTPOOL_C_TEST} ${OBJECTPOOL_C_TEST}.c)
target_link_libraries(${OBJECTPOOL_C_TEST} PRIVATE base)

add_executable(${OBJECTPOOL_TEST} ${OBJECTPOOL_TEST}.cpp)
target_link_libraries(${OBJECTPOOL_TEST} PRIVATE base)

add_executable(${RBTREE_TEST} ${RBTREE_TEST}.c)
target_link_libraries(${RBTREE_TEST} PRIVATE base)

add_executable(${RINGBUFFER_TEST} ${RINGBUFFER_TEST}.c)
target_link_libraries(${RINGBUFFER_TEST} PRIVATE base)

add_executable(${SHM_TEST} ${SHM_TEST}.c)
target_link_libraries(${SHM_TEST} PRIVATE base shm_lib)

add_executable(${SINGLETON_TEST} ${SINGLETON_TEST}.cpp)
target_link_libraries(${SINGLETON_TEST} PRIVATE base)

add_executable(${THREAD_TEST} ${THREAD_TEST}.cpp)
target_link_libraries(${THREAD_TEST} PRIVATE base)

add_executable(${THREADPOOL_C_TEST} ${THREADPOOL_C_TEST}.c)
target_link_libraries(${THREADPOOL_C_TEST} PRIVATE base)

add_executable(${THREADPOOL_CPP_TEST} ${THREADPOOL_CPP_TEST}.cpp)
target_link_libraries(${THREADPOOL_CPP_TEST} PRIVATE base)

add_executable(${THREADQUEUE_C_TEST} ${THREADQUEUE_C_TEST}.c)
target_link_libraries(${THREADQUEUE_C_TEST} PRIVATE base)

add_executable(${THREADQUEUE_CPP_TEST} ${THREADQUEUE_CPP_TEST}.cpp)
target_link_libraries(${THREADQUEUE_CPP_TEST} PRIVATE base)

add_executable(${TIMERCLUSTER_C_TEST} ${TIMERCLUSTER_C_TEST}.c)
target_link_libraries(${TIMERCLUSTER_C_TEST} PRIVATE base)

add_executable(${TIMERCLUSTER_CPP_TEST} ${TIMERCLUSTER_CPP_TEST}.cpp)
target_link_libraries(${TIMERCLUSTER_CPP_TEST} PRIVATE base)

add_executable(${XSTRING_TEST} ${XSTRING_TEST}.c)
target_link_libraries(${XSTRING_TEST} PRIVATE base)

install(TARGETS ${TEST_TEST} DESTINATION test)
install(TARGETS ${ARENA_TEST} DESTINATION test)
install(TARGETS ${AVL_TEST} DESTINATION test)
install(TARGETS ${AVL_UID_TEST} DESTINATION test)
install(TARGETS ${DARRAY_TEST} DESTINATION test)
install(TARGETS ${DEBUG_BACKTRACE_TEST} DESTINATION test)
install(TARGETS ${DEQUE_TEST} DESTINATION test)
install(TARGETS ${FSM_TEST} DESTINATION test)
install(TARGETS ${HEAP_TEST} DESTINATION test)
install(TARGETS ${LIST_TEST} DESTINATION test)
install(TARGETS ${LSTACK_TEST} DESTINATION test)
install(TARGETS ${LRUCACHE_TEST} DESTINATION test)
install(TARGETS ${OBJECTPOOL_C_TEST} DESTINATION test)
install(TARGETS ${OBJECTPOOL_TEST} DESTINATION test)
install(TARGETS ${RBTREE_TEST} DESTINATION test)
install(TARGETS ${RINGBUFFER_TEST} DESTINATION test)
install(TARGETS ${SHM_TEST} DESTINATION test)
install(TARGETS ${SINGLETON_TEST} DESTINATION test)
install(TARGETS ${THREAD_TEST} DESTINATION test)
install(TARGETS ${THREADPOOL_C_TEST} DESTINATION test)
install(TARGETS ${THREADPOOL_CPP_TEST} DESTINATION test)
install(TARGETS ${THREADQUEUE_C_TEST} DESTINATION test)
install(TARGETS ${THREADQUEUE_CPP_TEST} DESTINATION test)
install(TARGETS ${TIMERCLUSTER_C_TEST} DESTINATION test)
install(TARGETS ${TIMERCLUSTER_CPP_TEST} DESTINATION test)
install(TARGETS ${XSTRING_TEST} DESTINATION test)
