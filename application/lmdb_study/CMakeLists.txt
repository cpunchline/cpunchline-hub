set(LMDB_STUDY lmdb_study)

add_executable(${LMDB_STUDY}
    ${LMDB_STUDY}.c
)
if(NOT CPUNCHLINE_BUILD_THIRDPARTY)
    target_include_directories(${LMDB_STUDY} PRIVATE ${CPUNCHLINE_PRE_BUILT_DIR}/include/liblmdb)
else()
    target_include_directories(${LMDB_STUDY} PRIVATE ${CPUNCHLINE_THIRDPARTY_DIR}/lmdb/libraries/liblmdb)
endif()
target_link_libraries(${LMDB_STUDY} PRIVATE lmdb base)
target_link_directories(${LMDB_STUDY} PRIVATE ${CPUNCHLINE_PRE_BUILT_DIR}/lib)
install(TARGETS ${LMDB_STUDY} DESTINATION ${CMAKE_INSTALL_BINDIR})
