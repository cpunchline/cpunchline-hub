#include "dsa/base64.h"
#include "dsa/md5.h"
#include "dsa/darray.h"

#include "openssl/rsa.h"
#include "openssl_print.h"
#include "openssl_base64.h"
#include "openssl_md.h"
#include "openssl_mac.h"
#include "openssl_cipher.h"
#include "openssl_pkey.h"
#include "openssl_asym_cipher.h"
#include "openssl_rand.h"
#include "openssl_ssl_tls.h"
#include "openssl_study.h"

static void test_base64(void)
{
    char arr[] = "hello world";
    size_t arr_size = strlen(arr);

    char openssl_encode_arr[BASE64_ENCODE_LEN(arr_size)] = {};
    size_t openssl_encode_len = BASE64_ENCODE_LEN(arr_size);
    char openssl_decode_arr[BASE64_DECODE_LEN(openssl_encode_len)] = {};
    size_t openssl_decode_len = BASE64_DECODE_LEN(openssl_encode_len);

    LOG_PRINT_BUF("source_data", arr, arr_size);
    if (0 != openssl_base64_encode(arr, arr_size, openssl_encode_arr, &openssl_encode_len))
    {
        LOG_PRINT_ERROR("openssl_base64_encode fail");
    }
    LOG_PRINT_BUF("base64_encode_data", openssl_encode_arr, openssl_encode_len);

    if (0 != openssl_base64_decode(openssl_encode_arr, openssl_encode_len, openssl_decode_arr, &openssl_decode_len))
    {
        LOG_PRINT_ERROR("openssl_base64_decode fail");
    }
    LOG_PRINT_BUF("base64_decode_data", openssl_decode_arr, openssl_decode_len);

    char encode_arr[BASE64_ENCODE_LEN(arr_size)] = {};
    size_t encode_len = BASE64_ENCODE_LEN(arr_size);
    char decode_arr[BASE64_DECODE_LEN(encode_len)] = {};
    size_t decode_len = BASE64_DECODE_LEN(encode_len);

    encode_len = (size_t)base64_encode(arr, arr_size, encode_arr, encode_len);
    if (encode_len == 0)
    {
        LOG_PRINT_ERROR("base64_encode fail");
        return;
    }
    LOG_PRINT_BUF("base64_encode_data", encode_arr, encode_len);

    decode_len = (size_t)base64_decode(encode_arr, decode_arr, decode_len);
    if (encode_len == 0)
    {
        LOG_PRINT_ERROR("base64_encode fail");
        return;
    }
    LOG_PRINT_BUF("base64_decode_data", decode_arr, decode_len);
}

static void test_rand(void)
{
    // openssl rand -hex 32
    // openssl rand -base64 16

    uint8_t buf[256] = {};
    memset(buf, 0x00, sizeof(buf));
    if (0 == secure_random_bytes(buf, sizeof(buf)))
    {
        LOG_PRINT_ERROR("RAND_bytes fail!");
        print_openssl_err();
        return;
    }
}

static void test_md5_digest(void)
{
    // printf "hello world" | openssl dgst -md5
    char arr[] = "hello world";
    size_t arr_size = strlen(arr);
    char openssl_md5_digest_arr[EVP_MD_get_size(EVP_md5()) + 1] = {};
    size_t openssl_md5_digest_arr_size = (size_t)EVP_MD_get_size(EVP_md5());

    if (0 != openssl_provider_digest(OSSL_DIGEST_NAME_MD5, arr, arr_size, openssl_md5_digest_arr, &openssl_md5_digest_arr_size))
    {
        LOG_PRINT_ERROR("openssl_provider_digest fail");
        return;
    }
    LOG_PRINT_BUF("md5_digest", openssl_md5_digest_arr, openssl_md5_digest_arr_size);

    char md5_digest_arr[DSA_MD5_DIGEST_LENGTH + 1] = {};
    md5_ctx_t ctx = {};

    md5_begin(&ctx);
    md5_hash(arr, arr_size, &ctx);
    md5_end(md5_digest_arr, &ctx);
    LOG_PRINT_BUF("md5_digest", md5_digest_arr, strlen(md5_digest_arr));
}

