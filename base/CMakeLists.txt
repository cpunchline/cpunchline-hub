set(BASE_LIB base)

add_library(${BASE_LIB}-deps INTERFACE)
target_include_directories(${BASE_LIB}-deps INTERFACE inc)
target_include_directories(${BASE_LIB}-deps INTERFACE inc/hpp)

add_library(${BASE_LIB}-object OBJECT
    # utility
    src/utility/debug_backtrace.c
    src/utility/utils.c
    src/utility/threadpool.c
    src/utility/threadqueue.c
    src/utility/timercluster.c
    src/utility/objectpool.c

    # data structure
    src/dsa/avl.c
    src/dsa/avl_uid.c
    src/dsa/rbtree.c
    src/dsa/vlist.c
    src/dsa/ringbuffer.c

    # algorithm
    src/dsa/base64.c
    src/dsa/crc.c
    src/dsa/md5.c
    src/dsa/sha1.c
)
target_link_libraries(${BASE_LIB}-object PRIVATE ${BASE_LIB}-deps)

if(BUILD_SHARED_LIBS)
    add_library(${BASE_LIB} SHARED $<TARGET_OBJECTS:${BASE_LIB}-object>)
else()
    add_library(${BASE_LIB} STATIC $<TARGET_OBJECTS:${BASE_LIB}-object>)
endif()
target_link_libraries(${BASE_LIB} INTERFACE ${BASE_LIB}-deps) # for other module
set_property(TARGET ${BASE_LIB} PROPERTY OUTPUT_NAME "${BASE_LIB}")
set_property(TARGET ${BASE_LIB} PROPERTY VERSION 0.0.1)
set_property(TARGET ${BASE_LIB} PROPERTY SOVERSION 0)

if(WIN32)
    set_property(TARGET ${BASE_LIB} PROPERTY RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
    set_property(TARGET ${BASE_LIB} PROPERTY ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
    set_property(TARGET ${BASE_LIB} PROPERTY LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
    set_property(TARGET ${BASE_LIB} PROPERTY RUNTIME_OUTPUT_DIRECTORY_DEBUG ${PROJECT_BINARY_DIR})
    set_property(TARGET ${BASE_LIB} PROPERTY ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${PROJECT_BINARY_DIR})
    set_property(TARGET ${BASE_LIB} PROPERTY LIBRARY_OUTPUT_DIRECTORY_DEBUG ${PROJECT_BINARY_DIR})
    set_property(TARGET ${BASE_LIB} PROPERTY RUNTIME_OUTPUT_DIRECTORY_RELEASE ${PROJECT_BINARY_DIR})
    set_property(TARGET ${BASE_LIB} PROPERTY ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${PROJECT_BINARY_DIR})
    set_property(TARGET ${BASE_LIB} PROPERTY LIBRARY_OUTPUT_DIRECTORY_RELEASE ${PROJECT_BINARY_DIR})
endif()

install(TARGETS ${BASE_LIB} DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY inc/ DESTINATION include/${BASE_LIB})
