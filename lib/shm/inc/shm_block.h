#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>

#define SHM_BLOCK_ID_TEST (0)

typedef struct shm_test_block_t
{
    uint8_t test[1024];
} shm_test_block_t;

#ifdef __cplusplus
}
#endif