static void test_shake256_digest(void)
{
    // printf "hello world" | openssl dgst -shake256
    char arr[] = "hello world";
    size_t arr_size = strlen(arr);
    char openssl_shake256_digest_arr[EVP_MD_get_size(EVP_shake256()) + 1] = {};
    size_t openssl_shake256_digest_arr_size = (size_t)EVP_MD_get_size(EVP_shake256());

    if (0 != openssl_provider_xof_digest(LN_shake256, arr, arr_size, openssl_shake256_digest_arr, openssl_shake256_digest_arr_size))
    {
        LOG_PRINT_ERROR("openssl_provider_xof_digest fail");
        return;
    }
    LOG_PRINT_BUF("shake256_digest", openssl_shake256_digest_arr, openssl_shake256_digest_arr_size);
}

static void test_md5_digest_file(void)
{
    // openssl dgst -md5 ./application/openssl_study/src/openssl_study.c
    char test_file[] = __FILE__;
    char openssl_md5_digest_arr[EVP_MD_get_size(EVP_md5()) + 1] = {};
    size_t openssl_md5_digest_arr_size = (size_t)EVP_MD_get_size(EVP_md5());

    if (0 != openssl_provider_digest_file(OSSL_DIGEST_NAME_MD5, 256, test_file, openssl_md5_digest_arr, &openssl_md5_digest_arr_size))
    {
        LOG_PRINT_ERROR("openssl_provider_digest_file fail");
        return;
    }
    LOG_PRINT_BUF("md5_file_digest", openssl_md5_digest_arr, openssl_md5_digest_arr_size);

    char md5_digest_arr[DSA_MD5_DIGEST_LENGTH + 1] = {};

    md5sum(test_file, md5_digest_arr);
    LOG_PRINT_BUF("md5_file_digest", openssl_md5_digest_arr, DSA_MD5_DIGEST_LENGTH);
}

static void test_shake256_digest_file(void)
{
    // openssl dgst -shake256 ./application/openssl_study/src/openssl_study.c
    char test_file[] = __FILE__;
    char openssl_shake256_digest_arr[EVP_MD_get_size(EVP_shake256()) + 1] = {};
    size_t openssl_shake256_digest_arr_size = (size_t)EVP_MD_get_size(EVP_shake256());

    if (0 != openssl_provider_xof_digest_file(LN_shake256, 256, test_file, openssl_shake256_digest_arr, openssl_shake256_digest_arr_size))
    {
        LOG_PRINT_ERROR("openssl_provider_xof_digest_file fail");
        return;
    }
    LOG_PRINT_BUF("shake256_file_digest", openssl_shake256_digest_arr, openssl_shake256_digest_arr_size);
}

#if 0
static void _test_md5_digest_list_file(char *file_path, char *md5_digest_arr)
{
    FILE *fp = NULL;
    darray_uchar dest_array = darray_new();

    size_t default_each_handle_block_size = 256;
    uint8_t *each_handle_block = NULL;
    size_t each_handle_read = 0;
    uint8_t *each_handle_digest = NULL;
    md5_ctx_t ctx = {};
    md5_ctx_t each_ctx = {};

    each_handle_block = calloc(1, default_each_handle_block_size);
    if (NULL == each_handle_block)
    {
        LOG_PRINT_ERROR("calloc fail, err[%d](%s)", errno, strerror(errno));
        return;
    }

    // each_handle_digest_size must < default_each_handle_block_size
    each_handle_digest = calloc(1, default_each_handle_block_size);
    if (NULL == each_handle_digest)
    {
        LOG_PRINT_ERROR("calloc fail, err[%d](%s)", errno, strerror(errno));
        free(each_handle_block);
        return;
    }

    fp = fopen(file_path, "rb");
    if (!fp)
    {
        LOG_PRINT_ERROR("fopen fail, err[%d](%s)", errno, strerror(errno));
        free(each_handle_digest);
        free(each_handle_block);
        return;
    }

    while (1)
    {
        each_handle_read = fread(each_handle_block, 1, default_each_handle_block_size, fp);
        if (each_handle_read <= 0)
        {
            break;
        }
        md5_begin(&each_ctx);
        md5_hash(each_handle_block, each_handle_read, &each_ctx);
        memset(each_handle_digest, 0x00, default_each_handle_block_size);
        md5_end(each_handle_digest, &each_ctx);
        darray_append_items(dest_array, each_handle_digest, DSA_MD5_DIGEST_LENGTH);
    }

    int file_error = ferror(fp);
    if (0 != file_error)
    {
        LOG_PRINT_ERROR("ferror fail, ferror(%d), err[%d](%s)", file_error, errno, strerror(errno));
    }
    else
    {
        md5_begin(&ctx);
        md5_hash(dest_array.item, darray_size(dest_array), &ctx);
        md5_end(md5_digest_arr, &ctx);
    }

    fclose(fp);
    free(each_handle_digest);
    free(each_handle_block);
    darray_free(dest_array);
}

