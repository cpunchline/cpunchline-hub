#include <sys/stat.h>
#include "openssl/evp.h"
#include "dsa/darray.h"
#include "openssl_print.h"
#include "openssl_md.h"

#define DEFAULT_EACH_HANDLE_READ_BLOCK_SIZE (64)

int openssl_provider_digest(const char *digest_name, const void *_src, size_t src_len, void *dest, size_t *dest_len)
{
    int ret = 0;
    EVP_MD *md = NULL;
    EVP_MD_CTX *ctx = NULL;
    unsigned int output_len = 0;

    if (NULL == digest_name || NULL == _src || NULL == dest || NULL == dest_len)
    {
        LOG_PRINT_ERROR("invalid param!");
        return -1;
    }

    do
    {
        md = EVP_MD_fetch(NULL, digest_name, NULL);
        if (NULL == md)
        {
            LOG_PRINT_ERROR("EVP_MD_fetch fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if ((EVP_MD_get_flags(md) & EVP_MD_FLAG_XOF))
        {
            LOG_PRINT_ERROR("'%s' is an XOF algorithm!", digest_name);
            ret = -1;
            break;
        }

        size_t md_size = (size_t)EVP_MD_get_size(md);
        if (*dest_len < md_size)
        {
            LOG_PRINT_ERROR("dest_len[%zu] < md_size[%zu]!", *dest_len, md_size);
            ret = -1;
            break;
        }

        ctx = EVP_MD_CTX_new();
        if (NULL == ctx)
        {
            LOG_PRINT_ERROR("EVP_MD_CTX_new fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (1 != EVP_DigestInit_ex(ctx, md, NULL))
        {
            LOG_PRINT_ERROR("EVP_DigestInit_ex fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        // if use by file, may be while
        if (1 != EVP_DigestUpdate(ctx, _src, src_len))
        {
            LOG_PRINT_ERROR("EVP_DigestUpdate fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (1 != EVP_DigestFinal_ex(ctx, dest, &output_len))
        {
            LOG_PRINT_ERROR("EVP_DigestFinal_ex fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        *dest_len = output_len;
        ret = 0;
    } while (0);

    if (NULL != ctx)
    {
        EVP_MD_CTX_free(ctx);
    }

    if (NULL != md)
    {
        EVP_MD_free(md);
    }

    return ret;
}

int openssl_provider_xof_digest(const char *xof_digest_name, const void *_src, size_t src_len, void *dest, size_t dest_len)
{
    int ret = 0;
    EVP_MD *md = NULL;
    EVP_MD_CTX *ctx = NULL;

    if (NULL == xof_digest_name || NULL == _src || NULL == dest || 0 == dest_len)
    {
        LOG_PRINT_ERROR("invalid param!");
        return -1;
    }

    do
    {
        md = EVP_MD_fetch(NULL, xof_digest_name, NULL);
        if (NULL == md)
        {
            LOG_PRINT_ERROR("EVP_MD_fetch fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (!(EVP_MD_get_flags(md) & EVP_MD_FLAG_XOF))
        {
            LOG_PRINT_ERROR("'%s' is not an XOF algorithm!", xof_digest_name);
            ret = -1;
            break;
        }

        ctx = EVP_MD_CTX_new();
        if (NULL == ctx)
        {
            LOG_PRINT_ERROR("EVP_MD_CTX_new fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (1 != EVP_DigestInit_ex(ctx, md, NULL))
        {
            LOG_PRINT_ERROR("EVP_DigestInit_ex fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        // if use by file, may be while
        if (1 != EVP_DigestUpdate(ctx, _src, src_len))
        {
            LOG_PRINT_ERROR("EVP_DigestUpdate fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (1 != EVP_DigestFinalXOF(ctx, dest, dest_len))
        {
            LOG_PRINT_ERROR("EVP_DigestFinalXOF fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        ret = 0;
    } while (0);

    if (NULL != ctx)
    {
        EVP_MD_CTX_free(ctx);
    }

    if (NULL != md)
    {
        EVP_MD_free(md);
    }

    return ret;
}

int openssl_provider_digest_file(const char *digest_name, size_t each_handle_block_size, char *file_path, void *dest, size_t *dest_len)
{
    int ret = 0;
    BIO *bio = NULL;
    EVP_MD *md = NULL;
    EVP_MD_CTX *ctx = NULL;
    size_t default_each_handle_read_block_size = DEFAULT_EACH_HANDLE_READ_BLOCK_SIZE;
    uint8_t *each_handle_read_block = NULL;
    int each_handle_read = 0;
    unsigned int output_len = 0;

    if (NULL == digest_name || NULL == file_path || NULL == dest || NULL == dest_len)
    {
        LOG_PRINT_ERROR("invalid param!");
        return -1;
    }

    do
    {
        default_each_handle_read_block_size = (each_handle_block_size > default_each_handle_read_block_size) ? each_handle_block_size : default_each_handle_read_block_size;
        each_handle_read_block = calloc(1, default_each_handle_read_block_size);
        if (NULL == each_handle_read_block)
        {
            LOG_PRINT_ERROR("calloc fail, err[%d](%s)", errno, strerror(errno));
            ret = -1;
            break;
        }

        md = EVP_MD_fetch(NULL, digest_name, NULL);
        if (NULL == md)
        {
            LOG_PRINT_ERROR("EVP_MD_fetch fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if ((EVP_MD_get_flags(md) & EVP_MD_FLAG_XOF))
        {
            LOG_PRINT_ERROR("'%s' is an XOF algorithm!", digest_name);
            ret = -1;
            break;
        }

        size_t md_size = (size_t)EVP_MD_get_size(md);
        if (*dest_len < md_size)
        {
            LOG_PRINT_ERROR("dest_len[%zu] < md_size[%zu]!", *dest_len, md_size);
            ret = -1;
            break;
        }

        ctx = EVP_MD_CTX_new();
        if (NULL == ctx)
        {
            LOG_PRINT_ERROR("EVP_MD_CTX_new fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (1 != EVP_DigestInit_ex(ctx, md, NULL))
        {
            LOG_PRINT_ERROR("EVP_DigestInit_ex fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        bio = BIO_new_file(file_path, "rb");
        if (NULL == bio)
        {
            LOG_PRINT_ERROR("BIO_new_file fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        while ((each_handle_read = BIO_read(bio, each_handle_read_block, (int)default_each_handle_read_block_size)) > 0)
        {
            if (1 != EVP_DigestUpdate(ctx, each_handle_read_block, (size_t)each_handle_read))
            {
                LOG_PRINT_ERROR("EVP_DigestUpdate fail!");
                print_openssl_err();
                ret = -1;
                break;
            }
        }

        if (each_handle_read < 0)
        {
            if (BIO_should_retry(bio))
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

        if (1 != EVP_DigestFinal_ex(ctx, dest, &output_len))
        {
            LOG_PRINT_ERROR("EVP_DigestFinal_ex fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        *dest_len = (size_t)output_len;
        ret = 0;

    } while (0);

    if (NULL != bio)
    {
        BIO_free(bio);
    }

    if (NULL != ctx)
    {
        EVP_MD_CTX_free(ctx);
    }

    if (NULL != md)
    {
        EVP_MD_free(md);
    }

    if (NULL != each_handle_read_block)
    {
        free(each_handle_read_block);
    }

    return ret;
}

int openssl_provider_xof_digest_file(const char *xof_digest_name, size_t each_handle_block_size, char *file_path, void *dest, size_t dest_len)
{
    int ret = 0;
    BIO *bio = NULL;
    EVP_MD *md = NULL;
    EVP_MD_CTX *ctx = NULL;
    size_t default_each_handle_read_block_size = DEFAULT_EACH_HANDLE_READ_BLOCK_SIZE;
    uint8_t *each_handle_read_block = NULL;
    int each_handle_read = 0;

    if (NULL == xof_digest_name || NULL == file_path || 0 == dest_len)
    {
        LOG_PRINT_ERROR("invalid param!");
        return -1;
    }

    do
    {
        default_each_handle_read_block_size = (each_handle_block_size > default_each_handle_read_block_size) ? each_handle_block_size : default_each_handle_read_block_size;
        each_handle_read_block = calloc(1, default_each_handle_read_block_size);
        if (NULL == each_handle_read_block)
        {
            LOG_PRINT_ERROR("calloc fail, err[%d](%s)", errno, strerror(errno));
            ret = -1;
            break;
        }

        md = EVP_MD_fetch(NULL, xof_digest_name, NULL);
        if (NULL == md)
        {
            LOG_PRINT_ERROR("EVP_MD_fetch fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (!(EVP_MD_get_flags(md) & EVP_MD_FLAG_XOF))
        {
            LOG_PRINT_ERROR("'%s' is not an XOF algorithm!", xof_digest_name);
            ret = -1;
            break;
        }

        ctx = EVP_MD_CTX_new();
        if (NULL == ctx)
        {
            LOG_PRINT_ERROR("EVP_MD_CTX_new fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (1 != EVP_DigestInit_ex(ctx, md, NULL))
        {
            LOG_PRINT_ERROR("EVP_DigestInit_ex fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        bio = BIO_new_file(file_path, "rb");
        if (NULL == bio)
        {
            LOG_PRINT_ERROR("BIO_new_file fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        while ((each_handle_read = BIO_read(bio, each_handle_read_block, (int)default_each_handle_read_block_size)) > 0)
        {
            if (1 != EVP_DigestUpdate(ctx, each_handle_read_block, (size_t)each_handle_read))
            {
                LOG_PRINT_ERROR("EVP_DigestUpdate fail!");
                print_openssl_err();
                ret = -1;
                break;
            }
        }

        if (each_handle_read < 0)
        {
            if (BIO_should_retry(bio))
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

        if (1 != EVP_DigestFinalXOF(ctx, dest, dest_len))
        {
            LOG_PRINT_ERROR("EVP_DigestFinal_ex fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        ret = 0;

    } while (0);

    if (NULL != bio)
    {
        BIO_free(bio);
    }

    if (NULL != ctx)
    {
        EVP_MD_CTX_free(ctx);
    }

    if (NULL != md)
    {
        EVP_MD_free(md);
    }

    if (NULL != each_handle_read_block)
    {
        free(each_handle_read_block);
    }

    return ret;
}

int openssl_provider_digest_list_file(const char *digest_name, size_t each_handle_block_size, char *file_path, void *dest, size_t *dest_len)
{
    int ret = 0;
    BIO *bio = NULL;
    EVP_MD *md = NULL;
    EVP_MD_CTX *ctx = NULL;

    EVP_MD_CTX *each_ctx = NULL;
    size_t default_each_handle_read_block_size = DEFAULT_EACH_HANDLE_READ_BLOCK_SIZE;
    uint8_t *each_handle_read_block = NULL;
    int each_handle_read = 0;
    uint8_t *each_handle_digest = NULL;
    unsigned int each_handle_digest_size = 0;
    unsigned int output_len = 0;

    darray_uchar dest_array = darray_new();

    if (NULL == digest_name || NULL == file_path || NULL == dest || NULL == dest_len)
    {
        LOG_PRINT_ERROR("invalid param!");
        return -1;
    }

    do
    {
        default_each_handle_read_block_size = (each_handle_block_size > default_each_handle_read_block_size) ? each_handle_block_size : default_each_handle_read_block_size;
        each_handle_read_block = calloc(1, default_each_handle_read_block_size);
        if (NULL == each_handle_read_block)
        {
            LOG_PRINT_ERROR("calloc fail, err[%d](%s)", errno, strerror(errno));
            ret = -1;
            break;
        }

        // each_handle_digest_size must < default_each_handle_read_block_size
        each_handle_digest = calloc(1, default_each_handle_read_block_size);
        if (NULL == each_handle_digest)
        {
            LOG_PRINT_ERROR("calloc fail, err[%d](%s)", errno, strerror(errno));
            ret = -1;
            break;
        }

        md = EVP_MD_fetch(NULL, digest_name, NULL);
        if (NULL == md)
        {
            LOG_PRINT_ERROR("EVP_MD_fetch fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if ((EVP_MD_get_flags(md) & EVP_MD_FLAG_XOF))
        {
            LOG_PRINT_ERROR("'%s' is an XOF algorithm!", digest_name);
            ret = -1;
            break;
        }

        size_t md_size = (size_t)EVP_MD_get_size(md);
        if (*dest_len < md_size)
        {
            LOG_PRINT_ERROR("dest_len[%zu] < md_size[%zu]!", *dest_len, md_size);
            ret = -1;
            break;
        }

        ctx = EVP_MD_CTX_new();
        if (NULL == ctx)
        {
            LOG_PRINT_ERROR("EVP_MD_CTX_new fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (1 != EVP_DigestInit_ex(ctx, md, NULL))
        {
            LOG_PRINT_ERROR("EVP_DigestInit_ex fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        bio = BIO_new_file(file_path, "rb");
        if (NULL == bio)
        {
            LOG_PRINT_ERROR("BIO_new_file fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        each_ctx = EVP_MD_CTX_new();
        if (NULL == each_ctx)
        {
            LOG_PRINT_ERROR("EVP_MD_CTX_new fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        while (1)
        {
            each_handle_read = BIO_read(bio, each_handle_read_block, (int)default_each_handle_read_block_size);
            if (each_handle_read <= 0)
            {
                break;
            }

            if (1 != EVP_DigestInit_ex(each_ctx, md, NULL))
            {
                LOG_PRINT_ERROR("EVP_DigestInit_ex fail!");
                print_openssl_err();
                EVP_MD_CTX_free(each_ctx);
                ret = -1;
                break;
            }

            if (1 != EVP_DigestUpdate(each_ctx, each_handle_read_block, (size_t)each_handle_read))
            {
                LOG_PRINT_ERROR("EVP_DigestUpdate fail!");
                print_openssl_err();
                EVP_MD_CTX_free(each_ctx);
                ret = -1;
                break;
            }

            if (1 != EVP_DigestFinal_ex(each_ctx, each_handle_digest, &each_handle_digest_size))
            {
                LOG_PRINT_ERROR("EVP_DigestFinal_ex fail!");
                print_openssl_err();
                EVP_MD_CTX_free(each_ctx);
                ret = -1;
                break;
            }
            darray_append_items(dest_array, each_handle_digest, each_handle_digest_size);
            EVP_MD_CTX_free(each_ctx);
        }

        if (each_handle_read < 0 && BIO_should_retry(bio))
        {
            if (BIO_should_retry(bio))
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

        if (0 != ret)
        {
            break;
        }

        if (1 != EVP_DigestUpdate(ctx, dest_array.item, darray_size(dest_array)))
        {
            LOG_PRINT_ERROR("EVP_DigestUpdate fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (1 != EVP_DigestFinal_ex(ctx, dest, &output_len))
        {
            LOG_PRINT_ERROR("EVP_DigestFinal_ex fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        *dest_len = (size_t)output_len;
        ret = 0;
    } while (0);

    if (NULL != bio)
    {
        BIO_free(bio);
    }

    if (NULL != ctx)
    {
        EVP_MD_CTX_free(ctx);
    }

    if (NULL != md)
    {
        EVP_MD_free(md);
    }

    darray_free(dest_array);

    if (NULL != each_handle_digest)
    {
        free(each_handle_digest);
    }

    if (NULL != each_handle_read_block)
    {
        free(each_handle_read_block);
    }

    return ret;
}

int openssl_provider_digest_merkle_tree_file(const char *digest_name, size_t each_handle_block_size, char *file_path, void *dest, size_t *dest_len)
{
    int ret = 0;
    BIO *bio = NULL;
    EVP_MD *md = NULL;
    EVP_MD_CTX *ctx = NULL;

    EVP_MD_CTX *each_ctx = NULL;
    size_t default_each_handle_read_block_size = DEFAULT_EACH_HANDLE_READ_BLOCK_SIZE;
    uint8_t *each_handle_read_block = NULL;
    int each_handle_read = 0;
    uint8_t *each_handle_digest = NULL;
    unsigned int each_handle_digest_size = 0;

    uint8_t *other_each_handle_digest = NULL;
    size_t other_each_handle_digest_size = 0;
    unsigned int output_len = 0;

    size_t i = 0;
    darray_uchar *j = NULL;

    darray(darray_uchar) merkle_tree_array = darray_new();

    if (NULL == digest_name || NULL == file_path || NULL == dest || NULL == dest_len)
    {
        LOG_PRINT_ERROR("invalid param!");
        return -1;
    }

    do
    {
        default_each_handle_read_block_size = (each_handle_block_size > default_each_handle_read_block_size) ? each_handle_block_size : default_each_handle_read_block_size;
        each_handle_read_block = calloc(1, default_each_handle_read_block_size);
        if (NULL == each_handle_read_block)
        {
            LOG_PRINT_ERROR("calloc fail, err[%d](%s)", errno, strerror(errno));
            ret = -1;
            break;
        }

        // each_handle_digest_size must < default_each_handle_read_block_size
        each_handle_digest = calloc(1, default_each_handle_read_block_size);
        if (NULL == each_handle_digest)
        {
            LOG_PRINT_ERROR("calloc fail, err[%d](%s)", errno, strerror(errno));
            ret = -1;
            break;
        }

        // other_each_handle_digest_size must < default_each_handle_read_block_size
        other_each_handle_digest = calloc(1, default_each_handle_read_block_size);
        if (NULL == other_each_handle_digest)
        {
            LOG_PRINT_ERROR("calloc fail, err[%d](%s)", errno, strerror(errno));
            ret = -1;
            break;
        }

        md = EVP_MD_fetch(NULL, digest_name, NULL);
        if (NULL == md)
        {
            LOG_PRINT_ERROR("EVP_MD_fetch fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        size_t md_size = (size_t)EVP_MD_get_size(md);
        if (*dest_len < md_size)
        {
            LOG_PRINT_ERROR("dest_len[%zu] < md_size[%zu]!", *dest_len, md_size);
            ret = -1;
            break;
        }

        ctx = EVP_MD_CTX_new();
        if (NULL == ctx)
        {
            LOG_PRINT_ERROR("EVP_MD_CTX_new fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (1 != EVP_DigestInit_ex(ctx, md, NULL))
        {
            LOG_PRINT_ERROR("EVP_DigestInit_ex fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        bio = BIO_new_file(file_path, "rb");
        if (NULL == bio)
        {
            LOG_PRINT_ERROR("BIO_new_file fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        each_ctx = EVP_MD_CTX_new();
        if (NULL == each_ctx)
        {
            LOG_PRINT_ERROR("EVP_MD_CTX_new fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        while (1)
        {
            each_handle_read = BIO_read(bio, each_handle_read_block, (int)default_each_handle_read_block_size);
            if (each_handle_read <= 0)
            {
                break;
            }

            if (1 != EVP_DigestInit_ex(each_ctx, md, NULL))
            {
                LOG_PRINT_ERROR("EVP_DigestInit_ex fail!");
                print_openssl_err();
                EVP_MD_CTX_free(each_ctx);
                ret = -1;
                break;
            }

            if (1 != EVP_DigestUpdate(each_ctx, each_handle_read_block, (size_t)each_handle_read))
            {
                LOG_PRINT_ERROR("EVP_DigestUpdate fail!");
                print_openssl_err();
                EVP_MD_CTX_free(each_ctx);
                ret = -1;
                break;
            }

            if (1 != EVP_DigestFinal_ex(each_ctx, each_handle_digest, &each_handle_digest_size))
            {
                LOG_PRINT_ERROR("EVP_DigestFinal_ex fail!");
                print_openssl_err();
                EVP_MD_CTX_free(each_ctx);
                ret = -1;
                break;
            }
            darray_uchar new = darray_new();
            darray_append_items(new, each_handle_digest, each_handle_digest_size);
            darray_append(merkle_tree_array, new);
            EVP_MD_CTX_free(each_ctx);
        }

        if (each_handle_read < 0 && BIO_should_retry(bio))
        {
            if (BIO_should_retry(bio))
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

        if (0 != ret)
        {
            break;
        }

        if (darray_empty(merkle_tree_array))
        {
            LOG_PRINT_ERROR("empty file!");
            ret = -1;
            break;
        }

        while (darray_size(merkle_tree_array) > 1)
        {
            if (darray_size(merkle_tree_array) & 1)
            {
                darray_uchar back = darray_new();
                darray_append_items(back, darray_item(merkle_tree_array, darray_size(merkle_tree_array) - 1).item, darray_size(darray_item(merkle_tree_array, darray_size(merkle_tree_array) - 1)));
                darray_append(merkle_tree_array, back);
            }

            for (i = 0; i < darray_size(merkle_tree_array) / 2; ++i)
            {
                darray_uchar tmp = darray_new();
                darray_append_items(tmp, darray_item(merkle_tree_array, i * 2).item, darray_size(darray_item(merkle_tree_array, i * 2)));
                darray_append_items(tmp, darray_item(merkle_tree_array, i * 2 + 1).item, darray_size(darray_item(merkle_tree_array, i * 2 + 1)));
                memset(other_each_handle_digest, 0x00, default_each_handle_read_block_size);
                other_each_handle_digest_size = default_each_handle_read_block_size;
                if (0 != openssl_provider_digest(digest_name, tmp.item, darray_size(tmp), other_each_handle_digest, &other_each_handle_digest_size))
                {
                    LOG_PRINT_ERROR("openssl_provider_digest fail!");
                    darray_free(tmp);
                    break;
                }
                darray_free(tmp);

                darray_uchar new = darray_new();
                darray_append_items(new, other_each_handle_digest, other_each_handle_digest_size);
                darray_remove(merkle_tree_array, i);
                darray_insert(merkle_tree_array, i, new);
            }

            size_t max_i = darray_size(merkle_tree_array) - 1;
            size_t max_size = darray_size(merkle_tree_array) / 2;
            j = NULL;
            darray_foreach_reverse(j, merkle_tree_array)
            {
                if (0 == max_size || max_i < max_size)
                {
                    break;
                }
                (void)darray_pop(merkle_tree_array);
                darray_free(*j);
                max_i--;
            }
        }

        if (1 != EVP_DigestUpdate(ctx, darray_item(merkle_tree_array, 0).item, darray_size(darray_item(merkle_tree_array, 0))))
        {
            LOG_PRINT_ERROR("EVP_DigestUpdate fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (1 != EVP_DigestFinal_ex(ctx, dest, &output_len))
        {
            LOG_PRINT_ERROR("EVP_DigestFinal_ex fail!");
            print_openssl_err();
            ret = -1;
            break;
        }

        *dest_len = output_len;
        ret = 0;

    } while (0);

    if (NULL != bio)
    {
        BIO_free(bio);
    }

    if (NULL != ctx)
    {
        EVP_MD_CTX_free(ctx);
    }

    if (NULL != md)
    {
        EVP_MD_free(md);
    }

    j = NULL;
    darray_foreach(j, merkle_tree_array)
    {
        darray_free(*j);
    }
    darray_free(merkle_tree_array);

    if (NULL != each_handle_digest)
    {
        free(each_handle_digest);
    }

    if (NULL != other_each_handle_digest)
    {
        free(other_each_handle_digest);
    }

    if (NULL != each_handle_read_block)
    {
        free(each_handle_read_block);
    }

    return ret;
}