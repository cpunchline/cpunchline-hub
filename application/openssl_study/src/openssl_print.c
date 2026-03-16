#include "openssl_study.h"
#include "openssl_print.h"

#if PRINT_SUPPORTED_ALGORITHM_FLAG
static int print_provider(OSSL_PROVIDER *provider, void *arg)
{
    (void)arg;
    const char *name = OSSL_PROVIDER_get0_name(provider);
    LOG_PRINT_DEBUG("PROVIDER: %s", name ? name : "unknown");
    return 0;
}

static void print_digest(EVP_MD *md, void *arg)
{
    (void)arg;
    const char *name = EVP_MD_get0_name(md);

    if (NULL == name)
    {
        LOG_PRINT_DEBUG("digest algorithm but no name");
        return;
    }

    const char *desc = EVP_MD_get0_description(md);
    const int type = EVP_MD_get_type(md);
    const int digest_size = EVP_MD_get_size(md);
    const int block_size = EVP_MD_get_block_size(md);
    const OSSL_PROVIDER *prov = EVP_MD_get0_provider(md);
    const char *prov_name = prov ? OSSL_PROVIDER_get0_name(prov) : "legacy";

    // 哈希/摘要算法名称 | 描述 | 算法NID | 摘要长度 | 分组/块大小 | provider
    LOG_PRINT_DEBUG("Digest: %-30s | %s | type %d | Size: %2d bytes | Block: %2d bytes | %s",
                    name,
                    desc ? desc : "unknown",
                    type,
                    digest_size,
                    block_size,
                    prov_name);
    /*
        // 列举指定算法支持设置的参数
        const OSSL_PARAM *set_params = EVP_MD_CTX_settable_params(EVP_MD_CTX *ctx);
        for (const OSSL_PARAM *p = set_params; p->key != NULL; p++)
        {
            LOG_PRINT_DEBUG("Supported set param: %s\n", p->key);
        }

        // 列举指定算法支持获取的参数
        const OSSL_PARAM *get_params = EVP_MD_CTX_gettable_params(EVP_MD_CTX *ctx);
        for (const OSSL_PARAM *p = get_params; p->key != NULL; p++)
        {
            LOG_PRINT_DEBUG("Supported get param: %s\n", p->key);
        }
    */
}

static void print_mac(EVP_MAC *mac, void *arg)
{
    (void)arg;

    const char *name = EVP_MAC_get0_name(mac);
    if (NULL == name)
    {
        LOG_PRINT_DEBUG("mac algorithm but no name");
        return;
    }

    const char *desc = EVP_MAC_get0_description(mac);
    const OSSL_PROVIDER *prov = EVP_MAC_get0_provider(mac);
    const char *prov_name = prov ? OSSL_PROVIDER_get0_name(prov) : "legacy";

    // after EVP_MAC_init select digest
    // EVP_MAC_CTX_get_mac_size(EVP_MAC_CTX *);
    // EVP_MAC_CTX_get_block_size(EVP_MAC_CTX *);

    // 消息认证码算法名称 | 描述 | provider
    LOG_PRINT_DEBUG("MAC: %-30s  | %s | %s",
                    name ? name : "unknown",
                    desc ? desc : "unknown",
                    prov_name);
}

static void print_cipher(EVP_CIPHER *cipher, void *arg)
{
    (void)arg;
    const char *name = EVP_CIPHER_get0_name(cipher);
    if (NULL == name)
    {
        LOG_PRINT_DEBUG("cipher algorithm but no name");
        return;
    }

    const char *desc = EVP_CIPHER_get0_description(cipher);
    const int type = EVP_CIPHER_get_type(cipher);
    const int mode = EVP_CIPHER_get_mode(cipher);
    const int key_bits = EVP_CIPHER_get_key_length(cipher) * 8;
    const int iv_len = EVP_CIPHER_get_iv_length(cipher);
    const int block_sz = EVP_CIPHER_get_block_size(cipher);
    const OSSL_PROVIDER *prov = EVP_CIPHER_get0_provider(cipher);
    const char *prov_name = prov ? OSSL_PROVIDER_get0_name(prov) : "legacy";

    // 对称加密算法名称 | 描述 | 算法NID | 工作模式 | 密钥长度 | IV向量长度 | 分组/块大小 | provider
    LOG_PRINT_DEBUG("Cipher: %-30s | %s | type %d | Mode %d | Key: %3d bits | IV: %2d bytes | Block: %2d bytes | %s", name, desc, type, mode, key_bits, iv_len, block_sz, prov_name);
}