static void test_md5_digest_list_file(void)
{
    char test_file[] = __FILE__;
    char openssl_md5_digest_arr[EVP_MD_get_size(EVP_md5()) + 1] = {};
    size_t openssl_md5_digest_arr_size = (size_t)EVP_MD_get_size(EVP_md5());

    if (0 != openssl_provider_digest_list_file(OSSL_DIGEST_NAME_MD5, 256, test_file, openssl_md5_digest_arr, &openssl_md5_digest_arr_size))
    {
        LOG_PRINT_ERROR("openssl_provider_digest_list_file fail");
        return;
    }
    LOG_PRINT_BUF("md5_file_digest_list", openssl_md5_digest_arr, openssl_md5_digest_arr_size);

    char md5_digest_arr[DSA_MD5_DIGEST_LENGTH + 1] = {};
    _test_md5_digest_list_file(test_file, md5_digest_arr);
    LOG_PRINT_BUF("md5_file_digest_list", md5_digest_arr, strlen(md5_digest_arr));
}

static void test_sha256_digest_merkle_tree_file(void)
{
    char test_file[] = __FILE__;
    char openssl_sha256_digest_arr[EVP_MD_get_size(EVP_sha256()) + 1] = {};
    size_t openssl_sha256_digest_arr_size = (size_t)EVP_MD_get_size(EVP_sha256());

    if (0 != openssl_provider_digest_merkle_tree_file(OSSL_DIGEST_NAME_SHA2_256, 256, test_file, openssl_sha256_digest_arr, &openssl_sha256_digest_arr_size))
    {
        LOG_PRINT_ERROR("openssl_provider_digest_merkle_tree_file fail");
        return;
    }
    LOG_PRINT_BUF("md5_file_digest_merkle_tree_file", openssl_sha256_digest_arr, openssl_sha256_digest_arr_size);
}
#endif

static void test_hmac(void)
{
    // printf "hello world" | openssl dgst -sha256 -hmac "sharekey"
    char arr[] = "hello world";
    size_t arr_size = strlen(arr);
    char key[] = "sharekey";
    size_t key_size = strlen(key);
    char openssl_sha256_digest_arr[EVP_MD_get_size(EVP_sha256()) + 1] = {};
    size_t openssl_sha256_digest_arr_size = (size_t)EVP_MD_get_size(EVP_sha256());

    if (0 != openssl_provider_hmac(OSSL_DIGEST_NAME_SHA2_256, (uint8_t *)key, key_size, arr, arr_size, openssl_sha256_digest_arr, &openssl_sha256_digest_arr_size))
    {
        LOG_PRINT_ERROR("openssl_provider_hmac fail");
        return;
    }
    LOG_PRINT_BUF("hmac_sha2_256", openssl_sha256_digest_arr, openssl_sha256_digest_arr_size);
}

static void test_cmac(void)
{
    // printf "hello world" | openssl mac -cipher aes-256-cbc -macopt hexkey:73686172656b657973686172656b657973686172656b657973686172656b6579 CMAC
    char arr[] = "hello world";
    size_t arr_size = strlen(arr);
    size_t key_size = (size_t)EVP_CIPHER_get_key_length(EVP_aes_256_cbc());
    uint8_t key[key_size];
    memset(key, 0x00, key_size);
    memcpy(key, "sharekeysharekeysharekeysharekey", key_size);

    unsigned char cmac_out[16] = {}; // AES-CMAC 输出固定为 16 字节
    size_t cmac_out_len = sizeof(cmac_out);

    if (0 != openssl_provider_cmac(SN_aes_256_cbc, (uint8_t *)key, key_size, arr, arr_size, cmac_out, &cmac_out_len))
    {
        LOG_PRINT_ERROR("openssl_provider_cmac fail");
        return;
    }
    LOG_PRINT_BUF("cmac_aes_256_cbc", cmac_out, cmac_out_len);
}

