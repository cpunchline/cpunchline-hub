//
// Copyright 2018 Staysail Systems, Inc. <info@staysail.tech>
// Copyright 2018 Capitar IT Group BV <info@capitar.com>
//
// This software is supplied under the terms of the MIT License, a
// copy of which should be located in the distribution where this
// file was obtained (LICENSE.txt).  A copy of the license may also be
// found online at https://opensource.org/licenses/MIT.
//

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include <stddef.h>

typedef struct
{
    uint32_t digest[5]; // resulting digest
    uint64_t len;       // length in bits
    uint8_t blk[64];    // message block
    int idx;            // index of next byte in block
} sha1_ctx;

extern void sha1_init(sha1_ctx *);
extern void sha1_update(sha1_ctx *, const void *, size_t);
extern void sha1_final(sha1_ctx *, uint8_t[20]);
extern void sha1(const void *, size_t, uint8_t[20]);

#ifdef __cplusplus
}
#endif