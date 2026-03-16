set(IPC_NNG_LONG_LIB ipc_nng_long)
set(IPC_NNG_LONG_DAEMON ipc_nng_long_daemon)
set(IPC_NNG_LONG_APP1 ipc_nng_long_app1)
set(IPC_NNG_LONG_APP2 ipc_nng_long_app2)

if(NOT CPUNCHLINE_BUILD_THIRDPARTY)
    find_package(nng REQUIRED)
    if(TARGET nng::nng AND NOT TARGET nng)
        add_library(nng ALIAS nng::nng)
    endif()
endif()

add_library(${IPC_NNG_LONG_LIB}-deps INTERFACE)
target_include_directories(${IPC_NNG_LONG_LIB}-deps INTERFACE inc)

add_library(${IPC_NNG_LONG_LIB}-object OBJECT
    src/ipc_nng_long_inn.cpp
    src/ipc_nng_long.cpp
    src/ipc_nng_long_daemon.cpp
)
target_link_libraries(${IPC_NNG_LONG_LIB}-object PRIVATE ${IPC_NNG_LONG_LIB}-deps Threads::Threads nng base)

if(BUILD_SHARED_LIBS)
    add_library(${IPC_NNG_LONG_LIB} SHARED $<TARGET_OBJECTS:${IPC_NNG_LONG_LIB}-object>)
else()
    add_library(${IPC_NNG_LONG_LIB} STATIC $<TARGET_OBJECTS:${IPC_NNG_LONG_LIB}-object>)
endif()
target_link_libraries(${IPC_NNG_LONG_LIB} INTERFACE ${IPC_NNG_LONG_LIB}-deps)
target_link_libraries(${IPC_NNG_LONG_LIB} PRIVATE Threads::Threads nng)
set_property(TARGET ${IPC_NNG_LONG_LIB} PROPERTY OUTPUT_NAME "${IPC_NNG_LONG_LIB}")
set_property(TARGET ${IPC_NNG_LONG_LIB} PROPERTY VERSION 0.0.1)
set_property(TARGET ${IPC_NNG_LONG_LIB} PROPERTY SOVERSION 0)

if(WIN32)
    set_property(TARGET ${IPC_NNG_LONG_LIB} PROPERTY RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
    set_property(TARGET ${IPC_NNG_LONG_LIB} PROPERTY ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
    set_property(TARGET ${IPC_NNG_LONG_LIB} PROPERTY LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
    set_property(TARGET ${IPC_NNG_LONG_LIB} PROPERTY RUNTIME_OUTPUT_DIRECTORY_DEBUG ${PROJECT_BINARY_DIR})
    set_property(TARGET ${IPC_NNG_LONG_LIB} PROPERTY ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${PROJECT_BINARY_DIR})
    set_property(TARGET ${IPC_NNG_LONG_LIB} PROPERTY LIBRARY_OUTPUT_DIRECTORY_DEBUG ${PROJECT_BINARY_DIR})
    set_property(TARGET ${IPC_NNG_LONG_LIB} PROPERTY RUNTIME_OUTPUT_DIRECTORY_RELEASE ${PROJECT_BINARY_DIR})
    set_property(TARGET ${IPC_NNG_LONG_LIB} PROPERTY ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${PROJECT_BINARY_DIR})
    set_property(TARGET ${IPC_NNG_LONG_LIB} PROPERTY LIBRARY_OUTPUT_DIRECTORY_RELEASE ${PROJECT_BINARY_DIR})
endif()

install(TARGETS ${IPC_NNG_LONG_LIB} DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES inc/ipc_nng_long.h DESTINATION include/${IPC_NNG_LONG_LIB})

add_executable(${IPC_NNG_LONG_DAEMON}
    daemon/ipc_nng_long_daemon.c
)
target_link_libraries(${IPC_NNG_LONG_DAEMON} PRIVATE ${IPC_NNG_LONG_LIB} base)

install(TARGETS ${IPC_NNG_LONG_DAEMON} DESTINATION ${CMAKE_INSTALL_BINDIR})

add_executable(${IPC_NNG_LONG_APP1}
    client/ipc_nng_long_app1.c
)
target_link_libraries(${IPC_NNG_LONG_APP1} PRIVATE Threads::Threads ipc_nng_long base)
target_include_directories(${IPC_NNG_LONG_APP1} PRIVATE .)

install(TARGETS ${IPC_NNG_LONG_APP1} DESTINATION ${CMAKE_INSTALL_BINDIR})

add_executable(${IPC_NNG_LONG_APP2}
    client/ipc_nng_long_app2.c
)
target_link_libraries(${IPC_NNG_LONG_APP2} PRIVATE Threads::Threads ipc_nng_long base)
target_include_directories(${IPC_NNG_LONG_APP2} PRIVATE .)

install(TARGETS ${IPC_NNG_LONG_APP2} DESTINATION ${CMAKE_INSTALL_BINDIR})