static void test_cipher(void)
{
    // printf "hello world" | openssl enc -aes-256-cbc -K 73686172656b657973686172656b657973686172656b657973686172656b6579 -iv 88888888888888888888888888888888 -base64
    // printf "hello world" | openssl enc -aes-256-cbc -K 73686172656b657973686172656b657973686172656b657973686172656b6579 -iv 88888888888888888888888888888888 | openssl enc -aes-256-cbc -d -K 73686172656b657973686172656b657973686172656b657973686172656b6579 -iv 88888888888888888888888888888888
    char arr[] = "hello world";
    size_t arr_size = strlen(arr);

    size_t key_size = (size_t)EVP_CIPHER_get_key_length(EVP_aes_256_cbc());
    uint8_t key[key_size];
    memset(key, 0x00, key_size);
    memcpy(key, "sharekeysharekeysharekeysharekey", key_size);

    LOG_PRINT_BUF("key", key, key_size);

    size_t iv_len = (size_t)EVP_CIPHER_get_iv_length(EVP_aes_256_cbc());
    uint8_t iv[iv_len];
#if 0
    memset(iv, 0x00, iv_len);
    if (0 == secure_random_bytes(iv, iv_len))
    {
        LOG_PRINT_ERROR("RAND_bytes fail!");
        print_openssl_err();
        return;
    }
#else
    memset(iv, 0x88, iv_len);
#endif
    LOG_PRINT_BUF("iv", iv, iv_len);

    size_t cipher_encrypt_arr_size = arr_size + (size_t)EVP_CIPHER_get_block_size(EVP_aes_256_cbc()) + 1; // max encrypt size = plain_len + block_size;
    uint8_t cipher_encrypt_arr[cipher_encrypt_arr_size];
    memset(cipher_encrypt_arr, 0x00, cipher_encrypt_arr_size);

    size_t cipher_decrypt_arr_size = (size_t)cipher_encrypt_arr_size + 1; // max decrypt size = cipher_encrypt_arr_size;
    uint8_t cipher_decrypt_arr[cipher_decrypt_arr_size];
    memset(cipher_decrypt_arr, 0x00, cipher_decrypt_arr_size);

    if (0 != openssl_provider_cipher_encrypt(SN_aes_256_cbc, EVP_PADDING_PKCS7, iv, iv_len, key, key_size, arr, arr_size, cipher_encrypt_arr, &cipher_encrypt_arr_size))
    {
        LOG_PRINT_ERROR("openssl_provider_cipher_encrypt fail!");
        return;
    }

    LOG_PRINT_BUF("AES-256-CBC-endrypt", cipher_encrypt_arr, cipher_encrypt_arr_size);

    char openssl_encode_arr[BASE64_ENCODE_LEN(cipher_encrypt_arr_size)] = {};
    size_t openssl_encode_len = BASE64_ENCODE_LEN(cipher_encrypt_arr_size);

    if (0 != openssl_base64_encode(cipher_encrypt_arr, cipher_encrypt_arr_size, openssl_encode_arr, &openssl_encode_len))
    {
        LOG_PRINT_ERROR("openssl_base64_encode fail");
    }
    // rVtxmJZP3Ggbrd1v503O/A==
    LOG_PRINT_DEBUG("[%zu](%s)", openssl_encode_len, openssl_encode_arr);

    if (0 != openssl_provider_cipher_decrypt(SN_aes_256_cbc, EVP_PADDING_PKCS7, iv, iv_len, key, key_size, cipher_encrypt_arr, cipher_encrypt_arr_size, cipher_decrypt_arr, &cipher_decrypt_arr_size))
    {
        LOG_PRINT_ERROR("openssl_provider_cipher_decrypt fail!");
        return;
    }
    LOG_PRINT_BUF("AES-256-CBC-decrypt", cipher_decrypt_arr, cipher_decrypt_arr_size);
    LOG_PRINT_DEBUG("AES-256-CBC-decrypt string-[%zu][%s]", cipher_decrypt_arr_size, cipher_decrypt_arr);
}

