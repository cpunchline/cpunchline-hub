#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include <stdlib.h>

typedef uint32_t shm_block_id_e;

typedef struct shm_block_t
{
    uint32_t block_id;
    size_t block_size;
} shm_block_t;

int shm_init_manager(const shm_block_t *blocks, size_t count);
int shm_init_worker(void);
int shm_lock(uint32_t block_id);
int shm_unlock(uint32_t block_id);
void *shm_get_block_addr(uint32_t block_id);
int shm_get_block_data(uint32_t block_id, size_t len, void *buf);
int shm_set_block_data(uint32_t block_id, size_t len, const void *buf);

#ifdef __cplusplus
}
#endif
