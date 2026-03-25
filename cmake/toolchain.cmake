if(NOT WIN32)
    string(ASCII 27 Esc)
    set(ColorReset "${Esc}[m")
    set(ColorBold "${Esc}[1m")
    set(Red "${Esc}[31m")
    set(Green "${Esc}[32m")
    set(Yellow "${Esc}[33m")
    set(Blue "${Esc}[34m")
    set(Magenta "${Esc}[35m")
    set(Cyan "${Esc}[36m")
    set(White "${Esc}[37m")
    set(BoldRed "${Esc}[1;31m")
    set(BoldGreen "${Esc}[1;32m")
    set(BoldYellow "${Esc}[1;33m")
    set(BoldBlue "${Esc}[1;34m")
    set(BoldMagenta "${Esc}[1;35m")
    set(BoldCyan "${Esc}[1;36m")
    set(BoldWhite "${Esc}[1;37m")
endif()

include(GNUInstallDirs)

message(STATUS "${Yellow}${CMAKE_GENERATOR}${ColorReset}")

# cmake --system-information information.txt
cmake_host_system_information(RESULT OS_INFO QUERY OS_PLATFORM OS_NAME DISTRIB_PRETTY_NAME OS_RELEASE)
cmake_host_system_information(RESULT PROCESSOR_INFO QUERY PROCESSOR_DESCRIPTION NUMBER_OF_PHYSICAL_CORES TOTAL_PHYSICAL_MEMORY IS_64BIT)
cmake_host_system_information(RESULT DISTRO QUERY DISTRIB_INFO)
message(STATUS "${Yellow}${OS_INFO}${ColorReset}")
message(STATUS "${Yellow}${PROCESSOR_INFO}${ColorReset}")

# set(CMAKE_C_STANDARD 23)
# set(CMAKE_CXX_STANDARD 26)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# set(CMAKE_VERBOSE_MAKEFILE ON)

if(${CMAKE_EXPORT_COMPILE_COMMANDS})
    set(CMAKE_C_STANDARD_INCLUDE_DIRECTORIES ${CMAKE_C_IMPLICIT_INCLUDE_DIRECTORIES})
    set(CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES ${CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES})
endif()

set(CMAKE_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/output")

# export LD_LIBRARY_PATH=~/CPUNCHLINE_HUB/output/lib/:$LD_LIBRARY_PATH
set(CMAKE_SKIP_BUILD_RPATH OFF) # 不要跳过构建时的RPATH设置; 编译生成的可执行文件或库将包含一个路径列表, 该列表指定了在运行时应查找动态库的位置
set(CMAKE_BUILD_WITH_INSTALL_RPATH OFF) # CMake在构建(编译)时使用的RPATH应当与最终安装时的RPATH不相同
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH OFF) # 最终安装的RPATH时, 合并链接步骤中使用的库路径(比如指向本地构建的库或第三方库的临时安装目录), 启用这个选项可能会导致这些路径被包含到最终的RPATH中)
set(CMAKE_INSTALL_RPATH "$ORIGIN:$ORIGIN/lib:$ORIGIN/../lib:$ORIGIN/lib64:$ORIGIN/../lib64") # 确保在安装后, 程序能够正确地在指定的安装目录下查找动态链接库

if(NOT DEFINED BUILD_SHARED_LIBS)
    set(BUILD_SHARED_LIBS ON) # 默认编译动态库
endif()

if(NOT MSVC)
    find_program(CCACHE_PROGRAM ccache)
    if(CCACHE_PROGRAM)
        message(STATUS "${Green}Found CCache: ${CCACHE_PROGRAM} ${ColorReset}")
        set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ${CCACHE_PROGRAM})
        set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ${CCACHE_PROGRAM})
        set(CMAKE_C_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
        set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Qunused-arguments -fcolor-diagnostics")
        endif()
    endif()

    # Reproducible Builds
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ffile-prefix-map=${CMAKE_SOURCE_DIR}=. -fdebug-prefix-map=${CMAKE_SOURCE_DIR}=.")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffile-prefix-map=${CMAKE_SOURCE_DIR}=. -fdebug-prefix-map=${CMAKE_SOURCE_DIR}=.")
endif()

set(CMAKE_POSITION_INDEPENDENT_CODE ON) # bin -fPIE; lib -fPIC;
set(COMMON_WARNING_FLAGS "-Wall -Wextra -Wuninitialized -Wno-unused-function -Wredundant-decls -Wdate-time")
set(FORMAT_WARNING_FLAGS "-Wformat=2 -Wformat-security")
set(CONVERSION_WARNING_FLAGS "-Wsign-conversion -Wfloat-equal -Wfloat-conversion -Wconversion")
set(LIMIT_WARNING_FLAGS "-Waddress -Warray-bounds -Wdiv-by-zero -Wshadow -Wswitch-enum -Wswitch-default -Wfree-nonheap-object -Wsizeof-pointer-div")
set(SECRYPT_FLAGS "-D_FORTIFY_SOURCE=2 -fstack-protector-strong")
set(SECRYPT_LINKER_FLAGS "-Wl,-z,noexecstack -Wl,-z,relro -Wl,-z,now") # -pie only use to bin

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${COMMON_WARNING_FLAGS} ${FORMAT_WARNING_FLAGS} ${CONVERSION_WARNING_FLAGS} ${LIMIT_WARNING_FLAGS} ${SECRYPT_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COMMON_WARNING_FLAGS} ${FORMAT_WARNING_FLAGS} ${CONVERSION_WARNING_FLAGS} ${LIMIT_WARNING_FLAGS} ${SECRYPT_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}  -Wl,--hash-style=gnu -Wl,--as-needed -Wl,--enable-new-dtags -pie ${SECRYPT_LINKER_FLAGS}")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--hash-style=gnu -Wl,--as-needed -Wl,--enable-new-dtags ${SECRYPT_LINKER_FLAGS}")

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-missing-field-initializers")
endif()

if(NOT CPUNCHLINE_BUILD_THIRDPARTY)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror -Wfatal-errors")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror -Wfatal-errors")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -funwind-tables -fno-omit-frame-pointer -fno-optimize-sibling-calls -feliminate-unused-debug-types")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -funwind-tables -fno-omit-frame-pointer -fno-optimize-sibling-calls -feliminate-unused-debug-types")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -rdynamic")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -rdynamic")
else()
    include(CheckIPOSupported)
    check_ipo_supported(RESULT iposupported OUTPUT iposupporterror)
    if(iposupported)
        message(STATUS "${Yellow}IPO / LTO enabled${ColorReset}")
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
        cmake_policy(SET CMP0069 NEW)
    else()
        message(STATUS "${Red}IPO / LTO not supported: <${iposupporterror}>${ColorReset}")
    endif()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ffunction-sections -fdata-sections")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffunction-sections -fdata-sections")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--gc-sections")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--gc-sections")
endif()

function(find_package_from_path package path)
    set(_tmp_path "${CMAKE_PREFIX_PATH}")
    set(CMAKE_PREFIX_PATH "${path}")
    find_package("${package}" REQUIRED)
    set(CMAKE_PREFIX_PATH "${_tmp_path}")
endfunction()
