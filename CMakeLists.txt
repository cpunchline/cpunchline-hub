cmake_minimum_required(VERSION 3.20 FATAL_ERROR)

project(CPUNCHLINE LANGUAGES C CXX)

# option设置后不能通过CMake更改, 如果要更改只能通过-D参数更改
# 法一: 始终强制更新缓存 set(CPUNCHLINE_BUILD_TESTS ON CACHE BOOL "Whether or not to build the tests" FORCE) 缓存变量也可用于全局变量(子传父的场景)
# 法二: 删build/CMakeCache.txt重新编译

option(CPUNCHLINE_BUILD_TESTS "Whether or not to build the tests" OFF)
option(CPUNCHLINE_BUILD_COVERAGE "Whether or not to build the tests" OFF)
option(CPUNCHLINE_BUILD_THIRDPARTY "Whether or not to build the thirdparty" OFF)

set(CPUNCHLINE_ROOT_DIR ${PROJECT_SOURCE_DIR})
set(CPUNCHLINE_CMAKE_DIR ${CPUNCHLINE_ROOT_DIR}/cmake)
set(CPUNCHLINE_DOCS_DIR ${CPUNCHLINE_ROOT_DIR}/docs)
set(CPUNCHLINE_INCLUDE_DIR ${CPUNCHLINE_ROOT_DIR}/include)
set(CPUNCHLINE_BASE_DIR ${CPUNCHLINE_ROOT_DIR}/base)
set(CPUNCHLINE_APP_DIR ${CPUNCHLINE_ROOT_DIR}/application)
set(CPUNCHLINE_LIB_DIR ${CPUNCHLINE_ROOT_DIR}/lib)
set(CPUNCHLINE_SCRIPTS_DIR ${CPUNCHLINE_ROOT_DIR}/scripts)
set(CPUNCHLINE_SIMULATOR_DIR ${CPUNCHLINE_ROOT_DIR}/simulator)
set(CPUNCHLINE_TESTS_DIR ${CPUNCHLINE_ROOT_DIR}/tests)
set(CPUNCHLINE_THIRDPARTY_DIR ${CPUNCHLINE_ROOT_DIR}/thirdparty)
set(CPUNCHLINE_TOOLS_DIR ${CPUNCHLINE_ROOT_DIR}/tools)

set(CPUNCHLINE_PRE_BUILT_DIR ${CPUNCHLINE_TOOLS_DIR}/pre_built)
set(CPUNCHLINE_OUTPUT_DIR ${CPUNCHLINE_ROOT_DIR}/output)
set(CPUNCHLINE_BUILD_DIR ${CPUNCHLINE_ROOT_DIR}/build)

list(APPEND CMAKE_MODULE_PATH "${CPUNCHLINE_CMAKE_DIR}")
# 设置<包名>_DIR变量指向<包名>Config.cmake所在目录

if(NOT LINUX)
    message(FATAL_ERROR "Only LINUX is supported!")
endif()

find_package(Threads REQUIRED)

include(toolchain)
include(cross_toolchain)
include(version_info)

configure_file(${CPUNCHLINE_INCLUDE_DIR}/cpunchline_version.h.in ${CPUNCHLINE_INCLUDE_DIR}/cpunchline_version_auto.h)

set(LIB_GOOGLETEST_OPTION ON)
set(LIB_HV_OPTION ON)
set(LIB_LOCKFREE_MPMC_QUEUE_OPTION OFF)
set(LIB_CONCURRENTQUEUE_OPTION ON)
set(LIB_READERWRITERQUEUE_OPTION ON)
set(LIB_LIBZMQ_OPTION ON)
set(LIB_CZMQ_OPTION OFF)
set(LIB_CPPZMQ_OPTION OFF)
set(LIB_NNG_OPTION ON)
set(LIB_UTHASH_OPTION OFF)
set(LIB_NANOPB_OPTION ON)
set(LIB_THREAD_POOL_OPTION ON)
set(LIB_ASIO_OPTION OFF)
set(LIB_SPDLOG_OPTION OFF)
set(LIB_LMDB_OPTION OFF)
set(LIB_MOSQUITTO_OPTION OFF)
set(LIB_SHM_CONTAINER_OPTION OFF)
set(LIB_SQLITE_OPTION ON)
set(LIB_OPENSSL_OPTION ON)

if(CPUNCHLINE_BUILD_THIRDPARTY)
    set(CMAKE_INSTALL_PREFIX "${CPUNCHLINE_PRE_BUILT_DIR}")
    set(NANOPB_SRC_ROOT_FOLDER "${CPUNCHLINE_THIRDPARTY_DIR}/nanopb")
    add_subdirectory(${CPUNCHLINE_THIRDPARTY_DIR})
else()
    set(NANOPB_SRC_ROOT_FOLDER "${CPUNCHLINE_PRE_BUILT_DIR}/nanopb")
    list(APPEND CMAKE_PREFIX_PATH "${CPUNCHLINE_PRE_BUILT_DIR}") # 查找预构建库 FindXXX.cmake(项目自带的) or XXXConfig.cmake(系统的)
    add_subdirectory(${CPUNCHLINE_TOOLS_DIR})
    add_subdirectory(${CPUNCHLINE_BASE_DIR})
    add_subdirectory(${CPUNCHLINE_LIB_DIR})
    add_subdirectory(${CPUNCHLINE_APP_DIR})
    add_subdirectory(${CPUNCHLINE_SIMULATOR_DIR})
    if(CPUNCHLINE_BUILD_TESTS AND LIB_GOOGLETEST_OPTION)
        enable_testing()
        add_subdirectory(${CPUNCHLINE_TESTS_DIR})
    endif()
endif()

find_program(CLANG_TIDY_BIN clang-tidy)
find_program(RUN_CLANG_TIDY_BIN run-clang-tidy)

list(APPEND RUN_CLANG_TIDY_BIN_ARGS
    -clang-tidy-binary ${CLANG_TIDY_BIN}
    # -fix -format -style=file
    -p=${CPUNCHLINE_BUILD_DIR}
    -j8
)

add_custom_target(clang-tidy-check
    COMMAND ${RUN_CLANG_TIDY_BIN} ${RUN_CLANG_TIDY_BIN_ARGS}
    COMMENT "running clang tidy check"
)
