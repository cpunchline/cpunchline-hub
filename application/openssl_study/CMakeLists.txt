find_package_from_path(OpenSSL ${CPUNCHLINE_PRE_BUILT_DIR})

set(OPENSSL_STUDY openssl_study)

# 常用密码学 https://subingwen.cn/golang/cryptology/
add_executable(${OPENSSL_STUDY}
    src/openssl_study.c
    src/openssl_print.c
    src/openssl_base64.c
    src/openssl_md.c # 哈希/摘要算法
    src/openssl_mac.c # 消息认证码
    src/openssl_cipher.c # 对称加密算法
    src/openssl_asym_cipher.c # 非对称加密算法
    src/openssl_pkey.c # 密钥操作
    src/openssl_ssl_tls.c # ssl/tls
)
target_include_directories(${OPENSSL_STUDY} PRIVATE inc)
target_link_libraries(${OPENSSL_STUDY} PRIVATE OpenSSL::Crypto OpenSSL::SSL base)
target_compile_definitions(${OPENSSL_STUDY} PRIVATE
    OPENSSLDIR="${CPUNCHLINE_PRE_BUILT_DIR}/ssl"
    MODULESDIR="${CPUNCHLINE_PRE_BUILT_DIR}/lib/ossl-modules"
    EMGOMESDIR="${CPUNCHLINE_PRE_BUILT_DIR}/lib/engines-3"
    SSLTESTDIR="${CPUNCHLINE_ROOT_DIR}/cert"
)
install(TARGETS ${OPENSSL_STUDY} DESTINATION ${CMAKE_INSTALL_BINDIR})
