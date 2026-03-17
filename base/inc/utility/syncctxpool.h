#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <pthread.h>
#include "dsa/list.h"

typedef struct _syncctxpool_t syncctxpool_t;
typedef struct _syncctx_t syncctx_t;

typedef struct _syncctx_t
{
    struct list_head list;
    uint8_t *resp_data;
    size_t *resp_len;
    int result; // 0=ok, -1=error
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} syncctx_t;

typedef struct _syncctxpool_t
{
    struct list_head head;
    pthread_mutex_t mutex;
    size_t capacity;
    size_t size; // free count
} syncctxpool_t;

syncctxpool_t *syncctxpool_create(size_t init_count);
syncctx_t *syncctxpool_borrow(syncctxpool_t *pool);
void syncctxpool_return(syncctxpool_t *pool, syncctx_t *ctx);
void syncctxpool_destroy(syncctxpool_t *);

#ifdef __cplusplus
}
#endif
