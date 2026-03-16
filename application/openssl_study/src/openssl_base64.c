#include "openssl/evp.h"
#include "openssl/buffer.h"
#include "openssl/bio.h"

#include "openssl_print.h"
#include "openssl_base64.h"

#define BASE64_ENCODE_LEN(_len) ((((_len) + 2) / 3) * 4 + 1)
#define BASE64_DECODE_LEN(_len) (((_len) / 4) * 3 + 1)

int openssl_base64_encode(const void *_src, size_t src_len, void *dest, size_t *dest_len)
{
    BIO *bhead = NULL;
    BIO *bmem = NULL;
    BIO *b64 = NULL;
    BUF_MEM *bptr = NULL;
    int ret = 0;
    size_t writebytes = 0;

    if (NULL == _src || NULL == dest || NULL == dest_len)
    {
        LOG_PRINT_ERROR("invalid param!");
        return -1;
    }

    do
    {
        bmem = BIO_new(BIO_s_mem());
        if (NULL == bmem)
        {
            print_openssl_err();
            ret = -1;
            break;
        }

        b64 = BIO_new(BIO_f_base64());
        if (NULL == b64)
        {
            print_openssl_err();
            ret = -1;
            break;
        }

        // bhead => b64 => bmem <= btail
        bhead = BIO_push(b64, bmem); // bhead == b64
        if (NULL == bhead)
        {
            print_openssl_err();
            ret = -1;
            break;
        }

        BIO_set_flags(bhead, BIO_FLAGS_BASE64_NO_NL); // 64 bytes not add '\n'
        if (0 == BIO_write_ex(bhead, _src, src_len, &writebytes))
        {
            print_openssl_err();
            ret = -1;
            break;
        }

        if (writebytes != src_len)
        {
            LOG_PRINT_ERROR("Only wrote %zu of %zu bytes.", writebytes, src_len);
            ret = -1;
            break;
        }

        if (0 == BIO_flush(bhead))
        {
            print_openssl_err();
            ret = -1;
            break;
        }

        BIO_get_mem_ptr(bmem, &bptr);
        if (NULL == bptr)
        {
            print_openssl_err();
            ret = -1;
            break;
        }

        if (bptr->length <= *dest_len)
        {
            memcpy(dest, bptr->data, bptr->length);
        }
        else
        {
            LOG_PRINT_ERROR("input dest_len[%zu] < encode_len[%zu] is limit", *dest_len, bptr->length);
            ret = -1;
            break;
        }

        *dest_len = bptr->length;
        ret = 0;
    } while (0);

#if 0
    if (NULL != bmem)
    {
        BIO_free(bmem);
    }

    if (NULL != b64)
    {
        BIO_free(b64);
    }
#else
    if (NULL != bhead)
    {
        BIO_free_all(bhead);
    }
#endif

    return ret;
}

int openssl_base64_decode(const void *_src, size_t src_len, void *dest, size_t *dest_len)
{
    BIO *bhead = NULL;
    BIO *bmem = NULL;
    BIO *b64 = NULL;
    int ret = 0;
    size_t readbytes = 0;

    if (NULL == _src || NULL == dest || NULL == dest_len)
    {
        LOG_PRINT_ERROR("invalid param!");
        return -1;
    }

    do
    {
        if (*dest_len < BASE64_DECODE_LEN(src_len))
        {
            LOG_PRINT_ERROR("input dest_len[%zu] < decode_len[%zu]!", *dest_len, BASE64_DECODE_LEN(src_len));
            ret = -1;
            break;
        }

        bmem = BIO_new_mem_buf(_src, (int)src_len);
        if (NULL == bmem)
        {
            print_openssl_err();
            ret = -1;
            break;
        }

        b64 = BIO_new(BIO_f_base64());
        if (NULL == b64)
        {
            print_openssl_err();
            ret = -1;
            break;
        }

        // bhead => b64 => bmem <= btail
        bhead = BIO_push(b64, bmem); // bhead == b64
        if (NULL == bhead)
        {
            print_openssl_err();
            ret = -1;
            break;
        }

        BIO_set_flags(bhead, BIO_FLAGS_BASE64_NO_NL); // if exist '\n' in _src, then decode fail
        if (0 == BIO_read_ex(bhead, dest, *dest_len, &readbytes))
        {
            print_openssl_err();
            ret = -1;
            break;
        }
        *dest_len = readbytes;
        ret = 0;
    } while (0);

#if 0
    if (NULL != bmem)
    {
        BIO_free(bmem);
    }

    if (NULL != b64)
    {
        BIO_free(b64);
    }
#else
    if (NULL != bhead)
    {
        BIO_free_all(bhead);
    }
#endif

    return ret;
}