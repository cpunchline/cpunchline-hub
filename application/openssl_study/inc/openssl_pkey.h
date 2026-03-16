#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

// 生成公钥和私钥

// 私钥格式
// DER: ASN.1 的 Distinguished Encoding Rules
// PEM: PEM = Base64(DER) + 头尾标记
// PKCS#8: 通用私钥格式
// PKCS#1: 仅用于RSA

// 公钥格式(E(公钥指数) N(模数))
// SubjectPublicKeyInfo (SPKI): 符合 X.509 标准
// PKCS#1: 仅用于RSA

#include <stddef.h>

typedef enum _rsa_privatekey_format_e
{
    E_RSA_PRIVATEKEY_FORMAT_PKCS8_PEM = 1, // PKCS#8 PEM
    E_RSA_PRIVATEKEY_FORMAT_PKCS8_DER = 2, // PKCS#8 DER
    E_RSA_PRIVATEKEY_FORMAT_PKCS1_PEM = 3, // only RSA
    E_RSA_PRIVATEKEY_FORMAT_PKCS1_DER = 4, // only RSA
} rsa_privatekey_format_e;

typedef enum _rsa_publickey_format_e
{
    E_RSA_PUBLICKEY_FORMAT_X509_PEM = 1,
    E_RSA_PUBLICKEY_FORMAT_X509_DER = 2,
    E_RSA_PUBLICKEY_FORMAT_PKCS1_PEM = 3, // only RSA
    E_RSA_PUBLICKEY_FORMAT_PKCS1_DER = 4, // only RSA
} rsa_publickey_format_e;

typedef enum _ecc_privatekey_format_e
{
    E_ECC_PRIVATEKEY_FORMAT_PKCS8_PEM = 1,
    E_ECC_PRIVATEKEY_FORMAT_PKCS8_DER = 2,
    E_ECC_PRIVATEKEY_FORMAT_SEC1_PEM = 3,
    E_ECC_PRIVATEKEY_FORMAT_SEC1_DER = 4,
} ecc_privatekey_format_e;

typedef enum _ecc_publickey_format_e
{
    E_ECC_PUBLICKEY_FORMAT_X509_PEM = 1,
    E_ECC_PUBLICKEY_FORMAT_X509_DER = 2,
} ecc_publickey_format_e;

// e default RSA_F4
int openssl_provider_RSA_generate_key(unsigned long e, int rsa_keybits, int private_key_format, char *private_key, size_t *private_key_len, int public_key_format, char *public_key, size_t *public_key_len);
int openssl_provider_ECC_generate_key(const char *curve_name, int private_key_format, char *private_key, size_t *private_key_len, int public_key_format, char *public_key, size_t *public_key_len);
int openssl_provider_ECC_convert_private_key(int key_format, char *key, size_t key_len, int new_key_format, char *new_key, size_t *new_key_len);
int openssl_provider_ECC_convert_public_key(int key_format, char *key, size_t key_len, int new_key_format, char *new_key, size_t *new_key_len);

#ifdef __cplusplus
}
#endif