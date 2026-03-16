#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include "openssl/err.h"
#include "utility/utils.h"

static inline void print_openssl_err(void)
{
    // OpenSSL 的错误码是一个 32 位 unsigned long, 其详细内容查看openssl/err.h中的详细内部结构;
    // OpenSSL errcode: [0][8-bit lib][23-bit reason(5-bit rflags + [1 ERR_RFLAG_FATAL/ERR_RFLAG_COMMON] + 18-bits reason)]

    unsigned long err = 0;
    char errbuf[256] = {};
    // errbuf format: error:<library code>:<library name>:<function name(disuse)>:<reason>

    while ((err = ERR_get_error()) != 0)
    {
        ERR_error_string_n(err, errbuf, sizeof(errbuf));
        LOG_PRINT_ERROR("%s", errbuf);
    }
}

void print_openssl_info(void);

#ifdef __cplusplus
}
#endif
