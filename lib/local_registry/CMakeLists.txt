set(LOCAL_REGISTRY local_registry)
set(LOCAL_REGISTRY_CTRL local_registry_ctrl)
set(IPC_HV_SOA ipc_hv_soa)
set(IPC_HV_SOA_CLIENT1 ipc_hv_soa_client1)
set(IPC_HV_SOA_CLIENT2 ipc_hv_soa_client2)

if(NOT CPUNCHLINE_BUILD_THIRDPARTY)
    find_package(libhv REQUIRED)
endif()

set(PROTO_STAMP ${CPUNCHLINE_BUILD_DIR}/proto.stamp)

# Step 1: json -> proto
file(GLOB LOCAL_REGISTRY_JSON "${CMAKE_CURRENT_LIST_DIR}/json/*.json")
set(LOCAL_REGISTRY_GENERATED_PROTO "")
foreach(json ${LOCAL_REGISTRY_JSON})
    get_filename_component(fname ${json} NAME_WE)
    set(proto_file ${CMAKE_CURRENT_SOURCE_DIR}/generated/${fname}.proto)
    list(APPEND LOCAL_REGISTRY_GENERATED_PROTO ${proto_file})
    add_custom_command(
        OUTPUT ${proto_file}
        COMMAND ${CMAKE_COMMAND} -E echo "Using pre-generated ${proto_file}"
        DEPENDS ${PROTO_STAMP}
        COMMENT "Marking ${proto_file} as up-to-date"
    )
endforeach()

find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_custom_command(
    OUTPUT ${PROTO_STAMP}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_SOURCE_DIR}/generated
    COMMAND ${Python3_EXECUTABLE}
    ${CPUNCHLINE_TOOLS_DIR}/modulejson2proto/modulejson2proto.py
    ${CMAKE_CURRENT_LIST_DIR}/json
    ${CMAKE_CURRENT_LIST_DIR}/proto
    ${CMAKE_CURRENT_LIST_DIR}/generated
    COMMAND ${CMAKE_COMMAND} -E touch ${PROTO_STAMP}
    DEPENDS
    ${CPUNCHLINE_TOOLS_DIR}/modulejson2proto/modulejson2proto.py
    ${LOCAL_REGISTRY_JSON}
    COMMENT "Generating local registry json->proto code"
    VERBATIM
)

# Step 2: proto -> nanopb C
# nanopb_generate_cpp(TARGET ${LOCAL_REGISTRY}_nanopb_auto proto/${LOCAL_REGISTRY}.proto)

if(CPUNCHLINE_BUILD_THIRDPARTY)
    set(PROTOC_GEN_NANOPB_DIR "${NANOPB_SRC_ROOT_FOLDER}/generator")
else()
    set(PROTOC_GEN_NANOPB_DIR "${CPUNCHLINE_PRE_BUILT_DIR}/bin")
endif()

set(LOCAL_REGISTRY_GENERATED_CODE "")
foreach(proto ${LOCAL_REGISTRY_GENERATED_PROTO})
    get_filename_component(fname ${proto} NAME_WE)
    list(APPEND LOCAL_REGISTRY_GENERATED_CODE
        ${CMAKE_CURRENT_LIST_DIR}/generated/${fname}.pb.h
        ${CMAKE_CURRENT_LIST_DIR}/generated/${fname}.pb.c
        ${CMAKE_CURRENT_LIST_DIR}/generated/${fname}_autolib.c
    )
endforeach()
list(APPEND LOCAL_REGISTRY_GENERATED_CODE
    ${CMAKE_CURRENT_LIST_DIR}/generated/generator_autolib_def.h
    ${CMAKE_CURRENT_LIST_DIR}/generated/generator_autolib.h
    ${CMAKE_CURRENT_LIST_DIR}/generated/generator_autolib.c
)

add_custom_command(
    OUTPUT ${LOCAL_REGISTRY_GENERATED_CODE}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_SOURCE_DIR}/generated
    COMMAND sh
    ${CPUNCHLINE_SCRIPTS_DIR}/nanopb_generator.sh
    ${NANOPB_SRC_ROOT_FOLDER}/generator
    ${PROTOC_GEN_NANOPB_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/proto
    ${CMAKE_CURRENT_LIST_DIR}/generated
    DEPENDS
    ${CPUNCHLINE_SCRIPTS_DIR}/nanopb_generator.sh
    ${PROTO_STAMP}
    COMMENT "Generating local registry proto->nanopb C code"
    VERBATIM
)

add_library(${LOCAL_REGISTRY}_nanopb_auto ${LOCAL_REGISTRY_GENERATED_CODE})
target_link_libraries(${LOCAL_REGISTRY}_nanopb_auto PUBLIC protobuf-nanopb)
target_link_directories(${LOCAL_REGISTRY}_nanopb_auto PUBLIC ${CPUNCHLINE_PRE_BUILT_DIR}/lib)
target_include_directories(${LOCAL_REGISTRY}_nanopb_auto PUBLIC ${CMAKE_CURRENT_LIST_DIR}/generated)
target_include_directories(${LOCAL_REGISTRY}_nanopb_auto PUBLIC ${CPUNCHLINE_PRE_BUILT_DIR}/include/nanopb)
install(TARGETS ${LOCAL_REGISTRY}_nanopb_auto DESTINATION ${CMAKE_INSTALL_LIBDIR})