static void test_cipher_aead(void)
{
    char arr[] = "hello world";
    size_t arr_size = strlen(arr);

    size_t key_size = (size_t)EVP_CIPHER_get_key_length(EVP_aes_256_gcm());
    uint8_t key[key_size];
    memset(key, 0x00, key_size);
    memcpy(key, "sharekeysharekeysharekeysharekey", key_size);

    LOG_PRINT_BUF("key", key, key_size);

    size_t nonce_len = (size_t)EVP_CIPHER_get_iv_length(EVP_aes_256_gcm());
    uint8_t nonce[nonce_len];
#if 0
    memset(nonce, 0x00, nonce_len);
    if (0 == secure_random_bytes(nonce, nonce_len))
    {
        LOG_PRINT_ERROR("RAND_bytes fail!");
        print_openssl_err();
        return;
    }
#else
    memset(nonce, 0x88, nonce_len);
#endif
    LOG_PRINT_BUF("nonce", nonce, nonce_len);

#if 1
    uint8_t aad[] = "header";
    size_t aad_len = sizeof(aad) - 1;
    LOG_PRINT_BUF("aad", aad, aad_len);
#else
    uint8_t *aad = NULL;
    size_t aad_len = 0;
#endif

    size_t cipher_encrypt_arr_size = arr_size + (size_t)EVP_CIPHER_get_block_size(EVP_aes_256_gcm()) + 1; // max encrypt size = plain_len + block_size;
    uint8_t cipher_encrypt_arr[cipher_encrypt_arr_size];
    memset(cipher_encrypt_arr, 0x00, cipher_encrypt_arr_size);

    size_t cipher_decrypt_arr_size = (size_t)cipher_encrypt_arr_size + 1; // max decrypt size = cipher_encrypt_arr_size;
    uint8_t cipher_decrypt_arr[cipher_decrypt_arr_size];
    memset(cipher_decrypt_arr, 0x00, cipher_decrypt_arr_size);

    size_t tag_len = 16; // secure default for GCM
    uint8_t tag[tag_len];
    memset(tag, 0x00, tag_len);

    if (0 != openssl_provider_cipher_aead_encrypt(SN_aes_256_gcm, nonce, nonce_len, aad, aad_len, key, key_size, arr, arr_size, cipher_encrypt_arr, &cipher_encrypt_arr_size, tag, tag_len))
    {
        LOG_PRINT_ERROR("openssl_provider_cipher_aead_encrypt fail!");
        return;
    }

    LOG_PRINT_BUF("AES-256-GCM-endrypt", cipher_encrypt_arr, cipher_encrypt_arr_size);
    LOG_PRINT_BUF("AES-256-GCM-endrypt-tag", tag, tag_len);

    char openssl_encode_arr[BASE64_ENCODE_LEN(cipher_encrypt_arr_size)] = {};
    size_t openssl_encode_len = BASE64_ENCODE_LEN(cipher_encrypt_arr_size);

    if (0 != openssl_base64_encode(cipher_encrypt_arr, cipher_encrypt_arr_size, openssl_encode_arr, &openssl_encode_len))
    {
        LOG_PRINT_ERROR("openssl_base64_encode fail");
    }

    // UEt0l+hOqFC3irI=
    LOG_PRINT_DEBUG("[%zu](%s)", openssl_encode_len, openssl_encode_arr);

    if (0 != openssl_provider_cipher_aead_decrypt(SN_aes_256_gcm, nonce, nonce_len, aad, aad_len, key, key_size, cipher_encrypt_arr, cipher_encrypt_arr_size, cipher_decrypt_arr, &cipher_decrypt_arr_size, tag, tag_len))
    {
        LOG_PRINT_ERROR("openssl_provider_cipher_aead_decrypt fail!");
        return;
    }
    LOG_PRINT_BUF("AES-256-GCM-decrypt", cipher_decrypt_arr, cipher_decrypt_arr_size);
    LOG_PRINT_DEBUG("AES-256-GCM-decrypt string-[%zu][%s]", cipher_decrypt_arr_size, cipher_decrypt_arr);
}

