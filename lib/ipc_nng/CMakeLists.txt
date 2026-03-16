set(IPC_NNG_LIB ipc_nng)
set(IPC_NNG_MANAGER ipc_nng_manager)
set(IPC_NNG_APP1 ipc_nng_app1)
set(IPC_NNG_APP2 ipc_nng_app2)

if(NOT CPUNCHLINE_BUILD_THIRDPARTY)
    find_package(nng REQUIRED)
    if(TARGET nng::nng AND NOT TARGET nng)
        add_library(nng ALIAS nng::nng)
    endif()
endif()

add_library(${IPC_NNG_LIB}-deps INTERFACE)
target_include_directories(${IPC_NNG_LIB}-deps INTERFACE inc)

add_library(${IPC_NNG_LIB}-object OBJECT
    src/ipc_nng.c
)
target_link_libraries(${IPC_NNG_LIB}-object PRIVATE ${IPC_NNG_LIB}-deps Threads::Threads nng base)

if(BUILD_SHARED_LIBS)
    add_library(${IPC_NNG_LIB} SHARED $<TARGET_OBJECTS:${IPC_NNG_LIB}-object>)
else()
    add_library(${IPC_NNG_LIB} STATIC $<TARGET_OBJECTS:${IPC_NNG_LIB}-object>)
endif()
target_link_libraries(${IPC_NNG_LIB} INTERFACE ${IPC_NNG_LIB}-deps)
target_link_libraries(${IPC_NNG_LIB} PRIVATE Threads::Threads nng)
set_property(TARGET ${IPC_NNG_LIB} PROPERTY OUTPUT_NAME "${IPC_NNG_LIB}")
set_property(TARGET ${IPC_NNG_LIB} PROPERTY VERSION 0.0.1)
set_property(TARGET ${IPC_NNG_LIB} PROPERTY SOVERSION 0)

if(WIN32)
    set_property(TARGET ${IPC_NNG_LIB} PROPERTY RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
    set_property(TARGET ${IPC_NNG_LIB} PROPERTY ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
    set_property(TARGET ${IPC_NNG_LIB} PROPERTY LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
    set_property(TARGET ${IPC_NNG_LIB} PROPERTY RUNTIME_OUTPUT_DIRECTORY_DEBUG ${PROJECT_BINARY_DIR})
    set_property(TARGET ${IPC_NNG_LIB} PROPERTY ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${PROJECT_BINARY_DIR})
    set_property(TARGET ${IPC_NNG_LIB} PROPERTY LIBRARY_OUTPUT_DIRECTORY_DEBUG ${PROJECT_BINARY_DIR})
    set_property(TARGET ${IPC_NNG_LIB} PROPERTY RUNTIME_OUTPUT_DIRECTORY_RELEASE ${PROJECT_BINARY_DIR})
    set_property(TARGET ${IPC_NNG_LIB} PROPERTY ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${PROJECT_BINARY_DIR})
    set_property(TARGET ${IPC_NNG_LIB} PROPERTY LIBRARY_OUTPUT_DIRECTORY_RELEASE ${PROJECT_BINARY_DIR})
endif()

install(TARGETS ${IPC_NNG_LIB} DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY inc/ DESTINATION include/${IPC_NNG_LIB})

add_executable(${IPC_NNG_MANAGER}
    client/ipc_nng_manager.c
)
target_link_libraries(${IPC_NNG_MANAGER} PRIVATE Threads::Threads ipc_nng base)
target_include_directories(${IPC_NNG_MANAGER} PRIVATE .)

install(TARGETS ${IPC_NNG_MANAGER} DESTINATION ${CMAKE_INSTALL_BINDIR})

add_executable(${IPC_NNG_APP1}
    client/ipc_nng_app1.c
)
target_link_libraries(${IPC_NNG_APP1} PRIVATE Threads::Threads ipc_nng base)
target_include_directories(${IPC_NNG_APP1} PRIVATE .)

install(TARGETS ${IPC_NNG_APP1} DESTINATION ${CMAKE_INSTALL_BINDIR})

add_executable(${IPC_NNG_APP2}
    client/ipc_nng_app2.c
)
target_link_libraries(${IPC_NNG_APP2} PRIVATE Threads::Threads ipc_nng base)
target_include_directories(${IPC_NNG_APP2} PRIVATE .)

install(TARGETS ${IPC_NNG_APP2} DESTINATION ${CMAKE_INSTALL_BINDIR})
