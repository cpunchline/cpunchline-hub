set(IPC_ZMQ_LIB ipc_zmq)
set(IPC_ZMQ_MANAGER ipc_zmq_manager)
set(IPC_ZMQ_APP1 ipc_zmq_app1)
set(IPC_ZMQ_APP2 ipc_zmq_app2)

if(NOT CPUNCHLINE_BUILD_THIRDPARTY)
    find_package(ZeroMQ REQUIRED)
endif()

add_library(${IPC_ZMQ_LIB}-deps INTERFACE)
target_include_directories(${IPC_ZMQ_LIB}-deps INTERFACE inc)


add_library(${IPC_ZMQ_LIB}-object OBJECT
    src/ipc_zmq_inn.c
    src/ipc_zmq.c
)
target_link_libraries(${IPC_ZMQ_LIB}-object PRIVATE ${IPC_ZMQ_LIB}-deps Threads::Threads libzmq base)

if(BUILD_SHARED_LIBS)
    add_library(${IPC_ZMQ_LIB} SHARED $<TARGET_OBJECTS:${IPC_ZMQ_LIB}-object>)
else()
    add_library(${IPC_ZMQ_LIB} STATIC $<TARGET_OBJECTS:${IPC_ZMQ_LIB}-object>)
endif()
target_link_libraries(${IPC_ZMQ_LIB} INTERFACE ${IPC_ZMQ_LIB}-deps)
target_link_libraries(${IPC_ZMQ_LIB} PRIVATE Threads::Threads libzmq)
set_property(TARGET ${IPC_ZMQ_LIB} PROPERTY OUTPUT_NAME "${IPC_ZMQ_LIB}")
set_property(TARGET ${IPC_ZMQ_LIB} PROPERTY VERSION 0.0.1)
set_property(TARGET ${IPC_ZMQ_LIB} PROPERTY SOVERSION 0)

if(WIN32)
    set_property(TARGET ${IPC_ZMQ_LIB} PROPERTY RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
    set_property(TARGET ${IPC_ZMQ_LIB} PROPERTY ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
    set_property(TARGET ${IPC_ZMQ_LIB} PROPERTY LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
    set_property(TARGET ${IPC_ZMQ_LIB} PROPERTY RUNTIME_OUTPUT_DIRECTORY_DEBUG ${PROJECT_BINARY_DIR})
    set_property(TARGET ${IPC_ZMQ_LIB} PROPERTY ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${PROJECT_BINARY_DIR})
    set_property(TARGET ${IPC_ZMQ_LIB} PROPERTY LIBRARY_OUTPUT_DIRECTORY_DEBUG ${PROJECT_BINARY_DIR})
    set_property(TARGET ${IPC_ZMQ_LIB} PROPERTY RUNTIME_OUTPUT_DIRECTORY_RELEASE ${PROJECT_BINARY_DIR})
    set_property(TARGET ${IPC_ZMQ_LIB} PROPERTY ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${PROJECT_BINARY_DIR})
    set_property(TARGET ${IPC_ZMQ_LIB} PROPERTY LIBRARY_OUTPUT_DIRECTORY_RELEASE ${PROJECT_BINARY_DIR})
endif()

install(TARGETS ${IPC_ZMQ_LIB} DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES inc/ipc_zmq.h DESTINATION include/${IPC_ZMQ_LIB})

add_executable(${IPC_ZMQ_MANAGER}
    client/ipc_zmq_manager.c
)
target_link_libraries(${IPC_ZMQ_MANAGER} PUBLIC Threads::Threads ipc_zmq)
target_link_libraries(${IPC_ZMQ_MANAGER} PRIVATE base-deps)
target_include_directories(${IPC_ZMQ_MANAGER} PUBLIC .)

install(TARGETS ${IPC_ZMQ_MANAGER} DESTINATION ${CMAKE_INSTALL_BINDIR})

add_executable(${IPC_ZMQ_APP1}
    client/ipc_zmq_app1.c
)
target_link_libraries(${IPC_ZMQ_APP1} PRIVATE Threads::Threads ipc_zmq)
target_link_libraries(${IPC_ZMQ_APP1} PRIVATE base-deps)
target_include_directories(${IPC_ZMQ_APP1} PRIVATE .)

install(TARGETS ${IPC_ZMQ_APP1} DESTINATION ${CMAKE_INSTALL_BINDIR})

add_executable(${IPC_ZMQ_APP2}
    client/ipc_zmq_app2.c
)
target_link_libraries(${IPC_ZMQ_APP2} PUBLIC Threads::Threads ipc_zmq)
target_link_libraries(${IPC_ZMQ_APP2} PRIVATE base-deps)
target_include_directories(${IPC_ZMQ_APP2} PUBLIC .)

install(TARGETS ${IPC_ZMQ_APP2} DESTINATION ${CMAKE_INSTALL_BINDIR})
