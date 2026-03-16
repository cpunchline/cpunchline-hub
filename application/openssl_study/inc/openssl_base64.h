#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <stddef.h>

// bin to ASCII
int openssl_base64_encode(const void *_src, size_t src_len, void *dest, size_t *dest_len);
int openssl_base64_decode(const void *_src, size_t src_len, void *dest, size_t *dest_len);

#ifdef __cplusplus
}
#endif