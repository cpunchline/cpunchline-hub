set(HOTRELOAD_EXAMPLE hotreload)
set(PLUG_LIB plug)

add_library(${PLUG_LIB} SHARED
    src/custom_plug.c
)
target_include_directories(${PLUG_LIB} PRIVATE inc)
install(TARGETS ${PLUG_LIB} DESTINATION ${CMAKE_INSTALL_LIBDIR})

add_executable(${HOTRELOAD_EXAMPLE}
    src/hotreload.c
    src/main.c
)
target_include_directories(${HOTRELOAD_EXAMPLE} PRIVATE inc)
if(WIN32)
    target_compile_definitions(${HOTRELOAD_EXAMPLE} PRIVATE PLUG_SO_PATH="${CPUNCHLINE_OUTPUT_DIR}/lib/lib${PLUG_LIB}.dll")
else()
    target_compile_definitions(${HOTRELOAD_EXAMPLE} PRIVATE PLUG_SO_PATH="${CPUNCHLINE_OUTPUT_DIR}/lib/lib${PLUG_LIB}.so")
endif()
install(TARGETS ${HOTRELOAD_EXAMPLE} DESTINATION ${CMAKE_INSTALL_BINDIR})

# 分别编译一个a=1, b=1和一个a=2, b=2的libplug.so;
# 然后程序运行后时使用一个, 过程中用另一个替换原来的, 可以看到a,b值变化;