static void test_cipher_file(const char *src_file)
{
    // openssl enc -aes-256-cbc -in src_file -out encrypt_file -K "73686172656b657973686172656b657973686172656b657973686172656b6579" -iv "88888888888888888888888888888888"
    // openssl enc -aes-256-cbc -d -in encrypt_file -out decrypt_file -K "73686172656b657973686172656b657973686172656b657973686172656b6579" -iv "88888888888888888888888888888888"
    char *encrypt_file = "encrypt_file";
    char *decrypt_file = "decrypt_file";
    size_t key_size = (size_t)EVP_CIPHER_get_key_length(EVP_aes_256_cbc());
    uint8_t key[key_size];
    memset(key, 0x00, key_size);
    memcpy(key, "sharekeysharekeysharekeysharekey", key_size);

    LOG_PRINT_BUF("key", key, key_size);

    size_t iv_len = (size_t)EVP_CIPHER_get_iv_length(EVP_aes_256_cbc());
    uint8_t iv[iv_len];
    memset(iv, 0x88, iv_len);
    LOG_PRINT_BUF("iv", iv, iv_len);

    if (0 != openssl_provider_cipher_encrypt_file(SN_aes_256_cbc, EVP_PADDING_PKCS7, iv, iv_len, key, key_size, 256, (char *)src_file, encrypt_file))
    {
        LOG_PRINT_ERROR("openssl_provider_cipher_encrypt_file fail!");
        return;
    }

    if (0 != openssl_provider_cipher_decrypt_file(SN_aes_256_cbc, EVP_PADDING_PKCS7, iv, iv_len, key, key_size, 256, encrypt_file, decrypt_file))
    {
        LOG_PRINT_ERROR("openssl_provider_cipher_decrypt_file fail!");
        return;
    }
}

static void test_RSA(void)
{
    // PKCS#8私钥-X509公钥 (default pem)
    // openssl genpkey -algorithm RSA -pkeyopt rsa_keygen_bits:2048 -pkeyopt rsa_keygen_pubexp:65537
    // openssl genpkey -algorithm RSA -pkeyopt rsa_keygen_bits:2048 -pkeyopt rsa_keygen_pubexp:65537 | openssl pkey -pubout

    // PKCS#1私钥-PKCS#1公钥
    // 1. 先生成标准私钥(PKCS#8)openssl genpkey -algorithm RSA -pkeyopt rsa_keygen_bits:2048 -pkeyopt rsa_keygen_pubexp:65537
    // 2. 转换为 PKCS#1 pem私钥(传统格式) PKCS#1 公钥 openssl pkey -in priv.pem -outform PEM -traditional -out priv_pkcs1.pem
    // 2. 转换为 PKCS#1 pem私钥(传统格式) PKCS#1 公钥 openssl pkey -in priv.pem -outform DER -traditional -out priv_pkcs1.der
    // 3. 从 PKCS#1 私钥提取 PKCS#1 公钥 openssl rsa -in priv_pkcs1.pem -RSAPublicKey_out -out pub_pkcs1.pem

    // 验证方式
    // 代码私钥写入一个文件
    // openssl pkey -in priv_c.pem -pubout -out pub_from_openssl.pem
    // openssl rsa -in priv.pem -RSAPublicKey_out -out pub_pkcs1.pem
    // openssl rsa -in priv.pem -RSAPublicKey_out -outform DER -out pub_pkcs1.der
    // 然后对比生成的公钥和代码输出公钥是否一致
    char private_key[2048] = {};
    size_t private_key_len = sizeof(private_key);
    char public_key[1024] = {};
    size_t public_key_len = sizeof(public_key);
    if (0 != openssl_provider_RSA_generate_key(RSA_F4, 2048, E_RSA_PRIVATEKEY_FORMAT_PKCS8_PEM, private_key, &private_key_len, E_RSA_PUBLICKEY_FORMAT_X509_PEM, public_key, &public_key_len))
    {
        LOG_PRINT_ERROR("openssl_provider_RSA_generate_key fail!");
        return;
    }

    LOG_PRINT_DEBUG("private_key\n%s[%zu]", private_key, private_key_len);
    LOG_PRINT_DEBUG("public_key\n%s[%zu]", public_key, public_key_len);

    // 分块数 = 2048 / (256 - 11) = 9块;
    // 加密后总长度 = 256 * 9 = 2304;
    // 解密后总长度 = 原长度 = 2304;
    char arr[] = "hello world";
    size_t arr_size = strlen(arr);
    uint8_t encrypt_data[2304] = {};
    size_t encrypt_data_len = sizeof(encrypt_data);
    uint8_t decrypt_data[2304] = {};
    size_t decrypt_data_len = sizeof(decrypt_data);

    if (0 != openssl_provider_RSA_public_encrypt(RSA_PKCS1_PADDING, public_key, public_key_len, arr, arr_size, encrypt_data, &encrypt_data_len))
    {
        LOG_PRINT_ERROR("openssl_provider_RSA_public_encrypt fail!");
        return;
    }
    LOG_PRINT_BUF("rsa encrypt data", encrypt_data, encrypt_data_len);

    if (0 != openssl_provider_RSA_private_decrypt(RSA_PKCS1_PADDING, private_key, private_key_len, encrypt_data, encrypt_data_len, decrypt_data, &decrypt_data_len))
    {
        LOG_PRINT_ERROR("openssl_provider_RSA_private_decrypt fail!");
        return;
    }
    LOG_PRINT_BUF("rsa decrypt data", decrypt_data, decrypt_data_len);

    uint8_t sign_data[256] = {};
    size_t sign_data_len = sizeof(sign_data);

    // echo -n "hello world" > data.txt
    // openssl dgst -sha256 -sign private.pem -out signature.bin data.txt
    // openssl dgst -sha256 -verify public.pem -signature signature.bin data.txt
    if (0 != openssl_provider_RSA_sign(OSSL_DIGEST_NAME_SHA2_256, private_key, private_key_len, arr, arr_size, sign_data, &sign_data_len))
    {
        LOG_PRINT_ERROR("openssl_provider_RSA_sign fail!");
        return;
    }
    LOG_PRINT_BUF("rsa sign data", sign_data, sign_data_len);

    if (0 != openssl_provider_RSA_verify_sign(OSSL_DIGEST_NAME_SHA2_256, public_key, public_key_len, arr, arr_size, sign_data, sign_data_len))
    {
        LOG_PRINT_ERROR("openssl_provider_RSA_verify_sign fail!");
        return;
    }
    LOG_PRINT_DEBUG("verify sign ok!");
}

