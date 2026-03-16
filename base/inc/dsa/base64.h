#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <stddef.h>

#define BASE64_ENCODE_LEN(_len) ((((_len) + 2) / 3) * 4 + 1)
#define BASE64_DECODE_LEN(_len) (((_len) / 4) * 3 + 1)

int base64_encode(const void *_src, size_t src_len, void *dest, size_t dest_len);
int base64_decode(const void *_src, void *dest, size_t dest_len);

#ifdef __cplusplus
}
#endif