static void print_asym_cipher(EVP_ASYM_CIPHER *cipher, void *arg)
{
    (void)arg;

    const char *name = EVP_ASYM_CIPHER_get0_name(cipher);
    const char *desc = EVP_ASYM_CIPHER_get0_description(cipher);
    const OSSL_PROVIDER *prov = EVP_ASYM_CIPHER_get0_provider(cipher);
    const char *prov_name = prov ? OSSL_PROVIDER_get0_name(prov) : "legacy";

    LOG_PRINT_DEBUG("ASYM_CIPHER: %-30s | %s | %s",
                    name ? name : "unknown",
                    desc ? desc : "unknown",
                    prov_name);
}

static void print_rand(EVP_RAND *rand, void *arg)
{
    (void)arg;
    const char *name = EVP_RAND_get0_name(rand);
    const char *desc = EVP_RAND_get0_description(rand);
    const OSSL_PROVIDER *prov = EVP_RAND_get0_provider(rand);
    const char *prov_name = prov ? OSSL_PROVIDER_get0_name(prov) : "legacy";

    LOG_PRINT_DEBUG("RAND: %-30s | %s | %s",
                    name ? name : "unknown",
                    desc ? desc : "unknown",
                    prov_name);
}

static void print_kdf(EVP_KDF *kdf, void *arg)
{
    (void)arg;
    const char *name = EVP_KDF_get0_name(kdf);
    const char *desc = EVP_KDF_get0_description(kdf);
    const OSSL_PROVIDER *prov = EVP_KDF_get0_provider(kdf);
    const char *prov_name = prov ? OSSL_PROVIDER_get0_name(prov) : "legacy";

    LOG_PRINT_DEBUG("KDF: %-30s | %s | %s",
                    name ? name : "unknown",
                    desc ? desc : "unknown",
                    prov_name);
}

static void print_keymgmt(EVP_KEYMGMT *keymgmt, void *arg)
{
    (void)arg;

    const char *name = EVP_KEYMGMT_get0_name(keymgmt);
    const char *desc = EVP_KEYMGMT_get0_description(keymgmt);
    const OSSL_PROVIDER *prov = EVP_KEYMGMT_get0_provider(keymgmt);
    const char *prov_name = prov ? OSSL_PROVIDER_get0_name(prov) : "legacy";

    LOG_PRINT_DEBUG("KEYMGMT: %-30s | %s | %s",
                    name ? name : "unknown",
                    desc ? desc : "unknown",
                    prov_name);
}

static void print_signature(EVP_SIGNATURE *sig, void *arg)
{
    (void)arg;

    const char *name = EVP_SIGNATURE_get0_name(sig);
    const char *desc = EVP_SIGNATURE_get0_description(sig);
    const OSSL_PROVIDER *prov = EVP_SIGNATURE_get0_provider(sig);
    const char *prov_name = prov ? OSSL_PROVIDER_get0_name(prov) : "legacy";

    LOG_PRINT_DEBUG("SIGNATURE: %-30s | %s | %s",
                    name ? name : "unknown",
                    desc ? desc : "unknown",
                    prov_name);
}

static void print_kem(EVP_KEM *kem, void *arg)
{
    (void)arg;

    const char *name = EVP_KEM_get0_name(kem);
    const char *desc = EVP_KEM_get0_description(kem);
    const OSSL_PROVIDER *prov = EVP_KEM_get0_provider(kem);
    const char *prov_name = prov ? OSSL_PROVIDER_get0_name(prov) : "legacy";

    LOG_PRINT_DEBUG("KEM: %-30s | %s | %s",
                    name ? name : "unknown",
                    desc ? desc : "unknown",
                    prov_name);
}

static void print_keyexch(EVP_KEYEXCH *keyexch, void *arg)
{
    (void)arg;

    const char *name = EVP_KEYEXCH_get0_name(keyexch);
    const char *desc = EVP_KEYEXCH_get0_description(keyexch);
    const OSSL_PROVIDER *prov = EVP_KEYEXCH_get0_provider(keyexch);
    const char *prov_name = prov ? OSSL_PROVIDER_get0_name(prov) : "legacy";

    LOG_PRINT_DEBUG("KEYEXCH: %-30s | %s | %s",
                    name ? name : "unknown",
                    desc ? desc : "unknown",
                    prov_name);
}

static void print_encoder(OSSL_ENCODER *encoder, void *arg)
{
    (void)arg;
    const char *name = OSSL_ENCODER_get0_name(encoder);
    const char *desc = OSSL_ENCODER_get0_description(encoder);
    const char *prop = OSSL_ENCODER_get0_properties(encoder); // 如 "format=der", "structure=private-key"
    const OSSL_PROVIDER *prov = OSSL_ENCODER_get0_provider(encoder);
    const char *prov_name = prov ? OSSL_PROVIDER_get0_name(prov) : "legacy";
    LOG_PRINT_DEBUG("ENCODER: %-30s | %s | %s | %s",
                    name ? name : "unknown",
                    desc ? desc : "unknown",
                    prop ? prop : "unknown",
                    prov_name);
}