static void test_ECC(void)
{
    char arr[] = "hello world";
    size_t arr_size = strlen(arr);
    char private_key[2048] = {};
    size_t private_key_len = sizeof(private_key);
    char public_key[1024] = {};
    size_t public_key_len = sizeof(public_key);
    if (0 != openssl_provider_ECC_generate_key(SN_X9_62_prime256v1, E_ECC_PRIVATEKEY_FORMAT_PKCS8_PEM, private_key, &private_key_len, E_ECC_PUBLICKEY_FORMAT_X509_PEM, public_key, &public_key_len))
    {
        LOG_PRINT_ERROR("openssl_provider_ECC_generate_key fail!");
        return;
    }

    char private_key1[2048] = {};
    size_t private_key_len1 = sizeof(private_key1);
    char public_key1[1024] = {};
    size_t public_key_len1 = sizeof(public_key1);
    if (0 != openssl_provider_ECC_generate_key(SN_X9_62_prime256v1, E_ECC_PRIVATEKEY_FORMAT_PKCS8_PEM, private_key1, &private_key_len1, E_ECC_PUBLICKEY_FORMAT_X509_PEM, public_key1, &public_key_len1))
    {
        LOG_PRINT_ERROR("openssl_provider_ECC_generate_key fail!");
        return;
    }

    LOG_PRINT_DEBUG("private_key\n%s[%zu]", private_key, private_key_len);
    LOG_PRINT_DEBUG("public_key\n%s[%zu]", public_key, public_key_len);
    LOG_PRINT_DEBUG("private_key1\n%s[%zu]", private_key1, private_key_len1);
    LOG_PRINT_DEBUG("public_key1\n%s[%zu]", public_key1, public_key_len1);

    uint8_t sign_data[256] = {};
    size_t sign_data_len = sizeof(sign_data);

    if (0 != openssl_provider_ECDSA_sign(OSSL_DIGEST_NAME_SHA2_256, private_key, private_key_len, arr, arr_size, sign_data, &sign_data_len))
    {
        LOG_PRINT_ERROR("openssl_provider_ECDSA_sign fail!");
        return;
    }
    LOG_PRINT_BUF("ecc sign data", sign_data, sign_data_len);

    if (0 != openssl_provider_ECDSA_verify_sign(OSSL_DIGEST_NAME_SHA2_256, public_key, public_key_len, arr, arr_size, sign_data, sign_data_len))
    {
        LOG_PRINT_ERROR("openssl_provider_ECDSA_verify_sign fail!");
        return;
    }
    LOG_PRINT_DEBUG("verify sign ok!");

    uint8_t ecdh_exchange_key[256] = {};
    size_t ecdh_exchange_key_len = sizeof(ecdh_exchange_key);

    uint8_t ecdh_exchange_key1[256] = {};
    size_t ecdh_exchange_key_len1 = sizeof(ecdh_exchange_key1);

    // openssl ecparam -genkey -name prime256v1 -out alice_priv.pem
    // openssl ec -in alice_priv.pem -pubout -out alice_pub.pem

    // openssl ecparam -genkey -name prime256v1 -out bob_priv.pem
    // openssl ec -in bob_priv.pem -pubout -out bob_pub.pem

    // openssl pkeyutl -derive -inkey alice_priv.pem -peerkey bob_pub.pem -out shared_secret_alice.bin
    // openssl pkeyutl -derive -inkey bob_priv.pem -peerkey alice_pub.pem -out shared_secret_bob.bin

    if (0 != openssl_provider_ECDH_exchange_key(private_key, private_key_len, public_key1, public_key_len1, ecdh_exchange_key, &ecdh_exchange_key_len))
    {
        LOG_PRINT_ERROR("openssl_provider_ECDH_exchange_key fail!");
        return;
    }
    LOG_PRINT_BUF("ecdh exchange key", ecdh_exchange_key, ecdh_exchange_key_len);

    if (0 != openssl_provider_ECDH_exchange_key(private_key1, private_key_len1, public_key, public_key_len, ecdh_exchange_key1, &ecdh_exchange_key_len1))
    {
        LOG_PRINT_ERROR("openssl_provider_ECDH_exchange_key fail!");
        return;
    }
    LOG_PRINT_BUF("ecdh exchange key1", ecdh_exchange_key1, ecdh_exchange_key_len1);
}

