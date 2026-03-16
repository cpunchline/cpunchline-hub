add_compile_options(-w)

set(ZMQ_TEST_CLIENT zmq_test_client)
set(ZMQ_TEST_SERVER zmq_test_server)
set(ZMQ_TEST_PROXY zmq_test_proxy)

if(NOT CPUNCHLINE_BUILD_THIRDPARTY)
    find_package(ZeroMQ REQUIRED)
endif()

aux_source_directory(common/src ZMQ_TEST_COMMON_SRC_LIST)

# client
aux_source_directory(client ZMQ_TEST_CLIENT_SRC_LIST)
add_executable(${ZMQ_TEST_CLIENT} ${ZMQ_TEST_CLIENT_SRC_LIST} ${ZMQ_TEST_COMMON_SRC_LIST})
target_link_libraries(${ZMQ_TEST_CLIENT} PRIVATE libzmq)
target_include_directories(${ZMQ_TEST_CLIENT} PRIVATE client)
target_include_directories(${ZMQ_TEST_CLIENT} PRIVATE common/inc)

install(TARGETS ${ZMQ_TEST_CLIENT} DESTINATION test)

# server
aux_source_directory(server ZMQ_TEST_SERVER_SRC_LIST)
add_executable(${ZMQ_TEST_SERVER} ${ZMQ_TEST_SERVER_SRC_LIST} ${ZMQ_TEST_COMMON_SRC_LIST})
target_link_libraries(${ZMQ_TEST_SERVER} PRIVATE libzmq)
target_include_directories(${ZMQ_TEST_SERVER} PRIVATE server)
target_include_directories(${ZMQ_TEST_SERVER} PRIVATE common/inc)

install(TARGETS ${ZMQ_TEST_SERVER} DESTINATION test)

# proxy
aux_source_directory(proxy ZMQ_TEST_PROXY_SRC_LIST)
add_executable(${ZMQ_TEST_PROXY} ${ZMQ_TEST_PROXY_SRC_LIST} ${ZMQ_TEST_COMMON_SRC_LIST})
target_link_libraries(${ZMQ_TEST_PROXY} PRIVATE libzmq)
target_include_directories(${ZMQ_TEST_PROXY} PRIVATE proxy)
target_include_directories(${ZMQ_TEST_PROXY} PRIVATE common/inc)

install(TARGETS ${ZMQ_TEST_PROXY} DESTINATION test)
