#include "openssl/evp.h"
#include "openssl/core_names.h"
#include "openssl/bio.h"
#include "openssl/pem.h"
#include "openssl/bn.h"
#include "openssl/param_build.h"
#include "openssl/rsa.h"
#include "openssl/encoder.h"
#include "openssl_print.h"
#include "openssl/decoder.h"
#include "openssl_pkey.h"

static void print_bignum(const char *label, const BIGNUM *bn)
{
    if (bn == NULL)
    {
        LOG_PRINT_DEBUG("%s = (null)", label);
        return;
    }
    char *hex = BN_bn2hex(bn);
    if (hex)
    {
        LOG_PRINT_DEBUG("%s = %s", label, hex);
        OPENSSL_free(hex);
    }
    else
    {
        LOG_PRINT_DEBUG("%s = (conversion failed)", label);
    }
}

int openssl_provider_RSA_generate_key(unsigned long e, int rsa_keybits, int private_key_format, char *private_key, size_t *private_key_len, int public_key_format, char *public_key, size_t *public_key_len)
{
    EVP_PKEY_CTX *ctx = NULL;
    EVP_PKEY *pkey = NULL;
    BIO *bio_priv = NULL;
    BIO *bio_pub = NULL;
    BUF_MEM *buf_priv = NULL;
    BUF_MEM *buf_pub = NULL;
    OSSL_PARAM_BLD *param_bld = NULL;
    OSSL_PARAM *params = NULL;
    BIGNUM *e_bn = NULL;
    BIGNUM *n_bn = NULL;
    BIGNUM *d_bn = NULL;
    OSSL_ENCODER_CTX *ectx_priv = NULL;
    OSSL_ENCODER_CTX *ectx_pub = NULL;
    int ret = 0;

    if (NULL == private_key || NULL == private_key_len || NULL == public_key || NULL == public_key_len)
    {
        LOG_PRINT_ERROR("invalid param!");
        return -1;
    }

    do
    {
        param_bld = OSSL_PARAM_BLD_new();
        if (NULL == param_bld)
        {
            LOG_PRINT_ERROR("OSSL_PARAM_BLD_news fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        // bits
        if (0 == OSSL_PARAM_BLD_push_int(param_bld, OSSL_PKEY_PARAM_RSA_BITS, rsa_keybits))
        {
            LOG_PRINT_ERROR("OSSL_PARAM_BLD_push_int fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        // e
        e_bn = BN_new();
        if (NULL == e_bn)
        {
            LOG_PRINT_ERROR("BN_new fail!");
            print_openssl_err();
            ret = -1;
            break;
        }
        BN_set_word(e_bn, e);
        if (0 == OSSL_PARAM_BLD_push_BN(param_bld, OSSL_PKEY_PARAM_RSA_E, e_bn))
        {
            LOG_PRINT_ERROR("OSSL_PARAM_BLD_push_BN fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        params = OSSL_PARAM_BLD_to_param(param_bld);
        if (NULL == params)
        {
            LOG_PRINT_ERROR("OSSL_PARAM_BLD_to_param fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        ctx = EVP_PKEY_CTX_new_from_name(NULL, SN_rsa, NULL);
        if (NULL == ctx)
        {
            LOG_PRINT_ERROR("EVP_PKEY_CTX_new_from_name fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (0 == EVP_PKEY_keygen_init(ctx))
        {
            LOG_PRINT_ERROR("EVP_PKEY_keygen_init fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (0 == EVP_PKEY_CTX_set_params(ctx, params))
        {
            LOG_PRINT_ERROR("EVP_PKEY_CTX_set_params fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (EVP_PKEY_generate(ctx, &pkey) <= 0)
        {
            LOG_PRINT_ERROR("EVP_PKEY_generate fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        // 提取 e
        if (0 == EVP_PKEY_get_bn_param(pkey, OSSL_PKEY_PARAM_RSA_E, &e_bn))
        {
            LOG_PRINT_ERROR("EVP_PKEY_get_bn_param fail!");
            print_openssl_err();
            ret = -1;
            break;
        }
        print_bignum(OSSL_PKEY_PARAM_RSA_E, e_bn);

        // 提取 n
        if (0 == EVP_PKEY_get_bn_param(pkey, OSSL_PKEY_PARAM_RSA_N, &n_bn))
        {
            LOG_PRINT_ERROR("EVP_PKEY_get_bn_param fail!");
            print_openssl_err();
            ret = -1;
            break;
        }
        print_bignum(OSSL_PKEY_PARAM_RSA_N, n_bn);

        // 提取 d
        // 某些安全策略不支持导出
        if (!EVP_PKEY_get_bn_param(pkey, OSSL_PKEY_PARAM_RSA_D, &d_bn))
        {
            LOG_PRINT_ERROR("EVP_PKEY_get_bn_param fail!");
            print_openssl_err();
            ret = -1;
            break;
        }
        print_bignum(OSSL_PKEY_PARAM_RSA_D, d_bn);

        bio_priv = BIO_new(BIO_s_mem());
        if (NULL == bio_priv)
        {
            LOG_PRINT_ERROR("BIO_new fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        bio_pub = BIO_new(BIO_s_mem());
        if (NULL == bio_pub)
        {
            LOG_PRINT_ERROR("BIO_new fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        // private key
        if (E_RSA_PRIVATEKEY_FORMAT_PKCS8_PEM == private_key_format)
        {
            // PKCS#8 PEM
#if 0
        if (0 == PEM_write_bio_PrivateKey(bio_priv, pkey, NULL, NULL, 0, NULL, NULL))
        {
            LOG_PRINT_ERROR("PEM_write_bio_PrivateKey fail!");
            print_openssl_err();
            ret = -1;
            break;
        }
#else
            ectx_priv = OSSL_ENCODER_CTX_new_for_pkey(pkey, EVP_PKEY_PRIVATE_KEY, "PEM", "PrivateKeyInfo", NULL);
            if (NULL == ectx_priv)
            {
                LOG_PRINT_ERROR("OSSL_ENCODER_CTX_new_for_pkey fail!");
                print_openssl_err();
                ret = -1;
                break;
            }

            if (0 == OSSL_ENCODER_to_bio(ectx_priv, bio_priv))
            {
                LOG_PRINT_ERROR("OSSL_ENCODER_to_bio fail!");
                print_openssl_err();
                ret = -1;
                break;
            }
#endif
        }
        else if (E_RSA_PRIVATEKEY_FORMAT_PKCS8_DER == private_key_format)
        {
            // PKCS#8 DER
#if 0
        if (0 == i2d_PrivateKey_bio(bio_priv, pkey))
        {
            LOG_PRINT_ERROR("i2d_PrivateKey_bio fail!");
            print_openssl_err();
            ret = -1;
            break;
        }
#else
            ectx_priv = OSSL_ENCODER_CTX_new_for_pkey(pkey, EVP_PKEY_PRIVATE_KEY, "DER", "PrivateKeyInfo", NULL);
            if (NULL == ectx_priv)
            {
                LOG_PRINT_ERROR("OSSL_ENCODER_CTX_new_for_pkey fail!");
                print_openssl_err();
                ret = -1;
                break;
            }

            if (0 == OSSL_ENCODER_to_bio(ectx_priv, bio_priv))
            {
                LOG_PRINT_ERROR("OSSL_ENCODER_to_bio fail!");
                print_openssl_err();
                ret = -1;
                break;
            }
#endif
        }
        else if (E_RSA_PRIVATEKEY_FORMAT_PKCS1_PEM == private_key_format)
        {
            // PKCS#1 PEM
            ectx_priv = OSSL_ENCODER_CTX_new_for_pkey(pkey, EVP_PKEY_PRIVATE_KEY, "PEM", "RSAPrivateKey", NULL);
            if (NULL == ectx_priv)
            {
                LOG_PRINT_ERROR("OSSL_ENCODER_CTX_new_for_pkey fail!");
                print_openssl_err();
                ret = -1;
                break;
            }

            if (0 == OSSL_ENCODER_to_bio(ectx_priv, bio_priv))
            {
                LOG_PRINT_ERROR("OSSL_ENCODER_to_bio fail!");
                print_openssl_err();
                ret = -1;
                break;
            }
        }
        else if (E_RSA_PRIVATEKEY_FORMAT_PKCS1_DER == private_key_format)
        {
            // PKCS#1 DER
            ectx_priv = OSSL_ENCODER_CTX_new_for_pkey(pkey, EVP_PKEY_PRIVATE_KEY, "DER", "RSAPrivateKey", NULL);
            if (NULL == ectx_priv)
            {
                LOG_PRINT_ERROR("OSSL_ENCODER_CTX_new_for_pkey fail!");
                print_openssl_err();
                ret = -1;
                break;
            }

            if (0 == OSSL_ENCODER_to_bio(ectx_priv, bio_priv))
            {
                LOG_PRINT_ERROR("OSSL_ENCODER_to_bio fail!");
                print_openssl_err();
                ret = -1;
                break;
            }
        }
        else
        {
            LOG_PRINT_ERROR("invalid private_key_format[%d]!", private_key_format);
            ret = -1;
            break;
        }

        // public key
        if (E_RSA_PUBLICKEY_FORMAT_X509_PEM == public_key_format)
        {
            // X.509 PEM
            ectx_pub = OSSL_ENCODER_CTX_new_for_pkey(pkey, EVP_PKEY_PUBLIC_KEY, "PEM", "SubjectPublicKeyInfo", NULL);
            if (NULL == ectx_pub)
            {
                LOG_PRINT_ERROR("OSSL_ENCODER_CTX_new_for_pkey fail!");
                print_openssl_err();
                ret = -1;
                break;
            }

            if (0 == OSSL_ENCODER_to_bio(ectx_pub, bio_pub))
            {
                LOG_PRINT_ERROR("OSSL_ENCODER_to_bio fail!");
                print_openssl_err();
                ret = -1;
                break;
            }
        }
        else if (E_RSA_PUBLICKEY_FORMAT_X509_DER == public_key_format)
        {
            // X.509 DER
            ectx_pub = OSSL_ENCODER_CTX_new_for_pkey(pkey, EVP_PKEY_PUBLIC_KEY, "DER", "SubjectPublicKeyInfo", NULL);
            if (NULL == ectx_pub)
            {
                LOG_PRINT_ERROR("OSSL_ENCODER_CTX_new_for_pkey fail!");
                print_openssl_err();
                ret = -1;
                break;
            }

            if (0 == OSSL_ENCODER_to_bio(ectx_pub, bio_pub))
            {
                LOG_PRINT_ERROR("OSSL_ENCODER_to_bio fail!");
                print_openssl_err();
                ret = -1;
                break;
            }
        }
        else if (E_RSA_PUBLICKEY_FORMAT_PKCS1_PEM == public_key_format)
        {
            // PKCS#1 PEM
            ectx_pub = OSSL_ENCODER_CTX_new_for_pkey(pkey, EVP_PKEY_PUBLIC_KEY, "PEM", "RSAPublicKey", NULL);
            if (NULL == ectx_pub)
            {
                LOG_PRINT_ERROR("OSSL_ENCODER_CTX_new_for_pkey fail!");
                print_openssl_err();
                ret = -1;
                break;
            }

            if (0 == OSSL_ENCODER_to_bio(ectx_pub, bio_pub))
            {
                LOG_PRINT_ERROR("OSSL_ENCODER_to_bio fail!");
                print_openssl_err();
                ret = -1;
                break;
            }
        }
        else if (E_RSA_PUBLICKEY_FORMAT_PKCS1_DER == public_key_format)
        {
            // PKCS#1 DER
            ectx_pub = OSSL_ENCODER_CTX_new_for_pkey(pkey, EVP_PKEY_PUBLIC_KEY, "DER", "RSAPublicKey", NULL);
            if (NULL == ectx_pub)
            {
                LOG_PRINT_ERROR("OSSL_ENCODER_CTX_new_for_pkey fail!");
                print_openssl_err();
                ret = -1;
                break;
            }

            if (0 == OSSL_ENCODER_to_bio(ectx_pub, bio_pub))
            {
                LOG_PRINT_ERROR("OSSL_ENCODER_to_bio fail!");
                print_openssl_err();
                ret = -1;
                break;
            }
        }
        else
        {
            LOG_PRINT_ERROR("invalid public_key_format[%d]!", public_key_format);
            ret = -1;
            break;
        }

        BIO_get_mem_ptr(bio_priv, &buf_priv);
        if (NULL == buf_priv)
        {
            LOG_PRINT_ERROR("BIO_get_mem_ptr fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (*private_key_len < buf_priv->length)
        {
            LOG_PRINT_ERROR("private_key_len[%zu]  < buf_priv->length[%zu] fail!", *private_key_len, buf_priv->length);
            ret = -1;
            break;
        }
        memcpy(private_key, buf_priv->data, buf_priv->length);
        *private_key_len = buf_priv->length;

        BIO_get_mem_ptr(bio_pub, &buf_pub);
        if (NULL == buf_pub)
        {
            LOG_PRINT_ERROR("BIO_get_mem_ptr fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (*public_key_len < buf_pub->length)
        {
            LOG_PRINT_ERROR("public_key_len[%zu]  < buf_pub->length[%zu] fail!", *public_key_len, buf_pub->length);
            ret = -1;
            break;
        }
        memcpy(public_key, buf_pub->data, buf_pub->length);
        *public_key_len = buf_pub->length;

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

    if (NULL != bio_priv)
    {
        BIO_free(bio_priv);
    }

    if (NULL != bio_pub)
    {
        BIO_free(bio_pub);
    }

    if (NULL != param_bld)
    {
        OSSL_PARAM_BLD_free(param_bld);
    }

    if (NULL != params)
    {
        OSSL_PARAM_free(params);
    }

    if (NULL != e_bn)
    {
        BN_free(e_bn);
    }

    if (NULL != n_bn)
    {
        BN_free(n_bn);
    }

    if (NULL != d_bn)
    {
        BN_free(d_bn);
    }

    if (NULL != ectx_priv)
    {
        OSSL_ENCODER_CTX_free(ectx_priv);
    }

    if (NULL != ectx_pub)
    {
        OSSL_ENCODER_CTX_free(ectx_pub);
    }

    return ret;
}

int openssl_provider_ECC_generate_key(const char *curve_name, int private_key_format, char *private_key, size_t *private_key_len, int public_key_format, char *public_key, size_t *public_key_len)
{
    EVP_PKEY_CTX *ctx = NULL;
    EVP_PKEY *pkey = NULL;
    BIO *bio_priv = NULL;
    BIO *bio_pub = NULL;
    OSSL_PARAM_BLD *param_bld = NULL;
    OSSL_PARAM *params = NULL;
    BUF_MEM *buf_priv = NULL;
    BUF_MEM *buf_pub = NULL;
    OSSL_ENCODER_CTX *ectx_priv = NULL;
    OSSL_ENCODER_CTX *ectx_pub = NULL;
    int ret = 0;

    if (NULL == curve_name || NULL == private_key || NULL == private_key_len || NULL == public_key || NULL == public_key_len)
    {
        LOG_PRINT_ERROR("invalid param!");
        return -1;
    }

    do
    {
        ctx = EVP_PKEY_CTX_new_from_name(NULL, "EC", NULL);
        if (NULL == ctx)
        {
            LOG_PRINT_ERROR("EVP_PKEY_CTX_new_from_name fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        param_bld = OSSL_PARAM_BLD_new();
        if (NULL == param_bld)
        {
            LOG_PRINT_ERROR("OSSL_PARAM_BLD_new fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (0 == OSSL_PARAM_BLD_push_utf8_string(param_bld, OSSL_PKEY_PARAM_GROUP_NAME, curve_name, strlen(curve_name)))
        {
            LOG_PRINT_ERROR("OSSL_PARAM_BLD_push_utf8_string fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        params = OSSL_PARAM_BLD_to_param(param_bld);
        if (NULL == params)
        {
            LOG_PRINT_ERROR("OSSL_PARAM_BLD_to_param fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (0 == EVP_PKEY_keygen_init(ctx))
        {
            LOG_PRINT_ERROR("EVP_PKEY_keygen_init fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (0 == EVP_PKEY_CTX_set_params(ctx, params))
        {
            LOG_PRINT_ERROR("EVP_PKEY_CTX_set_params fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (EVP_PKEY_generate(ctx, &pkey) <= 0)
        {
            LOG_PRINT_ERROR("EVP_PKEY_generate fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        bio_priv = BIO_new(BIO_s_mem());
        if (NULL == bio_priv)
        {
            LOG_PRINT_ERROR("BIO_new fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        bio_pub = BIO_new(BIO_s_mem());
        if (NULL == bio_pub)
        {
            LOG_PRINT_ERROR("BIO_new fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        // private key
        if (private_key_format == E_ECC_PRIVATEKEY_FORMAT_PKCS8_PEM)
        {
            ectx_priv = OSSL_ENCODER_CTX_new_for_pkey(pkey, EVP_PKEY_PRIVATE_KEY, "PEM", "PrivateKeyInfo", NULL);
            if (NULL == ectx_priv)
            {
                LOG_PRINT_ERROR("OSSL_ENCODER_CTX_new_for_pkey fail!");
                print_openssl_err();
                ret = -1;
                break;
            }

            if (0 == OSSL_ENCODER_to_bio(ectx_priv, bio_priv))
            {
                LOG_PRINT_ERROR("OSSL_ENCODER_to_bio fail!");
                print_openssl_err();
                ret = -1;
                break;
            }
        }
        else if (private_key_format == E_ECC_PRIVATEKEY_FORMAT_PKCS8_DER)
        {
            ectx_priv = OSSL_ENCODER_CTX_new_for_pkey(pkey, EVP_PKEY_PRIVATE_KEY, "DER", "PrivateKeyInfo", NULL);
            if (NULL == ectx_priv)
            {
                LOG_PRINT_ERROR("OSSL_ENCODER_CTX_new_for_pkey fail!");
                print_openssl_err();
                ret = -1;
                break;
            }

            if (0 == OSSL_ENCODER_to_bio(ectx_priv, bio_priv))
            {
                LOG_PRINT_ERROR("OSSL_ENCODER_to_bio fail!");
                print_openssl_err();
                ret = -1;
                break;
            }
        }
        else if (private_key_format == E_ECC_PRIVATEKEY_FORMAT_SEC1_PEM)
        {
            ectx_priv = OSSL_ENCODER_CTX_new_for_pkey(pkey, EVP_PKEY_PRIVATE_KEY, "PEM", "type-specific", NULL);
            if (NULL == ectx_priv)
            {
                LOG_PRINT_ERROR("OSSL_ENCODER_CTX_new_for_pkey fail!");
                print_openssl_err();
                ret = -1;
                break;
            }

            if (0 == OSSL_ENCODER_to_bio(ectx_priv, bio_priv))
            {
                LOG_PRINT_ERROR("OSSL_ENCODER_to_bio fail!");
                print_openssl_err();
                ret = -1;
                break;
            }
        }
        else if (private_key_format == E_ECC_PRIVATEKEY_FORMAT_SEC1_DER)
        {
            ectx_priv = OSSL_ENCODER_CTX_new_for_pkey(pkey, EVP_PKEY_PRIVATE_KEY, "DER", "type-specific", NULL);
            if (NULL == ectx_priv)
            {
                LOG_PRINT_ERROR("OSSL_ENCODER_CTX_new_for_pkey fail!");
                print_openssl_err();
                ret = -1;
                break;
            }

            if (0 == OSSL_ENCODER_to_bio(ectx_priv, bio_priv))
            {
                LOG_PRINT_ERROR("OSSL_ENCODER_to_bio fail!");
                print_openssl_err();
                ret = -1;
                break;
            }
        }
        else
        {
            LOG_PRINT_ERROR("invalid private_key_format[%d]!", private_key_format);
            ret = -1;
            break;
        }

        // public key
        if (public_key_format == E_ECC_PUBLICKEY_FORMAT_X509_PEM)
        {
            ectx_pub = OSSL_ENCODER_CTX_new_for_pkey(pkey, EVP_PKEY_PUBLIC_KEY, "PEM", "SubjectPublicKeyInfo", NULL);
            if (NULL == ectx_pub)
            {
                LOG_PRINT_ERROR("OSSL_ENCODER_CTX_new_for_pkey fail!");
                print_openssl_err();
                ret = -1;
                break;
            }

            if (0 == OSSL_ENCODER_to_bio(ectx_pub, bio_pub))
            {
                LOG_PRINT_ERROR("OSSL_ENCODER_to_bio fail!");
                print_openssl_err();
                ret = -1;
                break;
            }
        }
        else if (public_key_format == E_ECC_PUBLICKEY_FORMAT_X509_DER)
        {
            ectx_pub = OSSL_ENCODER_CTX_new_for_pkey(pkey, EVP_PKEY_PUBLIC_KEY, "DER", "SubjectPublicKeyInfo", NULL);
            if (NULL == ectx_pub)
            {
                LOG_PRINT_ERROR("OSSL_ENCODER_CTX_new_for_pkey fail!");
                print_openssl_err();
                ret = -1;
                break;
            }

            if (0 == OSSL_ENCODER_to_bio(ectx_pub, bio_pub))
            {
                LOG_PRINT_ERROR("OSSL_ENCODER_to_bio fail!");
                print_openssl_err();
                ret = -1;
                break;
            }
        }
        else
        {
            LOG_PRINT_ERROR("invalid public_key_format[%d]!", public_key_format);
            ret = -1;
            break;
        }

        BIO_get_mem_ptr(bio_priv, &buf_priv);
        if (NULL == buf_priv)
        {
            LOG_PRINT_ERROR("BIO_get_mem_ptr fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (*private_key_len < buf_priv->length)
        {
            LOG_PRINT_ERROR("private_key_len[%zu]  < buf_priv->length[%zu] fail!", *private_key_len, buf_priv->length);
            ret = -1;
            break;
        }
        memcpy(private_key, buf_priv->data, buf_priv->length);
        *private_key_len = buf_priv->length;

        BIO_get_mem_ptr(bio_pub, &buf_pub);
        if (NULL == buf_pub)
        {
            LOG_PRINT_ERROR("BIO_get_mem_ptr fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (*public_key_len < buf_pub->length)
        {
            LOG_PRINT_ERROR("public_key_len[%zu]  < buf_pub->length[%zu] fail!", *public_key_len, buf_pub->length);
            ret = -1;
            break;
        }
        memcpy(public_key, buf_pub->data, buf_pub->length);
        *public_key_len = buf_pub->length;
    } while (0);

    if (NULL != ctx)
    {
        EVP_PKEY_CTX_free(ctx);
    }

    if (NULL != pkey)
    {
        EVP_PKEY_free(pkey);
    }

    if (NULL != bio_priv)
    {
        BIO_free(bio_priv);
    }

    if (NULL != bio_pub)
    {
        BIO_free(bio_pub);
    }

    if (NULL != param_bld)
    {
        OSSL_PARAM_BLD_free(param_bld);
    }

    if (NULL != params)
    {
        OSSL_PARAM_free(params);
    }

    if (NULL != ectx_priv)
    {
        OSSL_ENCODER_CTX_free(ectx_priv);
    }

    if (NULL != ectx_pub)
    {
        OSSL_ENCODER_CTX_free(ectx_pub);
    }

    return ret;
}

static const char *get_private_key_input_structure(int format)
{
    switch (format)
    {
        case E_ECC_PRIVATEKEY_FORMAT_PKCS8_PEM:
        case E_ECC_PRIVATEKEY_FORMAT_PKCS8_DER:
            return "PrivateKeyInfo"; // PKCS#8
        case E_ECC_PRIVATEKEY_FORMAT_SEC1_PEM:
        case E_ECC_PRIVATEKEY_FORMAT_SEC1_DER:
            return "type-specific"; // SEC1 for EC
        default:
            return NULL;
    }
}

static const char *get_private_key_output_structure(int format)
{
    return get_private_key_input_structure(format);
}

static const char *format_to_encoding(int format)
{
    if (format == E_ECC_PRIVATEKEY_FORMAT_PKCS8_PEM ||
        format == E_ECC_PRIVATEKEY_FORMAT_SEC1_PEM ||
        format == E_ECC_PUBLICKEY_FORMAT_X509_PEM)
    {
        return "PEM";
    }
    else if (format == E_ECC_PRIVATEKEY_FORMAT_PKCS8_DER ||
             format == E_ECC_PRIVATEKEY_FORMAT_SEC1_DER ||
             format == E_ECC_PUBLICKEY_FORMAT_X509_DER)
    {
        return "DER";
    }
    else
    {
        return NULL;
    }
}

static const char *get_public_key_structure(int format)
{
    switch (format)
    {
        case E_ECC_PUBLICKEY_FORMAT_X509_PEM:
        case E_ECC_PUBLICKEY_FORMAT_X509_DER:
            return "SubjectPublicKeyInfo";
        default:
            return NULL;
    }
}

int openssl_provider_ECC_convert_private_key(
    int key_format,
    char *key,
    size_t key_len,
    int new_key_format,
    char *new_key,
    size_t *new_key_len)
{
    if (NULL == key || 0 == key_len || NULL == new_key || NULL == new_key_len)
    {
        LOG_PRINT_ERROR("invalid params!");
        return -1;
    }

    OSSL_DECODER_CTX *dctx = NULL;
    OSSL_ENCODER_CTX *ectx = NULL;
    EVP_PKEY *pkey = NULL;
    BIO *bio_in = NULL, *bio_out = NULL;
    BUF_MEM *buf_out = NULL;
    int ret = 0;
    const char *input_struct = get_private_key_input_structure(key_format);
    const char *output_struct = get_private_key_output_structure(new_key_format);
    const char *input_encoding = format_to_encoding(key_format);
    const char *output_encoding = format_to_encoding(new_key_format);

    if (NULL == input_struct || NULL == output_struct || NULL == input_encoding || NULL == output_encoding)
    {
        LOG_PRINT_ERROR("invalid format!");
        return -1;
    }

    do
    {
        bio_in = BIO_new_mem_buf(key, (int)key_len);
        if (NULL == bio_in)
        {
            LOG_PRINT_ERROR("BIO_new_mem_buf fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        dctx = OSSL_DECODER_CTX_new_for_pkey(&pkey, input_encoding, input_struct, "EC", EVP_PKEY_PRIVATE_KEY, NULL, NULL);
        if (NULL == dctx)
        {
            LOG_PRINT_ERROR("OSSL_DECODER_CTX_new_for_pkey fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (1 != OSSL_DECODER_from_bio(dctx, bio_in))
        {
            LOG_PRINT_ERROR("OSSL_DECODER_from_bio fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        bio_out = BIO_new(BIO_s_mem());
        if (NULL == bio_out)
        {
            LOG_PRINT_ERROR("BIO_new fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        ectx = OSSL_ENCODER_CTX_new_for_pkey(pkey, EVP_PKEY_PRIVATE_KEY, output_encoding, output_struct, NULL);
        if (!ectx)
        {
            LOG_PRINT_ERROR("OSSL_ENCODER_CTX_new_for_pkey fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (1 != OSSL_ENCODER_to_bio(ectx, bio_out))
        {
            LOG_PRINT_ERROR("OSSL_ENCODER_to_bio fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (1 != BIO_get_mem_ptr(bio_out, &buf_out) || NULL == buf_out)
        {
            LOG_PRINT_ERROR("BIO_get_mem_ptr fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (*new_key_len < buf_out->length)
        {
            LOG_PRINT_ERROR("new_key_len[%zu] < buf_out->length[%zu]!", *new_key_len, buf_out->length);
            ret = -1;
            break;
        }

        memcpy(new_key, buf_out->data, buf_out->length);
        *new_key_len = buf_out->length;
        ret = 0;
    } while (0);

    if (NULL != bio_in)
    {
        BIO_free(bio_in);
    }

    if (NULL != bio_out)
    {
        BIO_free(bio_out);
    }

    if (NULL != pkey)
    {
        EVP_PKEY_free(pkey);
    }

    if (NULL != dctx)
    {
        OSSL_DECODER_CTX_free(dctx);
    }

    if (NULL != ectx)
    {
        OSSL_ENCODER_CTX_free(ectx);
    }

    return ret;
}

int openssl_provider_ECC_convert_public_key(int key_format, char *key, size_t key_len, int new_key_format, char *new_key, size_t *new_key_len)
{
    if (NULL == key || 0 == key_len || NULL == new_key || NULL == new_key_len)
    {
        LOG_PRINT_ERROR("invalid params!");
        return -1;
    }

    OSSL_DECODER_CTX *dctx = NULL;
    OSSL_ENCODER_CTX *ectx = NULL;
    EVP_PKEY *pkey = NULL;
    BIO *bio_in = NULL, *bio_out = NULL;
    BUF_MEM *buf_out = NULL;
    int ret = 0;
    const char *input_struct = get_public_key_structure(key_format);
    const char *output_struct = get_public_key_structure(new_key_format);
    const char *input_encoding = format_to_encoding(key_format);
    const char *output_encoding = format_to_encoding(new_key_format);

    if (NULL == input_struct || NULL == output_struct || NULL == input_encoding || NULL == output_encoding)
    {
        LOG_PRINT_ERROR("invalid format!");
        return -1;
    }

    do
    {
        bio_in = BIO_new_mem_buf(key, (int)key_len);
        if (NULL == bio_in)
        {
            LOG_PRINT_ERROR("BIO_new_mem_buf fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        dctx = OSSL_DECODER_CTX_new_for_pkey(&pkey, input_encoding, input_struct, "EC", EVP_PKEY_PUBLIC_KEY, NULL, NULL);
        if (NULL == dctx)
        {
            LOG_PRINT_ERROR("OSSL_DECODER_CTX_new_for_pkey fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (1 != OSSL_DECODER_from_bio(dctx, bio_in))
        {
            LOG_PRINT_ERROR("OSSL_DECODER_from_bio fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        bio_out = BIO_new(BIO_s_mem());
        if (NULL == bio_out)
        {
            LOG_PRINT_ERROR("BIO_new fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        ectx = OSSL_ENCODER_CTX_new_for_pkey(pkey, EVP_PKEY_PUBLIC_KEY, output_encoding, output_struct, NULL);
        if (!ectx)
        {
            LOG_PRINT_ERROR("OSSL_ENCODER_CTX_new_for_pkey fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (1 != OSSL_ENCODER_to_bio(ectx, bio_out))
        {
            LOG_PRINT_ERROR("OSSL_ENCODER_to_bio fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (1 != BIO_get_mem_ptr(bio_out, &buf_out) || NULL == buf_out)
        {
            LOG_PRINT_ERROR("BIO_get_mem_ptr fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (*new_key_len < buf_out->length)
        {
            LOG_PRINT_ERROR("new_key_len[%zu] < buf_out->length[%zu]!", *new_key_len, buf_out->length);
            ret = -1;
            break;
        }

        memcpy(new_key, buf_out->data, buf_out->length);
        *new_key_len = buf_out->length;
        ret = 0;
    } while (0);

    if (NULL != bio_in)
    {
        BIO_free(bio_in);
    }

    if (NULL != bio_out)
    {
        BIO_free(bio_out);
    }

    if (NULL != pkey)
    {
        EVP_PKEY_free(pkey);
    }

    if (NULL != dctx)
    {
        OSSL_DECODER_CTX_free(dctx);
    }

    if (NULL != ectx)
    {
        OSSL_ENCODER_CTX_free(ectx);
    }

    return ret;
}