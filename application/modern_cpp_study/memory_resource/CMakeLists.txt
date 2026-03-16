set(MEMORY_RESOURCE_APP memory_resource_app)

aux_source_directory(. MEMORY_RESOURCE_APP_SRC_LIST)

add_executable(${MEMORY_RESOURCE_APP} 
    memory_resource.cpp)
target_include_directories(${MEMORY_RESOURCE_APP} PRIVATE .)

install(TARGETS ${MEMORY_RESOURCE_APP} DESTINATION test)
