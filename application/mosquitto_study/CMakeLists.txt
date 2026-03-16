find_package_from_path(OpenSSL ${CPUNCHLINE_PRE_BUILT_DIR})

set(MOSQUITTO_STUDY mosquitto_study)

# 常用密码学 https://subingwen.cn/golang/cryptology/
add_executable(${MOSQUITTO_STUDY}
    mosquitto_local_client.c
)
target_include_directories(${MOSQUITTO_STUDY} PRIVATE inc)
target_link_libraries(${MOSQUITTO_STUDY} PRIVATE mosquitto OpenSSL::Crypto OpenSSL::SSL base)
if(NOT CPUNCHLINE_BUILD_THIRDPARTY)
    target_link_directories(${MOSQUITTO_STUDY} PRIVATE ${CPUNCHLINE_PRE_BUILT_DIR}/lib)
    target_include_directories(${MOSQUITTO_STUDY} PRIVATE ${CPUNCHLINE_PRE_BUILT_DIR}/include)
else()
    target_include_directories(${MOSQUITTO_STUDY} PRIVATE ${CPUNCHLINE_THIRDPARTY_DIR}/mosquitto/include)
endif()

install(TARGETS ${MOSQUITTO_STUDY} DESTINATION ${CMAKE_INSTALL_BINDIR})