static void print_decoder(OSSL_DECODER *decoder, void *arg)
{
    (void)arg;
    const char *name = OSSL_DECODER_get0_name(decoder);
    const char *desc = OSSL_DECODER_get0_description(decoder);
    const char *prop = OSSL_DECODER_get0_properties(decoder);
    const OSSL_PROVIDER *prov = OSSL_DECODER_get0_provider(decoder);
    const char *prov_name = prov ? OSSL_PROVIDER_get0_name(prov) : "legacy";
    LOG_PRINT_DEBUG("DECODER: %-30s | %s | %s | %s",
                    name ? name : "unknown",
                    desc ? desc : "unknown",
                    prop ? prop : "unknown",
                    prov_name);
}

static void print_store_loader(OSSL_STORE_LOADER *loader, void *arg)
{
    (void)arg;
    const char *desc = OSSL_STORE_LOADER_get0_description(loader);
    const char *prop = OSSL_STORE_LOADER_get0_properties(loader);
    const OSSL_PROVIDER *prov = OSSL_STORE_LOADER_get0_provider(loader);
    const char *prov_name = prov ? OSSL_PROVIDER_get0_name(prov) : "legacy";
    LOG_PRINT_DEBUG("STORE LOADER: %-30s | %s | %s",
                    desc ? desc : "unknown",
                    prop ? prop : "unknown",
                    prov_name);
}
#endif

void print_openssl_info(void)
{
#if 0
    // export OPENSSL_CONF=~/cpunchline-hub/tools/pre_built/ssl/openssl.cnfs
    setenv("OPENSSL_CONF", OPENSSLDIR "/openssl.cnf", 1);
    // default OPENSSL_INIT_SETTINGS
    char *config_file = CONF_get1_default_config_file();
    if (NULL == config_file)
    {
        LOG_PRINT_WARN("not find config_file!");
    }
    else
    {
        LOG_PRINT_INFO("%s", config_file);
        OPENSSL_free(config_file);
    }

    // if load config, setenv and use OPENSSL_INIT_LOAD_CONFIG
    OPENSSL_init_crypto(OPENSSL_INIT_LOAD_CONFIG, NULL);
#else
    OPENSSL_init_crypto(OPENSSL_INIT_NO_LOAD_CONFIG, NULL);
#endif
    OPENSSL_init_ssl(OPENSSL_INIT_SSL_DEFAULT, NULL);

    LOG_PRINT_INFO("[%s]-[%lu]", OpenSSL_version(OPENSSL_VERSION), OpenSSL_version_num());
    LOG_PRINT_INFO("%s", OpenSSL_version(OPENSSL_BUILT_ON));
    LOG_PRINT_INFO("%s", OpenSSL_version(OPENSSL_PLATFORM));
    LOG_PRINT_INFO("%s", OpenSSL_version(OPENSSL_CFLAGS));

#if PRINT_SUPPORTED_ALGORITHM_FLAG
    // default OSSL_LIB_CTX(use NULL)
    // OSSL_LIB_CTX *default_golbal_default = OSSL_LIB_CTX_get0_global_default();
    OSSL_PROVIDER_do_all(NULL, print_provider, NULL);               // openssl list -providers
    EVP_MD_do_all_provided(NULL, print_digest, NULL);               // openssl list -digest-algorithms
    EVP_MAC_do_all_provided(NULL, print_mac, NULL);                 // openssl list -mac-algorithms
    EVP_CIPHER_do_all_provided(NULL, print_cipher, NULL);           // openssl list -cipher-algorithms
    EVP_ASYM_CIPHER_do_all_provided(NULL, print_asym_cipher, NULL); // openssl list -asymcipher-algorithms
    EVP_RAND_do_all_provided(NULL, print_rand, NULL);               // openssl list -rand-algorithms
    EVP_KDF_do_all_provided(NULL, print_kdf, NULL);                 // openssl list -kdf-algorithms

    // openssl list -public-key-algorithms
    EVP_KEYMGMT_do_all_provided(NULL, print_keymgmt, NULL);     // openssl list -key-managers
    EVP_SIGNATURE_do_all_provided(NULL, print_signature, NULL); // openssl list -signature-algorithms
    EVP_KEM_do_all_provided(NULL, print_kem, NULL);             // openssl list -kem-algorithms
    EVP_KEYEXCH_do_all_provided(NULL, print_keyexch, NULL);     // openssl list -key-exchange-algorithms

    OSSL_ENCODER_do_all_provided(NULL, print_encoder, NULL);           // openssl list -encoders
    OSSL_DECODER_do_all_provided(NULL, print_decoder, NULL);           // openssl list -decoders
    OSSL_STORE_LOADER_do_all_provided(NULL, print_store_loader, NULL); // openssl list -store-loaders
#endif
}
