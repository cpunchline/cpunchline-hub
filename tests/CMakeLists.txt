if(NOT CPUNCHLINE_BUILD_THIRDPARTY)
    find_package(GTest REQUIRED)
endif()

set(GTEST_APP gtest_app)

add_executable(${GTEST_APP} test.cpp)

target_link_libraries(${GTEST_APP} PUBLIC GTest::gtest_main)
add_test(NAME ${GTEST_APP} COMMAND $<TARGET_FILE:${GTEST_APP}>)

install(TARGETS ${GTEST_APP} DESTINATION ${CMAKE_INSTALL_BINDIR})
