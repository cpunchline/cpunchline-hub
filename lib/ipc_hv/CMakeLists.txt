set(IPC_HV_LIB ipc_hv)
set(IPC_HV_CLIENT ipc_hv_client)
set(IPC_HV_SERVER ipc_hv_server)

if(NOT CPUNCHLINE_BUILD_THIRDPARTY)
    find_package(libhv REQUIRED)
endif()

add_library(${IPC_HV_LIB}-deps INTERFACE)
target_include_directories(${IPC_HV_LIB}-deps INTERFACE inc)

add_library(${IPC_HV_LIB}-object OBJECT
    src/ipc_hv.c
)
target_link_libraries(${IPC_HV_LIB}-object PRIVATE ${IPC_HV_LIB}-deps)
target_link_libraries(${IPC_HV_LIB}-object PRIVATE base hv)

if(BUILD_SHARED_LIBS)
    add_library(${IPC_HV_LIB} SHARED $<TARGET_OBJECTS:${IPC_HV_LIB}-object>)
else()
    add_library(${IPC_HV_LIB} STATIC $<TARGET_OBJECTS:${IPC_HV_LIB}-object>)
endif()
target_link_libraries(${IPC_HV_LIB} INTERFACE ${IPC_HV_LIB}-deps)
target_link_libraries(${IPC_HV_LIB} PRIVATE hv)
set_property(TARGET ${IPC_HV_LIB} PROPERTY OUTPUT_NAME "${IPC_HV_LIB}")
set_property(TARGET ${IPC_HV_LIB} PROPERTY VERSION 0.0.1)
set_property(TARGET ${IPC_HV_LIB} PROPERTY SOVERSION 0)

if(WIN32)
    set_property(TARGET ${IPC_HV_LIB} PROPERTY RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
    set_property(TARGET ${IPC_HV_LIB} PROPERTY ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
    set_property(TARGET ${IPC_HV_LIB} PROPERTY LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
    set_property(TARGET ${IPC_HV_LIB} PROPERTY RUNTIME_OUTPUT_DIRECTORY_DEBUG ${PROJECT_BINARY_DIR})
    set_property(TARGET ${IPC_HV_LIB} PROPERTY ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${PROJECT_BINARY_DIR})
    set_property(TARGET ${IPC_HV_LIB} PROPERTY LIBRARY_OUTPUT_DIRECTORY_DEBUG ${PROJECT_BINARY_DIR})
    set_property(TARGET ${IPC_HV_LIB} PROPERTY RUNTIME_OUTPUT_DIRECTORY_RELEASE ${PROJECT_BINARY_DIR})
    set_property(TARGET ${IPC_HV_LIB} PROPERTY ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${PROJECT_BINARY_DIR})
    set_property(TARGET ${IPC_HV_LIB} PROPERTY LIBRARY_OUTPUT_DIRECTORY_RELEASE ${PROJECT_BINARY_DIR})
endif()

install(TARGETS ${IPC_HV_LIB} DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY inc/ DESTINATION include/${IPC_HV_LIB})

add_executable(${IPC_HV_CLIENT}
    client/ipc_hv_client.c
)
target_link_libraries(${IPC_HV_CLIENT} PRIVATE Threads::Threads ipc_hv)
target_link_libraries(${IPC_HV_CLIENT} PRIVATE base)
target_include_directories(${IPC_HV_CLIENT} PRIVATE .)

add_executable(${IPC_HV_SERVER}
    client/ipc_hv_server.c
)
target_link_libraries(${IPC_HV_SERVER} PRIVATE Threads::Threads ipc_hv)
target_link_libraries(${IPC_HV_SERVER} PRIVATE base)
target_include_directories(${IPC_HV_SERVER} PRIVATE .)

install(TARGETS ${IPC_HV_CLIENT} ${IPC_HV_SERVER} DESTINATION ${CMAKE_INSTALL_BINDIR})
