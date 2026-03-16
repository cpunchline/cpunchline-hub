#include <sys/stat.h>
#include "openssl/core_names.h"
#include "openssl/evp.h"
#include "openssl_print.h"
#include "openssl_cipher.h"

#define DEFAULT_EACH_HANDLE_READ_BLOCK_SIZE (64)

int openssl_provider_cipher_encrypt(const char *cipher_name, int padding, uint8_t *iv, size_t iv_len, uint8_t *key, size_t key_len, const void *_src, size_t src_len, void *dest, size_t *dest_len)
{
    int ret = 0;
    EVP_CIPHER_CTX *ctx = NULL;
    EVP_CIPHER *cipher = NULL;
    int out1 = 0;
    int out2 = 0;

    if (NULL == cipher_name || NULL == key || NULL == _src || NULL == dest || NULL == dest_len)
    {
        LOG_PRINT_ERROR("invalid param!");
        return -1;
    }

    if (iv_len == 0)
    {
        iv = NULL; // ecb no need iv;
    }

    do
    {
        cipher = EVP_CIPHER_fetch(NULL, cipher_name, NULL);
        if (NULL == cipher)
        {
            LOG_PRINT_ERROR("EVP_CIPHER_fetch fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (0 == padding)
        {
            if (0 != src_len % (size_t)EVP_CIPHER_get_block_size(cipher))
            {
                LOG_PRINT_ERROR("no padding, src_len[%zu] must be integer multiple of block_size[%d]!", src_len, EVP_CIPHER_get_block_size(cipher));
                ret = -1;
                break;
            }
        }

        size_t expected_key_len = (size_t)EVP_CIPHER_get_key_length(cipher);
        if (key_len != expected_key_len)
        {
            LOG_PRINT_ERROR("key_len[%zu] != expected_key_len[%zu]!", key_len, expected_key_len);
            ret = -1;
            break;
        }

        size_t expected_iv_len = (size_t)EVP_CIPHER_get_iv_length(cipher);
        if (iv_len != expected_iv_len)
        {
            LOG_PRINT_ERROR("iv_len[%zu] != expected_iv_len[%zu]!", iv_len, expected_iv_len);
            ret = -1;
            break;
        }

        ctx = EVP_CIPHER_CTX_new();
        if (NULL == ctx)
        {
            LOG_PRINT_ERROR("EVP_CIPHER_CTX_new fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (0 == EVP_EncryptInit_ex2(ctx, cipher, key, iv, NULL))
        {
            LOG_PRINT_ERROR("EVP_EncryptInit_ex2 fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (0 == EVP_CIPHER_CTX_set_padding(ctx, padding))
        {
            LOG_PRINT_ERROR("EVP_CIPHER_CTX_set_padding fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        // handle complete block by no padding
        if (0 == EVP_EncryptUpdate(ctx, dest, &out1, _src, (int)src_len))
        {
            LOG_PRINT_ERROR("EVP_EncryptUpdate fail!");
            print_openssl_err();
            ret = -1;
            break;
        }
        *dest_len = (size_t)out1;

        // handle last not complete block by set padding
        if (0 == EVP_EncryptFinal_ex(ctx, dest + out1, &out2))
        {
            LOG_PRINT_ERROR("EVP_EncryptFinal_ex fail!");
            print_openssl_err();
            ret = -1;
            break;
        }
        *dest_len += (size_t)out2;
    } while (0);

    if (NULL != ctx)
    {
        EVP_CIPHER_CTX_free(ctx);
    }

    if (NULL != cipher)
    {
        EVP_CIPHER_free(cipher);
    }

    return ret;
}

int openssl_provider_cipher_decrypt(const char *cipher_name, int padding, uint8_t *iv, size_t iv_len, uint8_t *key, size_t key_len, const void *_src, size_t src_len, void *dest, size_t *dest_len)
{
    int ret = 0;

    EVP_CIPHER *cipher = NULL;
    EVP_CIPHER_CTX *ctx = NULL;
    int out1 = 0;
    int out2 = 0;

    if (NULL == cipher_name || NULL == key || NULL == _src || NULL == dest || NULL == dest_len)
    {
        LOG_PRINT_ERROR("invalid param!");
        return -1;
    }

    if (iv_len == 0)
    {
        iv = NULL; // ecb no need iv;
    }

    do
    {
        cipher = EVP_CIPHER_fetch(NULL, cipher_name, NULL);
        if (NULL == cipher)
        {
            LOG_PRINT_ERROR("EVP_CIPHER_fetch fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        size_t expected_key_len = (size_t)EVP_CIPHER_get_key_length(cipher);
        if (key_len != expected_key_len)
        {
            LOG_PRINT_ERROR("key_len[%zu] != expected_key_len[%zu]!", key_len, expected_key_len);
            ret = -1;
            break;
        }

        size_t expected_iv_len = (size_t)EVP_CIPHER_get_iv_length(cipher);
        if (iv_len != expected_iv_len)
        {
            LOG_PRINT_ERROR("iv_len[%zu] != expected_iv_len[%zu]!", iv_len, expected_iv_len);
            ret = -1;
            break;
        }

        ctx = EVP_CIPHER_CTX_new();
        if (NULL == ctx)
        {
            LOG_PRINT_ERROR("EVP_CIPHER_CTX_new fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (0 == EVP_DecryptInit_ex2(ctx, cipher, key, iv, NULL))
        {
            LOG_PRINT_ERROR("EVP_DecryptInit_ex2 fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (0 == EVP_CIPHER_CTX_set_padding(ctx, padding))
        {
            LOG_PRINT_ERROR("EVP_CIPHER_CTX_set_padding fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (0 == EVP_DecryptUpdate(ctx, dest, &out1, _src, (int)src_len))
        {
            LOG_PRINT_ERROR("EVP_DecryptUpdate fail!");
            print_openssl_err();
            ret = -1;
            break;
        }
        *dest_len = (size_t)out1;

        if (0 == EVP_DecryptFinal_ex(ctx, dest + out1, &out2))
        {
            LOG_PRINT_ERROR("EVP_DecryptFinal_ex fail!");
            print_openssl_err();
            ret = -1;
            break;
        }
        *dest_len += (size_t)out2;
    } while (0);

    if (NULL != ctx)
    {
        EVP_CIPHER_CTX_free(ctx);
    }

    if (NULL != cipher)
    {
        EVP_CIPHER_free(cipher);
    }

    return ret;
}

int openssl_provider_cipher_aead_encrypt(const char *cipher_name, const uint8_t *nonce, size_t nonce_len, uint8_t *aad, size_t aad_len, uint8_t *key, size_t key_len, const void *_src, size_t src_len, void *dest, size_t *dest_len, uint8_t *tag, size_t tag_len)
{
    int ret = 0;
    EVP_CIPHER_CTX *ctx = NULL;
    EVP_CIPHER *cipher = NULL;
    int out1 = 0;
    int out2 = 0;

    if (NULL == cipher_name || NULL == nonce || NULL == key || NULL == _src || NULL == dest || NULL == dest_len || NULL == tag)
    {
        LOG_PRINT_ERROR("invalid param!");
        return -1;
    }

    if (aad_len > 0 && NULL == aad)
    {
        LOG_PRINT_ERROR("aad_len > 0 but aad is NULL!");
        return -1;
    }

    do
    {
        cipher = EVP_CIPHER_fetch(NULL, cipher_name, NULL);
        if (NULL == cipher)
        {
            LOG_PRINT_ERROR("EVP_CIPHER_fetch fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        size_t expected_key_len = (size_t)EVP_CIPHER_get_key_length(cipher);
        if (key_len != expected_key_len)
        {
            LOG_PRINT_ERROR("key_len[%zu] != expected_key_len[%zu]!", key_len, expected_key_len);
            ret = -1;
            break;
        }

        size_t expected_nonce_len = (size_t)EVP_CIPHER_get_iv_length(cipher);
        if (nonce_len != expected_nonce_len)
        {
            LOG_PRINT_ERROR("nonce_len[%zu] != expected_nonce_len[%zu]!", nonce_len, expected_nonce_len);
            ret = -1;
            break;
        }

        ctx = EVP_CIPHER_CTX_new();
        if (NULL == ctx)
        {
            LOG_PRINT_ERROR("EVP_CIPHER_CTX_new fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (0 == EVP_EncryptInit_ex2(ctx, cipher, key, nonce, NULL))
        {
            LOG_PRINT_ERROR("EVP_EncryptInit_ex2 fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        // handle aad
        if (aad_len > 0)
        {
            int outlen = 0;
            if (0 == EVP_EncryptUpdate(ctx, NULL, &outlen, aad, (int)aad_len))
            {
                LOG_PRINT_ERROR("EVP_EncryptUpdate (AAD) fail!");
                print_openssl_err();
                ret = -1;
                break;
            }
        }

        // handle complete block
        if (0 == EVP_EncryptUpdate(ctx, dest, &out1, _src, (int)src_len))
        {
            LOG_PRINT_ERROR("EVP_EncryptUpdate fail!");
            print_openssl_err();
            ret = -1;
            break;
        }
        *dest_len = (size_t)out1;

        // handle end
        if (0 == EVP_EncryptFinal_ex(ctx, dest + out1, &out2))
        {
            LOG_PRINT_ERROR("EVP_EncryptFinal_ex fail!");
            print_openssl_err();
            ret = -1;
            break;
        }
        *dest_len += (size_t)out2;

        // get tag
        if (0 == EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, (int)tag_len, (void *)tag))
        {
            LOG_PRINT_ERROR("EVP_CTRL_AEAD_GET_TAG fail!");
            print_openssl_err();
            ret = -1;
            break;
        }
        ret = 0;
    } while (0);

    if (NULL != ctx)
    {
        EVP_CIPHER_CTX_free(ctx);
    }

    if (NULL != cipher)
    {
        EVP_CIPHER_free(cipher);
    }

    return ret;
}

int openssl_provider_cipher_aead_decrypt(const char *cipher_name, const uint8_t *nonce, size_t nonce_len, uint8_t *aad, size_t aad_len, uint8_t *key, size_t key_len, const void *_src, size_t src_len, void *dest, size_t *dest_len, uint8_t *tag, size_t tag_len)
{
    int ret = 0;
    EVP_CIPHER_CTX *ctx = NULL;
    EVP_CIPHER *cipher = NULL;
    int out1 = 0;
    int out2 = 0;

    if (NULL == cipher_name || NULL == nonce || NULL == key || NULL == _src || NULL == dest || NULL == dest_len || NULL == tag)
    {
        LOG_PRINT_ERROR("invalid param!");
        return -1;
    }

    if (aad_len > 0 && NULL == aad)
    {
        LOG_PRINT_ERROR("aad_len > 0 but aad is NULL!");
        return -1;
    }

    do
    {
        cipher = EVP_CIPHER_fetch(NULL, cipher_name, NULL);
        if (NULL == cipher)
        {
            LOG_PRINT_ERROR("EVP_CIPHER_fetch fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        size_t expected_key_len = (size_t)EVP_CIPHER_get_key_length(cipher);
        if (key_len != expected_key_len)
        {
            LOG_PRINT_ERROR("key_len[%zu] != expected_key_len[%zu]!", key_len, expected_key_len);
            ret = -1;
            break;
        }

        size_t expected_nonce_len = (size_t)EVP_CIPHER_get_iv_length(cipher);
        if (nonce_len != expected_nonce_len)
        {
            LOG_PRINT_ERROR("nonce_len[%zu] != expected_nonce_len[%zu]!", nonce_len, expected_nonce_len);
            ret = -1;
            break;
        }

        ctx = EVP_CIPHER_CTX_new();
        if (NULL == ctx)
        {
            LOG_PRINT_ERROR("EVP_CIPHER_CTX_new fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (0 == EVP_DecryptInit_ex2(ctx, cipher, key, nonce, NULL))
        {
            LOG_PRINT_ERROR("EVP_DecryptInit_ex2 fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (0 == EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, (int)tag_len, (void *)tag))
        {
            LOG_PRINT_ERROR("EVP_CIPHER_CTX_ctrl fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        // handle aad
        if (aad_len > 0)
        {
            int outlen = 0;
            if (0 == EVP_DecryptUpdate(ctx, NULL, &outlen, aad, (int)aad_len))
            {
                LOG_PRINT_ERROR("EVP_DecryptUpdate (AAD) fail!");
                print_openssl_err();
                ret = -1;
                break;
            }
        }

        // handle complete block
        if (0 == EVP_DecryptUpdate(ctx, dest, &out1, _src, (int)src_len))
        {
            LOG_PRINT_ERROR("EVP_DecryptUpdate fail!");
            print_openssl_err();
            ret = -1;
            break;
        }
        *dest_len = (size_t)out1;

        // verify tag
        if (0 == EVP_DecryptFinal_ex(ctx, dest + out1, &out2))
        {
            LOG_PRINT_ERROR("EVP_DecryptFinal_ex fail!");
            print_openssl_err();
            ret = -1;
            break;
        }
        *dest_len += (size_t)out2;
        ret = 0;
    } while (0);

    if (NULL != ctx)
    {
        EVP_CIPHER_CTX_free(ctx);
    }

    if (NULL != cipher)
    {
        EVP_CIPHER_free(cipher);
    }

    return ret;
}

int openssl_provider_cipher_encrypt_file(const char *cipher_name, int padding, uint8_t *iv, size_t iv_len, uint8_t *key, size_t key_len, size_t each_handle_block_size, char *src_file_path, char *dest_file_path)
{
    int ret = 0;
    BIO *src_bio = NULL;
    BIO *dest_bio = NULL;
    EVP_CIPHER_CTX *ctx = NULL;
    EVP_CIPHER *cipher = NULL;

    size_t default_each_handle_read_block_size = DEFAULT_EACH_HANDLE_READ_BLOCK_SIZE;
    size_t each_handle_write_block_size = 0;
    uint8_t *each_handle_read_block = NULL;
    int each_handle_read = 0;
    uint8_t *each_handle_write_block = NULL;
    int each_handle_write = 0;
    int out = 0;

    if (NULL == cipher_name || NULL == key || NULL == src_file_path || NULL == dest_file_path)
    {
        LOG_PRINT_ERROR("invalid param!");
        return -1;
    }

    if (iv_len == 0)
    {
        iv = NULL; // ecb no need iv;
    }

    do
    {
        default_each_handle_read_block_size = (each_handle_block_size > default_each_handle_read_block_size) ? each_handle_block_size : default_each_handle_read_block_size;
        each_handle_read_block = calloc(1, default_each_handle_read_block_size);
        if (NULL == each_handle_read_block)
        {
            LOG_PRINT_ERROR("calloc fail, err[%d](%s)", errno, strerror(errno));
            break;
        }

        cipher = EVP_CIPHER_fetch(NULL, cipher_name, NULL);
        if (NULL == cipher)
        {
            LOG_PRINT_ERROR("EVP_CIPHER_fetch fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (0 == padding)
        {
            struct stat st;
            if (0 != stat(src_file_path, &st))
            {
                LOG_PRINT_ERROR("stat fail, err[%d](%s)", errno, strerror(errno));
                ret = -1;
                break;
            }

            if (0 != (size_t)st.st_size % (size_t)EVP_CIPHER_get_block_size(cipher))
            {
                LOG_PRINT_ERROR("no padding, file_size[%zu] must be integer multiple of block_size[%d]!", (size_t)st.st_size, EVP_CIPHER_get_block_size(cipher));
                ret = -1;
                break;
            }
        }

        each_handle_write_block_size = default_each_handle_read_block_size + (size_t)EVP_CIPHER_get_block_size(cipher);
        each_handle_write_block = calloc(1, each_handle_write_block_size);
        if (NULL == each_handle_write_block)
        {
            LOG_PRINT_ERROR("calloc fail, err[%d](%s)", errno, strerror(errno));
            break;
        }

        size_t expected_key_len = (size_t)EVP_CIPHER_get_key_length(cipher);
        if (key_len != expected_key_len)
        {
            LOG_PRINT_ERROR("key_len[%zu] != expected_key_len[%zu]!", key_len, expected_key_len);
            ret = -1;
            break;
        }

        size_t expected_iv_len = (size_t)EVP_CIPHER_get_iv_length(cipher);
        if (iv_len != expected_iv_len)
        {
            LOG_PRINT_ERROR("iv_len[%zu] != expected_iv_len[%zu]!", iv_len, expected_iv_len);
            ret = -1;
            break;
        }

        ctx = EVP_CIPHER_CTX_new();
        if (NULL == ctx)
        {
            LOG_PRINT_ERROR("EVP_CIPHER_CTX_new fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (0 == EVP_EncryptInit_ex2(ctx, cipher, key, iv, NULL))
        {
            LOG_PRINT_ERROR("EVP_EncryptInit_ex2 fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (0 == EVP_CIPHER_CTX_set_padding(ctx, padding))
        {
            LOG_PRINT_ERROR("EVP_CIPHER_CTX_set_padding fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        src_bio = BIO_new_file(src_file_path, "rb");
        if (NULL == src_bio)
        {
            LOG_PRINT_ERROR("BIO_new_file fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        dest_bio = BIO_new_file(dest_file_path, "wb");
        if (NULL == dest_bio)
        {
            LOG_PRINT_ERROR("BIO_new_file fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        while ((each_handle_read = BIO_read(src_bio, each_handle_read_block, (int)default_each_handle_read_block_size)) > 0)
        {
            // handle complete block by no padding
            if (0 == EVP_EncryptUpdate(ctx, each_handle_write_block, &out, each_handle_read_block, each_handle_read))
            {
                LOG_PRINT_ERROR("EVP_EncryptUpdate fail!");
                print_openssl_err();
                ret = -1;
                break;
            }

            each_handle_write = BIO_write(dest_bio, each_handle_write_block, out);
            if (each_handle_write < 0 || each_handle_write != out)
            {
                LOG_PRINT_ERROR("BIO_write fail!");
                print_openssl_err();
                ret = -1;
                break;
            }
        }

        if (each_handle_read < 0)
        {
            if (BIO_should_retry(src_bio))
            {
                LOG_PRINT_ERROR("BIO_read should retry (unexpected in blocking mode)");
            }
            else
            {
                LOG_PRINT_ERROR("BIO_read failed with error");
                print_openssl_err();
            }
            ret = -1;
            break;
        }

        // handle last not complete block by set padding
        if (0 == EVP_EncryptFinal_ex(ctx, each_handle_write_block, &out))
        {
            LOG_PRINT_ERROR("EVP_EncryptFinal_ex fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (out > 0)
        {
            each_handle_write = BIO_write(dest_bio, each_handle_write_block, out);
            if (each_handle_write < 0 || each_handle_write != out)
            {
                LOG_PRINT_ERROR("BIO_write fail!");
                print_openssl_err();
                ret = -1;
                break;
            }
        }

        ret = 0;
    } while (0);

    if (NULL != src_bio)
    {
        BIO_free(src_bio);
    }

    if (NULL != dest_bio)
    {
        BIO_free(dest_bio);
    }

    if (NULL != ctx)
    {
        EVP_CIPHER_CTX_free(ctx);
    }

    if (NULL != cipher)
    {
        EVP_CIPHER_free(cipher);
    }

    if (NULL != each_handle_read_block)
    {
        free(each_handle_read_block);
    }

    if (NULL != each_handle_write_block)
    {
        free(each_handle_write_block);
    }

    return ret;
}

int openssl_provider_cipher_decrypt_file(const char *cipher_name, int padding, uint8_t *iv, size_t iv_len, uint8_t *key, size_t key_len, size_t each_handle_block_size, char *src_file_path, char *dest_file_path)
{
    int ret = 0;
    BIO *src_bio = NULL;
    BIO *dest_bio = NULL;
    EVP_CIPHER_CTX *ctx = NULL;
    EVP_CIPHER *cipher = NULL;

    size_t default_each_handle_read_block_size = DEFAULT_EACH_HANDLE_READ_BLOCK_SIZE;
    size_t each_handle_write_block_size = 0;
    uint8_t *each_handle_read_block = NULL;
    int each_handle_read = 0;
    uint8_t *each_handle_write_block = NULL;
    int each_handle_write = 0;
    int out = 0;

    if (NULL == cipher_name || NULL == key || NULL == src_file_path || NULL == dest_file_path)
    {
        LOG_PRINT_ERROR("invalid param!");
        return -1;
    }

    if (iv_len == 0)
    {
        iv = NULL; // ecb no need iv;
    }

    do
    {
        default_each_handle_read_block_size = (each_handle_block_size > default_each_handle_read_block_size) ? each_handle_block_size : default_each_handle_read_block_size;
        each_handle_read_block = calloc(1, default_each_handle_read_block_size);
        if (NULL == each_handle_read_block)
        {
            LOG_PRINT_ERROR("calloc fail, err[%d](%s)", errno, strerror(errno));
            break;
        }

        cipher = EVP_CIPHER_fetch(NULL, cipher_name, NULL);
        if (NULL == cipher)
        {
            LOG_PRINT_ERROR("EVP_CIPHER_fetch fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (0 == padding)
        {
            struct stat st;
            if (0 != stat(src_file_path, &st))
            {
                LOG_PRINT_ERROR("stat fail, err[%d](%s)", errno, strerror(errno));
                ret = -1;
                break;
            }

            if (0 != (size_t)st.st_size % (size_t)EVP_CIPHER_get_block_size(cipher))
            {
                LOG_PRINT_ERROR("no padding, file_size[%zu] must be integer multiple of block_size[%d]!", (size_t)st.st_size, EVP_CIPHER_get_block_size(cipher));
                ret = -1;
                break;
            }
        }

        each_handle_write_block_size = default_each_handle_read_block_size + (size_t)EVP_CIPHER_get_block_size(cipher);
        each_handle_write_block = calloc(1, each_handle_write_block_size);
        if (NULL == each_handle_write_block)
        {
            LOG_PRINT_ERROR("calloc fail, err[%d](%s)", errno, strerror(errno));
            ret = -1;
            break;
        }

        size_t expected_key_len = (size_t)EVP_CIPHER_get_key_length(cipher);
        if (key_len != expected_key_len)
        {
            LOG_PRINT_ERROR("key_len[%zu] != expected_key_len[%zu]!", key_len, expected_key_len);
            ret = -1;
            break;
        }

        size_t expected_iv_len = (size_t)EVP_CIPHER_get_iv_length(cipher);
        if (iv_len != expected_iv_len)
        {
            LOG_PRINT_ERROR("iv_len[%zu] != expected_iv_len[%zu]!", iv_len, expected_iv_len);
            ret = -1;
            break;
        }

        ctx = EVP_CIPHER_CTX_new();
        if (NULL == ctx)
        {
            LOG_PRINT_ERROR("EVP_CIPHER_CTX_new fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (0 == EVP_DecryptInit_ex2(ctx, cipher, key, iv, NULL))
        {
            LOG_PRINT_ERROR("EVP_DecryptInit_ex2 fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (0 == EVP_CIPHER_CTX_set_padding(ctx, padding))
        {
            LOG_PRINT_ERROR("EVP_CIPHER_CTX_set_padding fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        src_bio = BIO_new_file(src_file_path, "rb");
        if (NULL == src_bio)
        {
            LOG_PRINT_ERROR("BIO_new_file fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        dest_bio = BIO_new_file(dest_file_path, "wb");
        if (NULL == dest_bio)
        {
            LOG_PRINT_ERROR("BIO_new_file fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        while ((each_handle_read = BIO_read(src_bio, each_handle_read_block, (int)default_each_handle_read_block_size)) > 0)
        {
            // handle complete block by no padding
            if (0 == EVP_DecryptUpdate(ctx, each_handle_write_block, &out, each_handle_read_block, (int)each_handle_read))
            {
                LOG_PRINT_ERROR("EVP_DecryptUpdate fail!");
                print_openssl_err();
                ret = -1;
                break;
            }

            each_handle_write = BIO_write(dest_bio, each_handle_write_block, out);
            if (each_handle_write < 0 || each_handle_write != out)
            {
                LOG_PRINT_ERROR("BIO_write fail!");
                print_openssl_err();
                ret = -1;
                break;
            }
        }

        if (each_handle_read < 0)
        {
            if (BIO_should_retry(src_bio))
            {
                LOG_PRINT_ERROR("BIO_read should retry (unexpected in blocking mode)");
            }
            else
            {
                LOG_PRINT_ERROR("BIO_read failed with error");
                print_openssl_err();
            }
            ret = -1;
            break;
        }

        // handle last not complete block by set padding
        if (0 == EVP_DecryptFinal_ex(ctx, each_handle_write_block, &out))
        {
            LOG_PRINT_ERROR("EVP_DecryptFinal_ex fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (out > 0)
        {
            each_handle_write = BIO_write(dest_bio, each_handle_write_block, out);
            if (each_handle_write < 0 || each_handle_write != out)
            {
                LOG_PRINT_ERROR("BIO_write fail!");
                print_openssl_err();
                ret = -1;
                break;
            }
        }

        ret = 0;
    } while (0);

    if (NULL != src_bio)
    {
        BIO_free(src_bio);
    }

    if (NULL != dest_bio)
    {
        BIO_free(dest_bio);
    }

    if (NULL != ctx)
    {
        EVP_CIPHER_CTX_free(ctx);
    }

    if (NULL != cipher)
    {
        EVP_CIPHER_free(cipher);
    }

    if (NULL != each_handle_read_block)
    {
        free(each_handle_read_block);
    }

    if (NULL != each_handle_write_block)
    {
        free(each_handle_write_block);
    }

    return ret;
}

int openssl_provider_cipher_aead_encrypt_file(const char *cipher_name, const uint8_t *nonce, size_t nonce_len, uint8_t *aad, size_t aad_len, uint8_t *key, size_t key_len, size_t each_handle_block_size, char *src_file_path, char *dest_file_path, uint8_t *tag, size_t tag_len)
{
    int ret = 0;
    BIO *src_bio = NULL;
    BIO *dest_bio = NULL;
    EVP_CIPHER_CTX *ctx = NULL;
    EVP_CIPHER *cipher = NULL;

    size_t default_each_handle_read_block_size = DEFAULT_EACH_HANDLE_READ_BLOCK_SIZE;
    size_t each_handle_write_block_size = 0;
    uint8_t *each_handle_read_block = NULL;
    int each_handle_read = 0;
    uint8_t *each_handle_write_block = NULL;
    int each_handle_write = 0;
    int out = 0;

    if (NULL == cipher_name || NULL == nonce || NULL == key || NULL == src_file_path || NULL == dest_file_path || NULL == tag)
    {
        LOG_PRINT_ERROR("invalid param!");
        return -1;
    }

    if (aad_len > 0 && NULL == aad)
    {
        LOG_PRINT_ERROR("aad_len > 0 but aad is NULL!");
        return -1;
    }

    do
    {
        default_each_handle_read_block_size = (each_handle_block_size > default_each_handle_read_block_size) ? each_handle_block_size : default_each_handle_read_block_size;
        each_handle_read_block = calloc(1, default_each_handle_read_block_size);
        if (NULL == each_handle_read_block)
        {
            LOG_PRINT_ERROR("calloc fail, err[%d](%s)", errno, strerror(errno));
            break;
        }

        cipher = EVP_CIPHER_fetch(NULL, cipher_name, NULL);
        if (NULL == cipher)
        {
            LOG_PRINT_ERROR("EVP_CIPHER_fetch fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        each_handle_write_block_size = default_each_handle_read_block_size + (size_t)EVP_CIPHER_get_block_size(cipher);
        each_handle_write_block = calloc(1, each_handle_write_block_size);
        if (NULL == each_handle_write_block)
        {
            LOG_PRINT_ERROR("calloc fail, err[%d](%s)", errno, strerror(errno));
            break;
        }

        size_t expected_key_len = (size_t)EVP_CIPHER_get_key_length(cipher);
        if (key_len != expected_key_len)
        {
            LOG_PRINT_ERROR("key_len[%zu] != expected_key_len[%zu]!", key_len, expected_key_len);
            ret = -1;
            break;
        }

        size_t expected_nonce_len = (size_t)EVP_CIPHER_get_iv_length(cipher);
        if (nonce_len != expected_nonce_len)
        {
            LOG_PRINT_ERROR("nonce_len[%zu] != expected_nonce_len[%zu]!", nonce_len, expected_nonce_len);
            ret = -1;
            break;
        }

        ctx = EVP_CIPHER_CTX_new();
        if (NULL == ctx)
        {
            LOG_PRINT_ERROR("EVP_CIPHER_CTX_new fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (0 == EVP_EncryptInit_ex2(ctx, cipher, key, nonce, NULL))
        {
            LOG_PRINT_ERROR("EVP_EncryptInit_ex2 fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        // Process AAD
        if (aad_len > 0)
        {
            int outlen = 0;
            if (0 == EVP_EncryptUpdate(ctx, NULL, &outlen, aad, (int)aad_len))
            {
                LOG_PRINT_ERROR("EVP_EncryptUpdate (AAD) fail");
                print_openssl_err();
                ret = -1;
                break;
            }
        }

        src_bio = BIO_new_file(src_file_path, "rb");
        if (NULL == src_bio)
        {
            LOG_PRINT_ERROR("BIO_new_file fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        dest_bio = BIO_new_file(dest_file_path, "wb");
        if (NULL == dest_bio)
        {
            LOG_PRINT_ERROR("BIO_new_file fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        while ((each_handle_read = BIO_read(src_bio, each_handle_read_block, (int)default_each_handle_read_block_size)) > 0)
        {
            // handle complete block
            if (0 == EVP_EncryptUpdate(ctx, each_handle_write_block, &out, each_handle_read_block, each_handle_read))
            {
                LOG_PRINT_ERROR("EVP_EncryptUpdate fail!");
                print_openssl_err();
                ret = -1;
                break;
            }

            each_handle_write = BIO_write(dest_bio, each_handle_write_block, out);
            if (each_handle_write < 0 || each_handle_write != out)
            {
                LOG_PRINT_ERROR("BIO_write fail!");
                print_openssl_err();
                ret = -1;
                break;
            }
        }

        if (each_handle_read < 0)
        {
            if (BIO_should_retry(src_bio))
            {
                LOG_PRINT_ERROR("BIO_read should retry (unexpected in blocking mode)");
            }
            else
            {
                LOG_PRINT_ERROR("BIO_read failed with error");
                print_openssl_err();
            }
            ret = -1;
            break;
        }

        // handle end
        if (0 == EVP_EncryptFinal_ex(ctx, each_handle_write_block, &out))
        {
            LOG_PRINT_ERROR("EVP_EncryptFinal_ex fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (out > 0)
        {
            each_handle_write = BIO_write(dest_bio, each_handle_write_block, out);
            if (each_handle_write < 0 || each_handle_write != out)
            {
                LOG_PRINT_ERROR("BIO_write fail!");
                print_openssl_err();
                ret = -1;
                break;
            }
        }

        // get tag
        if (0 == EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, (int)tag_len, (void *)tag))
        {
            LOG_PRINT_ERROR("EVP_CIPHER_CTX_ctrl fail");
            print_openssl_err();
            break;
        }

        ret = 0;
    } while (0);

    if (NULL != src_bio)
    {
        BIO_free(src_bio);
    }

    if (NULL != dest_bio)
    {
        BIO_free(dest_bio);
    }

    if (NULL != ctx)
    {
        EVP_CIPHER_CTX_free(ctx);
    }

    if (NULL != cipher)
    {
        EVP_CIPHER_free(cipher);
    }

    if (NULL != each_handle_read_block)
    {
        free(each_handle_read_block);
    }

    if (NULL != each_handle_write_block)
    {
        free(each_handle_write_block);
    }

    return ret;
}

int openssl_provider_cipher_aead_decrypt_file(const char *cipher_name, const uint8_t *nonce, size_t nonce_len, uint8_t *aad, size_t aad_len, uint8_t *key, size_t key_len, size_t each_handle_block_size, char *src_file_path, char *dest_file_path, uint8_t *tag, size_t tag_len)
{
    int ret = 0;
    BIO *src_bio = NULL;
    BIO *dest_bio = NULL;
    EVP_CIPHER_CTX *ctx = NULL;
    EVP_CIPHER *cipher = NULL;

    size_t default_each_handle_read_block_size = DEFAULT_EACH_HANDLE_READ_BLOCK_SIZE;
    size_t each_handle_write_block_size = 0;
    uint8_t *each_handle_read_block = NULL;
    int each_handle_read = 0;
    uint8_t *each_handle_write_block = NULL;
    int each_handle_write = 0;
    int out = 0;

    if (NULL == cipher_name || NULL == nonce || NULL == key || NULL == src_file_path || NULL == dest_file_path || NULL == tag)
    {
        LOG_PRINT_ERROR("invalid param!");
        return -1;
    }

    if (aad_len > 0 && NULL == aad)
    {
        LOG_PRINT_ERROR("aad_len > 0 but aad is NULL!");
        return -1;
    }

    do
    {
        default_each_handle_read_block_size = (each_handle_block_size > default_each_handle_read_block_size) ? each_handle_block_size : default_each_handle_read_block_size;
        each_handle_read_block = calloc(1, default_each_handle_read_block_size);
        if (NULL == each_handle_read_block)
        {
            LOG_PRINT_ERROR("calloc fail, err[%d](%s)", errno, strerror(errno));
            break;
        }

        cipher = EVP_CIPHER_fetch(NULL, cipher_name, NULL);
        if (NULL == cipher)
        {
            LOG_PRINT_ERROR("EVP_CIPHER_fetch fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        each_handle_write_block_size = default_each_handle_read_block_size + (size_t)EVP_CIPHER_get_block_size(cipher);
        each_handle_write_block = calloc(1, each_handle_write_block_size);
        if (NULL == each_handle_write_block)
        {
            LOG_PRINT_ERROR("calloc fail, err[%d](%s)", errno, strerror(errno));
            ret = -1;
            break;
        }

        size_t expected_key_len = (size_t)EVP_CIPHER_get_key_length(cipher);
        if (key_len != expected_key_len)
        {
            LOG_PRINT_ERROR("key_len[%zu] != expected_key_len[%zu]!", key_len, expected_key_len);
            ret = -1;
            break;
        }

        size_t expected_nonce_len = (size_t)EVP_CIPHER_get_iv_length(cipher);
        if (nonce_len != expected_nonce_len)
        {
            LOG_PRINT_ERROR("nonce_len[%zu] != expected_nonce_len[%zu]!", nonce_len, expected_nonce_len);
            ret = -1;
            break;
        }

        ctx = EVP_CIPHER_CTX_new();
        if (NULL == ctx)
        {
            LOG_PRINT_ERROR("EVP_CIPHER_CTX_new fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (0 == EVP_DecryptInit_ex2(ctx, cipher, key, nonce, NULL))
        {
            LOG_PRINT_ERROR("EVP_DecryptInit_ex2 fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (0 == EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, (int)tag_len, (void *)tag))
        {
            LOG_PRINT_ERROR("EVP_CIPHER_CTX_ctrl fail");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (aad_len > 0)
        {
            int outlen = 0;
            if (0 == EVP_DecryptUpdate(ctx, NULL, &outlen, aad, (int)aad_len))
            {
                LOG_PRINT_ERROR("EVP_DecryptUpdate (AAD) fail");
                print_openssl_err();
                ret = -1;
                break;
            }
        }

        src_bio = BIO_new_file(src_file_path, "rb");
        if (NULL == src_bio)
        {
            LOG_PRINT_ERROR("BIO_new_file fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        dest_bio = BIO_new_file(dest_file_path, "wb");
        if (NULL == dest_bio)
        {
            LOG_PRINT_ERROR("BIO_new_file fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        while ((each_handle_read = BIO_read(src_bio, each_handle_read_block, (int)default_each_handle_read_block_size)) > 0)
        {
            // handle complete block
            if (0 == EVP_DecryptUpdate(ctx, each_handle_write_block, &out, each_handle_read_block, (int)each_handle_read))
            {
                LOG_PRINT_ERROR("EVP_DecryptUpdate fail!");
                print_openssl_err();
                ret = -1;
                break;
            }

            each_handle_write = BIO_write(dest_bio, each_handle_write_block, out);
            if (each_handle_write < 0 || each_handle_write != out)
            {
                LOG_PRINT_ERROR("BIO_write fail!");
                print_openssl_err();
                ret = -1;
                break;
            }
        }

        if (each_handle_read < 0)
        {
            if (BIO_should_retry(src_bio))
            {
                LOG_PRINT_ERROR("BIO_read should retry (unexpected in blocking mode)");
            }
            else
            {
                LOG_PRINT_ERROR("BIO_read failed with error");
                print_openssl_err();
            }
            ret = -1;
            break;
        }

        // verify tag
        if (0 == EVP_DecryptFinal_ex(ctx, each_handle_write_block, &out))
        {
            LOG_PRINT_ERROR("EVP_DecryptFinal_ex fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (out > 0)
        {
            each_handle_write = BIO_write(dest_bio, each_handle_write_block, out);
            if (each_handle_write < 0 || each_handle_write != out)
            {
                LOG_PRINT_ERROR("BIO_write fail!");
                print_openssl_err();
                ret = -1;
                break;
            }
        }

        ret = 0;
    } while (0);

    if (NULL != src_bio)
    {
        BIO_free(src_bio);
    }

    if (NULL != dest_bio)
    {
        BIO_free(dest_bio);
    }

    if (NULL != ctx)
    {
        EVP_CIPHER_CTX_free(ctx);
    }

    if (NULL != cipher)
    {
        EVP_CIPHER_free(cipher);
    }

    if (NULL != each_handle_read_block)
    {
        free(each_handle_read_block);
    }

    if (NULL != each_handle_write_block)
    {
        free(each_handle_write_block);
    }

    return ret;
}