# daemon
add_executable(${LOCAL_REGISTRY}
    daemon/local_registry.cpp
    daemon/local_registry_logic.cpp
)
target_include_directories(${LOCAL_REGISTRY} PRIVATE inc)
target_link_libraries(${LOCAL_REGISTRY} PRIVATE base hv ${LOCAL_REGISTRY}_nanopb_auto)

install(TARGETS ${LOCAL_REGISTRY} DESTINATION ${CMAKE_INSTALL_BINDIR})

# ctrl
add_executable(${LOCAL_REGISTRY_CTRL}
    ctrl/local_registry_ctrl.cpp
)
target_include_directories(${LOCAL_REGISTRY_CTRL} PRIVATE inc)
target_link_libraries(${LOCAL_REGISTRY_CTRL} PRIVATE base hv ${LOCAL_REGISTRY}_nanopb_auto)

install(TARGETS ${LOCAL_REGISTRY_CTRL} DESTINATION ${CMAKE_INSTALL_BINDIR})

# lib
add_library(${IPC_HV_SOA}-deps INTERFACE)
target_include_directories(${IPC_HV_SOA}-deps INTERFACE inc)
target_include_directories(${IPC_HV_SOA}-deps INTERFACE lib)
target_link_libraries(${IPC_HV_SOA}-deps INTERFACE hv)

add_library(${IPC_HV_SOA}-object OBJECT
    lib/ipc_hv_soa_inn_timer.cpp
    lib/ipc_hv_soa_inn_map.cpp
    lib/ipc_hv_soa_inn_io.cpp
    lib/ipc_hv_soa_inn.cpp
    lib/ipc_hv_soa.cpp
)
target_link_libraries(${IPC_HV_SOA}-object PRIVATE ${IPC_HV_SOA}-deps)
target_link_libraries(${IPC_HV_SOA}-object PRIVATE base ${LOCAL_REGISTRY}_nanopb_auto)

if(BUILD_SHARED_LIBS)
    add_library(${IPC_HV_SOA} SHARED $<TARGET_OBJECTS:${IPC_HV_SOA}-object>)
else()
    add_library(${IPC_HV_SOA} STATIC $<TARGET_OBJECTS:${IPC_HV_SOA}-object>)
endif()

target_link_libraries(${IPC_HV_SOA} INTERFACE ${IPC_HV_SOA}-deps)
target_link_libraries(${IPC_HV_SOA} PRIVATE base)
set_property(TARGET ${IPC_HV_SOA} PROPERTY OUTPUT_NAME "${IPC_HV_SOA}")
set_property(TARGET ${IPC_HV_SOA} PROPERTY VERSION 0.0.1)
set_property(TARGET ${IPC_HV_SOA} PROPERTY SOVERSION 0)

if(WIN32)
    set_property(TARGET ${IPC_HV_SOA} PROPERTY RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
    set_property(TARGET ${IPC_HV_SOA} PROPERTY ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
    set_property(TARGET ${IPC_HV_SOA} PROPERTY LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
    set_property(TARGET ${IPC_HV_SOA} PROPERTY RUNTIME_OUTPUT_DIRECTORY_DEBUG ${PROJECT_BINARY_DIR})
    set_property(TARGET ${IPC_HV_SOA} PROPERTY ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${PROJECT_BINARY_DIR})
    set_property(TARGET ${IPC_HV_SOA} PROPERTY LIBRARY_OUTPUT_DIRECTORY_DEBUG ${PROJECT_BINARY_DIR})
    set_property(TARGET ${IPC_HV_SOA} PROPERTY RUNTIME_OUTPUT_DIRECTORY_RELEASE ${PROJECT_BINARY_DIR})
    set_property(TARGET ${IPC_HV_SOA} PROPERTY ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${PROJECT_BINARY_DIR})
    set_property(TARGET ${IPC_HV_SOA} PROPERTY LIBRARY_OUTPUT_DIRECTORY_RELEASE ${PROJECT_BINARY_DIR})
endif()

install(TARGETS ${IPC_HV_SOA} DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES lib/ipc_hv_soa.h DESTINATION include/${IPC_HV_SOA})

# client
add_executable(${IPC_HV_SOA_CLIENT1}
    client/ipc_hv_soa_client1.c
)
target_include_directories(${IPC_HV_SOA_CLIENT1} PRIVATE inc)
target_link_libraries(${IPC_HV_SOA_CLIENT1} PRIVATE base ${IPC_HV_SOA} ${LOCAL_REGISTRY}_nanopb_auto)

install(TARGETS ${IPC_HV_SOA_CLIENT1} DESTINATION ${CMAKE_INSTALL_BINDIR})

add_executable(${IPC_HV_SOA_CLIENT2}
    client/ipc_hv_soa_client2.c
)
target_include_directories(${IPC_HV_SOA_CLIENT2} PRIVATE inc)
target_link_libraries(${IPC_HV_SOA_CLIENT2} PRIVATE base ${IPC_HV_SOA} ${LOCAL_REGISTRY}_nanopb_auto)

install(TARGETS ${IPC_HV_SOA_CLIENT2} DESTINATION ${CMAKE_INSTALL_BINDIR})
