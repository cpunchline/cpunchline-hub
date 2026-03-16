#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include "openssl/err.h"
#include "openssl/core_names.h"
#include "openssl/conf.h"
#include "openssl/crypto.h"
#include "openssl/ssl.h"
#include "openssl/provider.h"
#include "openssl/evp.h"
#include "openssl/kdf.h"
#include "openssl/encoder.h"
#include "openssl/decoder.h"
#include "openssl/store.h"
#include "openssl/rand.h"

#define PRINT_SUPPORTED_ALGORITHM_FLAG (1)

// OSSL_PROVIDER              提供者
// EVP_CIPHER                 对称加密算法
// EVP_MD                     哈希/摘要算法
// EVP_MAC                    消息认证码
// EVP_KDF                    密钥派生函数
// EVP_PKEY, EVP_PKEY_CTX     密钥生成/签名/加密/交换等;

/*
BIO
BIO对象: BIO *

BIO *BIO_new(const BIO_METHOD *type); // 创建BIO对象
BIO_METHOD分为:6种过滤器; 8种数据源;
    数据源source/sink:
        BIO_s_mem();
        BIO_s_socket();
        BIO_s_fd();
        BIO_s_file();
    过滤器filter      BIO_f_xxx();

BIO *BIO_push(BIO *b, BIO *append);
// BIO链: 将数据源交给过滤器处理: 包含一个source BIO, 一个或多个filter BIO;
// 例如: 将一个内存数据先经过base64加密, 在经过md5加密等, 就需要这样的链式处理;

int BIO_write(BIO *b, const void *data, int dlen);                   写编码(链条头->尾)
int BIO_read_ex(BIO *b, void *data, size_t dlen, size_t *readbytes); 读解码(链条尾->头)
**/

#ifdef __cplusplus
}
#endif
