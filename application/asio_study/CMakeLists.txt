set(ASIO_STUDY asio_study)

add_executable(${ASIO_STUDY}
    asio_study.cpp
)
if(NOT CPUNCHLINE_BUILD_THIRDPARTY)
    target_include_directories(${ASIO_STUDY} PRIVATE ${CPUNCHLINE_PRE_BUILT_DIR}/include)
else()
    target_include_directories(${ASIO_STUDY} PRIVATE ${CPUNCHLINE_THIRDPARTY_DIR}/asio/include)
endif()
target_include_directories(${ASIO_STUDY} PRIVATE .)
target_compile_options(${ASIO_STUDY} PRIVATE -w)

install(TARGETS ${ASIO_STUDY} DESTINATION ${CMAKE_INSTALL_BINDIR})
