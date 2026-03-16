#include "openssl_print.h"
#include "openssl_pkey.h"
#include "openssl/pem.h"
#include "openssl/rsa.h"
#include "openssl/decoder.h"
#include "openssl_asym_cipher.h"

int openssl_provider_RSA_public_encrypt(int padding, char *public_key, size_t public_key_len, const void *_src, size_t src_len, void *dest, size_t *dest_len)
{
    int ret = 0;
    EVP_PKEY_CTX *ctx = NULL;
    EVP_PKEY *pkey = NULL;
    OSSL_DECODER_CTX *dctx = NULL;
    BIO *bio = NULL;
    size_t out_len = 0;
    size_t max_src_len = 0;
    size_t pkey_size = 0;

    if (NULL == public_key || NULL == _src || NULL == dest)
    {
        LOG_PRINT_ERROR("invalid param!");
        return -1;
    }

    do
    {
        bio = BIO_new_mem_buf(public_key, (int)public_key_len);
        if (NULL == bio)
        {
            LOG_PRINT_ERROR("BIO_new_mem_buf fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        dctx = OSSL_DECODER_CTX_new_for_pkey(&pkey, NULL, NULL, SN_rsa, EVP_PKEY_PUBLIC_KEY, NULL, NULL);
        if (NULL == dctx)
        {
            LOG_PRINT_ERROR("OSSL_DECODER_CTX_new_for_pkey fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (0 == OSSL_DECODER_from_bio(dctx, bio))
        {
            LOG_PRINT_ERROR("OSSL_DECODER_from_bio fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (NULL == pkey)
        {
            LOG_PRINT_ERROR("read PUBLIC KEY fail");
            print_openssl_err();
            ret = -1;
            break;
        }

        ctx = EVP_PKEY_CTX_new_from_pkey(NULL, pkey, NULL);
        if (NULL == ctx)
        {
            LOG_PRINT_ERROR("EVP_PKEY_CTX_new_from_pkey fail");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (0 == EVP_PKEY_encrypt_init(ctx))
        {
            LOG_PRINT_ERROR("EVP_PKEY_encrypt_init fail");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (EVP_PKEY_CTX_set_rsa_padding(ctx, padding) <= 0)
        {
            LOG_PRINT_ERROR("EVP_PKEY_CTX_set_rsa_padding fail");
            print_openssl_err();
            ret = -1;
            break;
        }

        pkey_size = (size_t)EVP_PKEY_get_size(pkey);
        if (RSA_PKCS1_PADDING == padding)
        {
            // padding=RSA_PKCS1_PADDING要分块加密; 最大支持EVP_PKEY_get_size(pkey) - RSA_PKCS1_PADDING_SIZE;
            max_src_len = pkey_size - RSA_PKCS1_PADDING_SIZE;
        }
        else if (RSA_PKCS1_OAEP_PADDING == padding)
        {
            // padding=RSA_PKCS1_OAEP_PADDING要分块加密; 每块max_src_len = EVP_PKEY_get_size(pkey) - 42;s
            max_src_len = pkey_size - 42;
        }
        else
        {
            max_src_len = pkey_size;
        }

        if (src_len <= max_src_len)
        {
            if (EVP_PKEY_encrypt(ctx, NULL, &out_len, (const unsigned char *)_src, src_len) <= 0)
            {
                LOG_PRINT_ERROR("EVP_PKEY_encrypt fail");
                print_openssl_err();
                ret = -1;
                break;
            }

            if (*dest_len < out_len)
            {
                LOG_PRINT_ERROR("dest_len[%zu] < out_len[%zu]", *dest_len, out_len);
                ret = -1;
                break;
            }

            if (0 == EVP_PKEY_encrypt(ctx, (unsigned char *)dest, &out_len, (const unsigned char *)_src, src_len))
            {
                LOG_PRINT_ERROR("EVP_PKEY_encrypt fail");
                print_openssl_err();
                ret = -1;
                break;
            }
        }
        else
        {
            LOG_PRINT_WARN("need chunk block to encrypt");
            size_t each_blocksize = 0;
            size_t each_encryptsize = 0;
            for (size_t i = 0; i < src_len; i += max_src_len)
            {
                each_blocksize = (src_len - i > max_src_len) ? max_src_len : (src_len - i);

                if (EVP_PKEY_encrypt(ctx, NULL, &each_encryptsize,
                                     (const unsigned char *)_src + i, each_blocksize) <= 0)
                {
                    LOG_PRINT_ERROR("EVP_PKEY_encrypt fail!");
                    print_openssl_err();
                    ret = -1;
                    break;
                }

                if (*dest_len < out_len + each_encryptsize)
                {
                    LOG_PRINT_ERROR("dest_len[%zu] < out_len[%zu]", *dest_len, out_len + each_encryptsize);
                    ret = -1;
                    break;
                }

                if (0 == EVP_PKEY_encrypt(ctx, (unsigned char *)dest + out_len, &each_encryptsize, (const unsigned char *)_src + i, each_blocksize))
                {
                    LOG_PRINT_ERROR("EVP_PKEY_encrypt fail");
                    print_openssl_err();
                    ret = -1;
                    break;
                }
                out_len += each_encryptsize;
            }
        }

        *dest_len = out_len;
        ret = 0;
    } while (0);

    if (NULL != ctx)
    {
        EVP_PKEY_CTX_free(ctx);
    }

    if (NULL != pkey)
    {
        EVP_PKEY_free(pkey);
    }

    if (NULL != dctx)
    {
        OSSL_DECODER_CTX_free(dctx);
    }

    if (NULL != bio)
    {
        BIO_free(bio);
    }

    return ret;
}

int openssl_provider_RSA_private_decrypt(int padding, char *private_key, size_t private_key_len, const void *_src, size_t src_len, void *dest, size_t *dest_len)
{
    int ret = 0;
    EVP_PKEY_CTX *ctx = NULL;
    EVP_PKEY *pkey = NULL;
    OSSL_DECODER_CTX *dctx = NULL;
    BIO *bio = NULL;
    size_t out_len = 0;
    size_t pkey_size = 0;

    if (NULL == private_key || NULL == _src || NULL == dest)
    {
        LOG_PRINT_ERROR("invalid param!");
        return -1;
    }

    do
    {
        bio = BIO_new_mem_buf(private_key, (int)private_key_len);
        if (NULL == bio)
        {
            LOG_PRINT_ERROR("BIO_new_mem_buf fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        dctx = OSSL_DECODER_CTX_new_for_pkey(&pkey, NULL, NULL, SN_rsa, EVP_PKEY_PRIVATE_KEY, NULL, NULL);
        if (NULL == dctx)
        {
            LOG_PRINT_ERROR("OSSL_DECODER_CTX_new_for_pkey fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (0 == OSSL_DECODER_from_bio(dctx, bio))
        {
            LOG_PRINT_ERROR("OSSL_DECODER_from_bio fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (NULL == pkey)
        {
            LOG_PRINT_ERROR("read PRIVATE KEY fail");
            print_openssl_err();
            ret = -1;
            break;
        }

        ctx = EVP_PKEY_CTX_new_from_pkey(NULL, pkey, NULL);
        if (NULL == ctx)
        {
            LOG_PRINT_ERROR("EVP_PKEY_CTX_new_from_pkey fail");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (0 == EVP_PKEY_decrypt_init(ctx))
        {
            LOG_PRINT_ERROR("EVP_PKEY_decrypt_init fail");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (0 == EVP_PKEY_CTX_set_rsa_padding(ctx, padding))
        {
            LOG_PRINT_ERROR("EVP_PKEY_CTX_set_rsa_padding fail");
            print_openssl_err();
            ret = -1;
            break;
        }

        pkey_size = (size_t)EVP_PKEY_get_size(pkey);
        if (src_len <= pkey_size)
        {
            if (0 == EVP_PKEY_decrypt(ctx, NULL, &out_len, (const unsigned char *)_src, src_len))
            {
                LOG_PRINT_ERROR("EVP_PKEY_decrypt fail");
                print_openssl_err();
                ret = -1;
                break;
            }

            if (*dest_len < out_len)
            {
                LOG_PRINT_ERROR("dest_len[%zu] < out_len[%zu]", *dest_len, out_len);
                ret = -1;
                break;
            }

            if (0 == EVP_PKEY_decrypt(ctx, (unsigned char *)dest, &out_len, (const unsigned char *)_src, src_len))
            {
                LOG_PRINT_ERROR("EVP_PKEY_decrypt fail");
                print_openssl_err();
                ret = -1;
                break;
            }
        }
        else
        {
            LOG_PRINT_WARN("need chunk block to decrypt");
            if (src_len % pkey_size != 0)
            {
                LOG_PRINT_ERROR("src_len[%zu] is not a multiple of pkey_size[%zu]", src_len, pkey_size);
                ret = -1;
                break;
            }
            size_t each_decryptsize = 0;
            for (size_t i = 0; i < src_len; i += pkey_size)
            {
                each_decryptsize = pkey_size;
                if (0 == EVP_PKEY_decrypt(ctx, NULL, &each_decryptsize, (const unsigned char *)_src + i, pkey_size))
                {
                    LOG_PRINT_ERROR("EVP_PKEY_decrypt fail!");
                    print_openssl_err();
                    ret = -1;
                    break;
                }

                if (*dest_len < out_len + each_decryptsize)
                {
                    LOG_PRINT_ERROR("dest_len[%zu] < out_len[%zu]", *dest_len, out_len + each_decryptsize);
                    ret = -1;
                    break;
                }

                if (0 == EVP_PKEY_decrypt(ctx, (unsigned char *)dest + out_len, &each_decryptsize, (const unsigned char *)_src + i, pkey_size))
                {
                    LOG_PRINT_ERROR("EVP_PKEY_decrypt fail");
                    print_openssl_err();
                    ret = -1;
                    break;
                }
                out_len += each_decryptsize;
            }
        }

        *dest_len = out_len;
        ret = 0;
    } while (0);

    if (NULL != ctx)
    {
        EVP_PKEY_CTX_free(ctx);
    }

    if (NULL != pkey)
    {
        EVP_PKEY_free(pkey);
    }

    if (NULL != dctx)
    {
        OSSL_DECODER_CTX_free(dctx);
    }

    if (NULL != bio)
    {
        BIO_free(bio);
    }

    return ret;
}

int openssl_provider_RSA_sign(const char *digest_name, char *private_key, size_t private_key_len, void *_src, size_t src_len, void *sign, size_t *sign_len)
{
    int ret = 0;
    EVP_MD_CTX *mctx = NULL;
    EVP_PKEY *pkey = NULL;
    OSSL_DECODER_CTX *dctx = NULL;
    BIO *bio = NULL;

    if (NULL == digest_name || NULL == private_key || NULL == _src || NULL == sign || NULL == sign_len)
    {
        LOG_PRINT_ERROR("invalid param!");
        return -1;
    }

    do
    {
        bio = BIO_new_mem_buf(private_key, (int)private_key_len);
        if (NULL == bio)
        {
            LOG_PRINT_ERROR("BIO_new_mem_buf fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        dctx = OSSL_DECODER_CTX_new_for_pkey(&pkey, NULL, NULL, SN_rsa, EVP_PKEY_PRIVATE_KEY, NULL, NULL);
        if (NULL == dctx)
        {
            LOG_PRINT_ERROR("OSSL_DECODER_CTX_new_for_pkey fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (0 == OSSL_DECODER_from_bio(dctx, bio))
        {
            LOG_PRINT_ERROR("OSSL_DECODER_from_bio fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        mctx = EVP_MD_CTX_new();
        if (NULL == mctx)
        {
            LOG_PRINT_ERROR("EVP_MD_CTX_new fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (0 == EVP_DigestSignInit_ex(mctx, NULL, digest_name, NULL, NULL, pkey, NULL))
        {
            LOG_PRINT_ERROR("EVP_DigestSignInit fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (0 == EVP_DigestSignUpdate(mctx, _src, src_len))
        {
            LOG_PRINT_ERROR("EVP_DigestSignUpdate fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        size_t required_len = 0;
        if (0 == EVP_DigestSignFinal(mctx, NULL, &required_len))
        {
            LOG_PRINT_ERROR("EVP_DigestSignFinal fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (*sign_len < required_len)
        {
            LOG_PRINT_ERROR("sign_len[%zu] < required_len[%zu]", *sign_len, required_len);
            ret = -1;
            break;
        }

        if (0 == EVP_DigestSignFinal(mctx, (unsigned char *)sign, sign_len))
        {
            LOG_PRINT_ERROR("EVP_DigestSignFinal fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        ret = 0;
    } while (0);

    if (NULL != mctx)
    {
        EVP_MD_CTX_free(mctx);
    }

    if (NULL != pkey)
    {
        EVP_PKEY_free(pkey);
    }

    if (NULL != dctx)
    {
        OSSL_DECODER_CTX_free(dctx);
    }

    if (NULL != bio)
    {
        BIO_free(bio);
    }

    return ret;
}

int openssl_provider_RSA_verify_sign(const char *digest_name, char *public_key, size_t public_key_len, void *_src, size_t src_len, void *sign, size_t sign_len)
{
    int ret = 0;
    EVP_MD_CTX *mctx = NULL;
    EVP_PKEY *pkey = NULL;
    OSSL_DECODER_CTX *dctx = NULL;
    BIO *bio = NULL;

    if (NULL == digest_name || NULL == public_key || NULL == _src || NULL == sign)
    {
        LOG_PRINT_ERROR("invalid param!");
        return -1;
    }

    do
    {
        bio = BIO_new_mem_buf(public_key, (int)public_key_len);
        if (NULL == bio)
        {
            LOG_PRINT_ERROR("BIO_new_mem_buf fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        dctx = OSSL_DECODER_CTX_new_for_pkey(&pkey, NULL, NULL, SN_rsa, EVP_PKEY_PUBLIC_KEY, NULL, NULL);
        if (NULL == dctx)
        {
            LOG_PRINT_ERROR("OSSL_DECODER_CTX_new_for_pkey fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (0 == OSSL_DECODER_from_bio(dctx, bio))
        {
            LOG_PRINT_ERROR("OSSL_DECODER_from_bio fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        mctx = EVP_MD_CTX_new();
        if (NULL == mctx)
        {
            LOG_PRINT_ERROR("EVP_MD_CTX_new fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (0 == EVP_DigestVerifyInit_ex(mctx, NULL, digest_name, NULL, NULL, pkey, NULL))
        {
            LOG_PRINT_ERROR("EVP_DigestVerifyInit_ex fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (0 == EVP_DigestVerifyUpdate(mctx, _src, src_len))
        {
            LOG_PRINT_ERROR("EVP_DigestVerifyUpdate fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (0 == EVP_DigestVerifyFinal(mctx, (unsigned char *)sign, sign_len))
        {
            LOG_PRINT_ERROR("EVP_DigestSignFinal fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        ret = 0;
    } while (0);

    if (NULL != mctx)
    {
        EVP_MD_CTX_free(mctx);
    }

    if (NULL != pkey)
    {
        EVP_PKEY_free(pkey);
    }

    if (NULL != dctx)
    {
        OSSL_DECODER_CTX_free(dctx);
    }

    if (NULL != bio)
    {
        BIO_free(bio);
    }

    return ret;
}

int openssl_provider_DSA_sign(const char *digest_name, char *private_key, size_t private_key_len, void *_src, size_t src_len, void *sign, size_t *sign_len)
{
    int ret = 0;
    EVP_MD_CTX *mctx = NULL;
    EVP_PKEY *pkey = NULL;
    OSSL_DECODER_CTX *dctx = NULL;
    BIO *bio = NULL;

    if (NULL == digest_name || NULL == private_key || NULL == _src || NULL == sign || NULL == sign_len)
    {
        LOG_PRINT_ERROR("invalid param!");
        return -1;
    }

    do
    {
        bio = BIO_new_mem_buf(private_key, (int)private_key_len);
        if (NULL == bio)
        {
            LOG_PRINT_ERROR("BIO_new_mem_buf fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        dctx = OSSL_DECODER_CTX_new_for_pkey(&pkey, NULL, NULL, SN_dsa, EVP_PKEY_PRIVATE_KEY, NULL, NULL);
        if (NULL == dctx)
        {
            LOG_PRINT_ERROR("OSSL_DECODER_CTX_new_for_pkey fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (0 == OSSL_DECODER_from_bio(dctx, bio))
        {
            LOG_PRINT_ERROR("OSSL_DECODER_from_bio fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        mctx = EVP_MD_CTX_new();
        if (NULL == mctx)
        {
            LOG_PRINT_ERROR("EVP_MD_CTX_new fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (0 == EVP_DigestSignInit_ex(mctx, NULL, digest_name, NULL, NULL, pkey, NULL))
        {
            LOG_PRINT_ERROR("EVP_DigestSignInit fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (0 == EVP_DigestSignUpdate(mctx, _src, src_len))
        {
            LOG_PRINT_ERROR("EVP_DigestSignUpdate fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        size_t required_len = 0;
        if (0 == EVP_DigestSignFinal(mctx, NULL, &required_len))
        {
            LOG_PRINT_ERROR("EVP_DigestSignFinal fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (*sign_len < required_len)
        {
            LOG_PRINT_ERROR("sign_len[%zu] < required_len[%zu]", *sign_len, required_len);
            ret = -1;
            break;
        }

        if (0 == EVP_DigestSignFinal(mctx, (unsigned char *)sign, sign_len))
        {
            LOG_PRINT_ERROR("EVP_DigestSignFinal fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        ret = 0;
    } while (0);

    if (NULL != mctx)
    {
        EVP_MD_CTX_free(mctx);
    }

    if (NULL != pkey)
    {
        EVP_PKEY_free(pkey);
    }

    if (NULL != dctx)
    {
        OSSL_DECODER_CTX_free(dctx);
    }

    if (NULL != bio)
    {
        BIO_free(bio);
    }

    return ret;
}

int openssl_provider_DSA_verify_sign(const char *digest_name, char *public_key, size_t public_key_len, void *_src, size_t src_len, void *sign, size_t sign_len)
{
    int ret = 0;
    EVP_MD_CTX *mctx = NULL;
    EVP_PKEY *pkey = NULL;
    OSSL_DECODER_CTX *dctx = NULL;
    BIO *bio = NULL;

    if (NULL == digest_name || NULL == public_key || NULL == _src || NULL == sign)
    {
        LOG_PRINT_ERROR("invalid param!");
        return -1;
    }

    do
    {
        bio = BIO_new_mem_buf(public_key, (int)public_key_len);
        if (NULL == bio)
        {
            LOG_PRINT_ERROR("BIO_new_mem_buf fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        dctx = OSSL_DECODER_CTX_new_for_pkey(&pkey, NULL, NULL, SN_dsa, EVP_PKEY_PUBLIC_KEY, NULL, NULL);
        if (NULL == dctx)
        {
            LOG_PRINT_ERROR("OSSL_DECODER_CTX_new_for_pkey fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (0 == OSSL_DECODER_from_bio(dctx, bio))
        {
            LOG_PRINT_ERROR("OSSL_DECODER_from_bio fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        mctx = EVP_MD_CTX_new();
        if (NULL == mctx)
        {
            LOG_PRINT_ERROR("EVP_MD_CTX_new fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (0 == EVP_DigestVerifyInit_ex(mctx, NULL, digest_name, NULL, NULL, pkey, NULL))
        {
            LOG_PRINT_ERROR("EVP_DigestVerifyInit_ex fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (0 == EVP_DigestVerifyUpdate(mctx, _src, src_len))
        {
            LOG_PRINT_ERROR("EVP_DigestVerifyUpdate fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (0 == EVP_DigestVerifyFinal(mctx, (unsigned char *)sign, sign_len))
        {
            LOG_PRINT_ERROR("EVP_DigestSignFinal fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        ret = 0;
    } while (0);

    if (NULL != mctx)
    {
        EVP_MD_CTX_free(mctx);
    }

    if (NULL != pkey)
    {
        EVP_PKEY_free(pkey);
    }

    if (NULL != dctx)
    {
        OSSL_DECODER_CTX_free(dctx);
    }

    if (NULL != bio)
    {
        BIO_free(bio);
    }

    return ret;
}

int openssl_provider_ECDSA_sign(const char *digest_name, char *private_key, size_t private_key_len, void *_src, size_t src_len, void *sign, size_t *sign_len)
{
    int ret = 0;
    EVP_MD_CTX *mctx = NULL;
    EVP_PKEY *pkey = NULL;
    OSSL_DECODER_CTX *dctx = NULL;
    BIO *bio = NULL;

    if (NULL == digest_name || NULL == private_key || NULL == _src || NULL == sign || NULL == sign_len)
    {
        LOG_PRINT_ERROR("invalid param!");
        return -1;
    }

    do
    {
        bio = BIO_new_mem_buf(private_key, (int)private_key_len);
        if (NULL == bio)
        {
            LOG_PRINT_ERROR("BIO_new_mem_buf fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        dctx = OSSL_DECODER_CTX_new_for_pkey(&pkey, NULL, NULL, "EC", EVP_PKEY_PRIVATE_KEY, NULL, NULL);
        if (NULL == dctx)
        {
            LOG_PRINT_ERROR("OSSL_DECODER_CTX_new_for_pkey fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (0 == OSSL_DECODER_from_bio(dctx, bio))
        {
            LOG_PRINT_ERROR("OSSL_DECODER_from_bio fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        mctx = EVP_MD_CTX_new();
        if (NULL == mctx)
        {
            LOG_PRINT_ERROR("EVP_MD_CTX_new fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (0 == EVP_DigestSignInit_ex(mctx, NULL, digest_name, NULL, NULL, pkey, NULL))
        {
            LOG_PRINT_ERROR("EVP_DigestSignInit fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (0 == EVP_DigestSignUpdate(mctx, _src, src_len))
        {
            LOG_PRINT_ERROR("EVP_DigestSignUpdate fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        size_t required_len = 0;
        if (0 == EVP_DigestSignFinal(mctx, NULL, &required_len))
        {
            LOG_PRINT_ERROR("EVP_DigestSignFinal fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (*sign_len < required_len)
        {
            LOG_PRINT_ERROR("sign_len[%zu] < required_len[%zu]", *sign_len, required_len);
            ret = -1;
            break;
        }

        if (0 == EVP_DigestSignFinal(mctx, (unsigned char *)sign, sign_len))
        {
            LOG_PRINT_ERROR("EVP_DigestSignFinal fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        ret = 0;
    } while (0);

    if (NULL != mctx)
    {
        EVP_MD_CTX_free(mctx);
    }

    if (NULL != pkey)
    {
        EVP_PKEY_free(pkey);
    }

    if (NULL != dctx)
    {
        OSSL_DECODER_CTX_free(dctx);
    }

    if (NULL != bio)
    {
        BIO_free(bio);
    }

    return ret;
}

int openssl_provider_ECDSA_verify_sign(const char *digest_name, char *public_key, size_t public_key_len, void *_src, size_t src_len, void *sign, size_t sign_len)
{
    int ret = 0;
    EVP_MD_CTX *mctx = NULL;
    EVP_PKEY *pkey = NULL;
    OSSL_DECODER_CTX *dctx = NULL;
    BIO *bio = NULL;

    if (NULL == digest_name || NULL == public_key || NULL == _src || NULL == sign)
    {
        LOG_PRINT_ERROR("invalid param!");
        return -1;
    }

    do
    {
        bio = BIO_new_mem_buf(public_key, (int)public_key_len);
        if (NULL == bio)
        {
            LOG_PRINT_ERROR("BIO_new_mem_buf fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        dctx = OSSL_DECODER_CTX_new_for_pkey(&pkey, NULL, NULL, "EC", EVP_PKEY_PUBLIC_KEY, NULL, NULL);
        if (NULL == dctx)
        {
            LOG_PRINT_ERROR("OSSL_DECODER_CTX_new_for_pkey fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (0 == OSSL_DECODER_from_bio(dctx, bio))
        {
            LOG_PRINT_ERROR("OSSL_DECODER_from_bio fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        mctx = EVP_MD_CTX_new();
        if (NULL == mctx)
        {
            LOG_PRINT_ERROR("EVP_MD_CTX_new fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (0 == EVP_DigestVerifyInit_ex(mctx, NULL, digest_name, NULL, NULL, pkey, NULL))
        {
            LOG_PRINT_ERROR("EVP_DigestVerifyInit_ex fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (0 == EVP_DigestVerifyUpdate(mctx, _src, src_len))
        {
            LOG_PRINT_ERROR("EVP_DigestVerifyUpdate fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (0 == EVP_DigestVerifyFinal(mctx, (unsigned char *)sign, sign_len))
        {
            LOG_PRINT_ERROR("EVP_DigestSignFinal fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        ret = 0;
    } while (0);

    if (NULL != mctx)
    {
        EVP_MD_CTX_free(mctx);
    }

    if (NULL != pkey)
    {
        EVP_PKEY_free(pkey);
    }

    if (NULL != dctx)
    {
        OSSL_DECODER_CTX_free(dctx);
    }

    if (NULL != bio)
    {
        BIO_free(bio);
    }

    return ret;
}

int openssl_provider_ECDH_exchange_key(char *private_key, size_t private_key_len, char *peer_public_key, size_t peer_public_key_len, void *dest, size_t *dest_len)
{
    int ret = 0;
    EVP_PKEY *local_pkey = NULL; // 本地私钥(含公钥)
    EVP_PKEY *peer_pkey = NULL;  // 对方公钥
    EVP_PKEY_CTX *ctx = NULL;
    OSSL_DECODER_CTX *priv_dctx = NULL;
    OSSL_DECODER_CTX *pub_dctx = NULL;
    BIO *priv_bio = NULL;
    BIO *pub_bio = NULL;
    size_t shared_secret_len = 0;

    if (NULL == private_key || NULL == peer_public_key || NULL == dest || NULL == dest)
    {
        LOG_PRINT_ERROR("invalid param!");
        return -1;
    }

    do
    {
        priv_bio = BIO_new_mem_buf(private_key, (int)private_key_len);
        if (NULL == priv_bio)
        {
            LOG_PRINT_ERROR("BIO_new_mem_buf fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        priv_dctx = OSSL_DECODER_CTX_new_for_pkey(&local_pkey, NULL, NULL, "EC", EVP_PKEY_PRIVATE_KEY, NULL, NULL);
        if (NULL == priv_dctx)
        {
            LOG_PRINT_ERROR("OSSL_DECODER_CTX_new_for_pkey fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (0 == OSSL_DECODER_from_bio(priv_dctx, priv_bio))
        {
            LOG_PRINT_ERROR("OSSL_DECODER_from_bio fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        pub_bio = BIO_new_mem_buf(peer_public_key, (int)peer_public_key_len);
        if (NULL == pub_bio)
        {
            LOG_PRINT_ERROR("BIO_new_mem_buf fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        pub_dctx = OSSL_DECODER_CTX_new_for_pkey(&peer_pkey, NULL, NULL, "EC", EVP_PKEY_PUBLIC_KEY, NULL, NULL);
        if (NULL == pub_dctx)
        {
            LOG_PRINT_ERROR("OSSL_DECODER_CTX_new_for_pkey fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (0 == OSSL_DECODER_from_bio(pub_dctx, pub_bio))
        {
            LOG_PRINT_ERROR("OSSL_DECODER_from_bio fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        ctx = EVP_PKEY_CTX_new(local_pkey, NULL);
        if (NULL == ctx)
        {
            LOG_PRINT_ERROR("EVP_PKEY_CTX_new fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (0 == EVP_PKEY_derive_init(ctx))
        {
            LOG_PRINT_ERROR("EVP_PKEY_derive_init fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (0 == EVP_PKEY_derive_set_peer(ctx, peer_pkey))
        {
            LOG_PRINT_ERROR("EVP_PKEY_derive_set_peer fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (0 == EVP_PKEY_derive(ctx, NULL, &shared_secret_len))
        {
            LOG_PRINT_ERROR("EVP_PKEY_derive to get length fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (shared_secret_len > *dest_len)
        {
            LOG_PRINT_ERROR("shared_secret_len[%zu] > dest_len[%zu]", shared_secret_len, *dest_len);
            ret = -1;
            break;
        }

        if (0 == EVP_PKEY_derive(ctx, (unsigned char *)dest, &shared_secret_len))
        {
            LOG_PRINT_ERROR("EVP_PKEY_derive fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        *dest_len = shared_secret_len;
        ret = 0;
    } while (0);

    if (NULL != local_pkey)
    {
        EVP_PKEY_free(local_pkey);
    }

    if (NULL != peer_pkey)
    {
        EVP_PKEY_free(peer_pkey);
    }

    if (NULL != ctx)
    {
        EVP_PKEY_CTX_free(ctx);
    }

    if (NULL != priv_dctx)
    {
        OSSL_DECODER_CTX_free(priv_dctx);
    }

    if (NULL != pub_dctx)
    {
        OSSL_DECODER_CTX_free(pub_dctx);
    }

    if (NULL != priv_bio)
    {
        BIO_free(priv_bio);
    }

    if (NULL != pub_bio)
    {
        BIO_free(pub_bio);
    }

    return ret;
}