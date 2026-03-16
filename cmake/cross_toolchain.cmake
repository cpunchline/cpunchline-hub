# cmake 版本4.0以上

# 交叉编译
# 新式写法 cmake --toolchain toolchain_dir --install-prefix output_dir -DCMAKE_SYSROOT=sysroot_dir
# 老式写法 cmake -DCMAKE_TOOLCHAIN_FILE=toolchain_dir -DCMAKE_INSTALL_PREFILX=output_dir -DCMAKE_SYSROOT=sysroot_dir

# 交叉编译rpath问题
# set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-rpath-link,lib_path")
# set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-rpath-link,lib_path")

# cross_toolchain.cmake
# set(CMAKE_SYSTEM_NAME Linux)
# set(CMAKE_SYSTEM_PROCESSOR processor_name) # arm aarch64

# get_filename_component(CURRENT_SCRIPT_DIR "${CMAKE_TOOLCHAIN_FILE}" DIRECTORY)

if(CMAKE_CROSSCOMPILING)
    message(STATUS "${ColorReset}cross compilation${ColorReset}")
    message(STATUS "${Yellow}CMAKE_SYSTEM_NAME      = ${CMAKE_SYSTEM_NAME}${ColorReset}")
    message(STATUS "${Yellow}CMAKE_SYSTEM_PROCESSOR = ${CMAKE_SYSTEM_PROCESSOR}${ColorReset}")
else()
    message(STATUS "${Yellow}non-cross compilation${ColorReset}")
endif()
# set(CMAKE_C_COMPILER ${C_COMPILER_PATH} CACHE PATH "C compiler path" FORCE)
# set(CMAKE_CXX_COMPILER ${CXX_COMPILER_PATH} CACHE PATH "C++ compiler path" FORCE)
# set(CMAKE_STRIP "strip")

# set(CMAKE_C_COMPILER_WORKS ON)
# set(CMAKE_CXX_COMPILER_WORKS ON)

# set(CMAKE_FIND_ROOT_PATH sysroot_path)

# NEVER 本地系统; ONLY 目标系统; BOTH 都用;
# set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM_ENV ONLY)
# set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
# set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
# set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# link_directories(${CMAKE_SYSROOT}/lib64)
# link_directories(${CMAKE_SYSROOT}/usr/lib64)
