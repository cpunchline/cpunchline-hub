#include "openssl/core_names.h"
#include "openssl/evp.h"
#include "openssl_print.h"
#include "openssl_mac.h"

int openssl_provider_hmac(const char *digest_name, uint8_t *key, size_t key_len, const void *_src, size_t src_len, void *dest, size_t *dest_len)
{
    int ret = 0;
    EVP_MAC *mac = NULL;
    EVP_MAC_CTX *ctx = NULL;

    if (NULL == digest_name || NULL == key || NULL == _src || NULL == dest || NULL == dest_len)
    {
        LOG_PRINT_ERROR("invalid param!");
        return -1;
    }

    do
    {
        mac = EVP_MAC_fetch(NULL, OSSL_MAC_NAME_HMAC, NULL);
        if (NULL == mac)
        {
            LOG_PRINT_ERROR("EVP_MD_fetch fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        ctx = EVP_MAC_CTX_new(mac);
        if (NULL == ctx)
        {
            LOG_PRINT_ERROR("EVP_MAC_CTX_new fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        // select digest algorithm
        OSSL_PARAM params[] = {
            OSSL_PARAM_utf8_string(OSSL_MAC_PARAM_DIGEST, (char *)digest_name, strlen(digest_name)),
            OSSL_PARAM_END,
        };

        if (0 == EVP_MAC_init(ctx, key, key_len, params))
        {
            LOG_PRINT_ERROR("EVP_MAC_init fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        size_t mac_size = EVP_MAC_CTX_get_mac_size(ctx);
        if (*dest_len < mac_size)
        {
            LOG_PRINT_ERROR("dest_len[%zu] < mac_size[%zu]!", *dest_len, mac_size);
            ret = -1;
            break;
        }

        if (0 == EVP_MAC_update(ctx, (const unsigned char *)_src, src_len))
        {
            LOG_PRINT_ERROR("EVP_MAC_update fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (0 == EVP_MAC_final(ctx, dest, dest_len, *dest_len))
        {
            LOG_PRINT_ERROR("EVP_MAC_final fail!");
            print_openssl_err();
            ret = -1;
            break;
        }
    } while (0);

    if (NULL != ctx)
    {
        EVP_MAC_CTX_free(ctx);
    }

    if (NULL != mac)
    {
        EVP_MAC_free(mac);
    }

    return ret;
}

int openssl_provider_cmac(const char *cipher_name, uint8_t *key, size_t key_len, const void *_src, size_t src_len, void *dest, size_t *dest_len)
{
    int ret = 0;
    EVP_CIPHER *cipher_ctx = NULL;
    EVP_MAC *mac = NULL;
    EVP_MAC_CTX *ctx = NULL;

    if (NULL == cipher_name || NULL == key || NULL == _src || NULL == dest || NULL == dest_len)
    {
        LOG_PRINT_ERROR("invalid param!");
        return -1;
    }

    do
    {
        cipher_ctx = EVP_CIPHER_fetch(NULL, cipher_name, NULL);
        if (cipher_ctx == NULL)
        {
            LOG_PRINT_ERROR("EVP_CIPHER_fetch fail for cipher: %s", cipher_name);
            print_openssl_err();
            ret = -1;
            break;
        }

        size_t expected_key_len = (size_t)EVP_CIPHER_get_key_length(cipher_ctx);
        if (key_len != expected_key_len)
        {
            LOG_PRINT_ERROR("key_len[%zu] != expected_key_len[%zu]!", key_len, expected_key_len);
            ret = -1;
            break;
        }

        mac = EVP_MAC_fetch(NULL, OSSL_MAC_NAME_CMAC, NULL);
        if (NULL == mac)
        {
            LOG_PRINT_ERROR("EVP_MD_fetch fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        ctx = EVP_MAC_CTX_new(mac);
        if (NULL == ctx)
        {
            LOG_PRINT_ERROR("EVP_MAC_CTX_new fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        // select digest algorithm
        OSSL_PARAM params[] = {
            OSSL_PARAM_utf8_string(OSSL_MAC_PARAM_CIPHER, (char *)cipher_name, strlen(cipher_name)),
            OSSL_PARAM_END,
        };

        if (0 == EVP_MAC_init(ctx, key, key_len, params))
        {
            LOG_PRINT_ERROR("EVP_MAC_init fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        size_t mac_size = EVP_MAC_CTX_get_mac_size(ctx);
        if (*dest_len < mac_size)
        {
            LOG_PRINT_ERROR("dest_len[%zu] < mac_size[%zu]!", *dest_len, mac_size);
            ret = -1;
            break;
        }

        if (0 == EVP_MAC_update(ctx, (const unsigned char *)_src, src_len))
        {
            LOG_PRINT_ERROR("EVP_MAC_update fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (0 == EVP_MAC_final(ctx, dest, dest_len, *dest_len))
        {
            LOG_PRINT_ERROR("EVP_MAC_final fail!");
            print_openssl_err();
            ret = -1;
            break;
        }
    } while (0);

    if (NULL != cipher_ctx)
    {
        EVP_CIPHER_free(cipher_ctx);
    }

    if (NULL != ctx)
    {
        EVP_MAC_CTX_free(ctx);
    }

    if (NULL != mac)
    {
        EVP_MAC_free(mac);
    }

    return ret;
}