static void *tcp_tls_server_thread_func(void *arg)
{
    (void)arg;
    tcp_tls_server(SSLTESTDIR "/server/server.crt", SSLTESTDIR "/server/server.key");
    return NULL;
}

static void *tcp_tls_client_thread_func(void *arg)
{
    (void)arg;
    tcp_tls_client(SSLTESTDIR "/server/server.crt");
    return NULL;
}

static void test_ssl_tls(void)
{
    pthread_t tcp_tls_server_thread = 0;
    pthread_t tcp_tls_client_thread = 0;

    pthread_create(&tcp_tls_server_thread, NULL, tcp_tls_server_thread_func, NULL);
    sleep(1);
    pthread_create(&tcp_tls_client_thread, NULL, tcp_tls_client_thread_func, NULL);

    pthread_join(tcp_tls_server_thread, NULL);
    pthread_join(tcp_tls_client_thread, NULL);
}

int main(int argc, const char *argv[])
{
    print_openssl_info();
    printf("\n\n");

    OSSL_PROVIDER *default_provider = NULL;
    OSSL_PROVIDER *legacy_provider = NULL;

    default_provider = OSSL_PROVIDER_load(NULL, "default");
    if (NULL == default_provider)
    {
        LOG_PRINT_ERROR("load default provider fail!");
        print_openssl_err();
        goto main_end;
    }

    // please not compile with no-legacy
    if (0 == OSSL_PROVIDER_set_default_search_path(NULL, MODULESDIR))
    {
        LOG_PRINT_ERROR("set provider search path fail!");
        print_openssl_err();
        exit(1);
    }

    legacy_provider = OSSL_PROVIDER_load(NULL, "legacy");
    if (NULL == legacy_provider)
    {
        LOG_PRINT_ERROR("load legacy provider fail");
        print_openssl_err();
        goto main_end;
    }

    test_base64();
    printf("\n\n");

    // RAND
    test_rand();
    printf("\n\n");

    // md
    test_md5_digest();
    test_md5_digest_file();
    test_shake256_digest();
    test_shake256_digest_file();
#if 0
    test_md5_digest_list_file();
    test_sha256_digest_merkle_tree_file();
#endif
    printf("\n\n");

    // mac
    test_hmac();
    test_cmac();
    printf("\n\n");

    // cipher
    test_cipher();
    test_cipher_aead();
    if (argc > 1)
    {
        test_cipher_file(argv[1]);
    }
    printf("\n\n");

    // RSA generate key and encrypt/decrypt and sign/verify
    test_RSA();
    printf("\n\n");

    // ECC generate key and encrypt/decrypt and sign/verify and exchange key
    test_ECC();
    printf("\n\n");

    test_ssl_tls();

main_end:
    if (NULL != legacy_provider)
    {
        OSSL_PROVIDER_unload(legacy_provider);
    }

    if (NULL != default_provider)
    {
        OSSL_PROVIDER_unload(default_provider);
    }

    return 0;
}