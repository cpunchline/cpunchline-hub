
#include "utility/utils.h"
#include "utility/syncctxpool.h"

static syncctx_t *syncctx_create(void)
{
    syncctx_t *ctx = (syncctx_t *)calloc(1, sizeof(syncctx_t));
    if (NULL != ctx)
    {
        pthread_mutexattr_t mutexattr = {};
        pthread_mutexattr_init(&mutexattr);
        (void)pthread_mutexattr_settype(&mutexattr, PTHREAD_MUTEX_ERRORCHECK);

        while ((pthread_mutex_init(&ctx->mutex, &mutexattr) != 0) &&
               (pthread_mutex_init(&ctx->mutex, NULL) != 0))
        {
            // We must have memory exhaustion -- ENOMEM, or
            // in some cases EAGAIN.  Wait a bit before we try to
            // give things a chance to settle down.
            util_msleep(10);
        }
        pthread_cond_init(&ctx->cond, NULL);
        ctx->resp_data = NULL;
        ctx->resp_len = NULL;
        ctx->result = -1;
        INIT_LIST_HEAD(&ctx->list);
        pthread_mutexattr_destroy(&mutexattr);
    }
    else
    {
        LOG_PRINT_ERROR("calloc fail, errno[%d](%s)", errno, strerror(errno));
    }

    return ctx;
}

static void syncctx_destroy(syncctx_t *ctx)
{
    if (NULL == ctx)
    {
        return;
    }

    pthread_mutex_destroy(&ctx->mutex);
    pthread_cond_destroy(&ctx->cond);
    free(ctx);
}

syncctxpool_t *syncctxpool_create(size_t init_count)
{
    syncctxpool_t *pool = (syncctxpool_t *)calloc(1, sizeof(syncctxpool_t));
    if (NULL == pool)
    {
        LOG_PRINT_ERROR("calloc fail, errno[%d](%s)", errno, strerror(errno));
        return NULL;
    }

    INIT_LIST_HEAD(&pool->head);
    pool->capacity = 0;
    pool->size = 0;

    pthread_mutexattr_t mutexattr = {};
    pthread_mutexattr_init(&mutexattr);
    (void)pthread_mutexattr_settype(&mutexattr, PTHREAD_MUTEX_ERRORCHECK);

    for (size_t i = 0; i < init_count; ++i)
    {
        syncctx_t *ctx = syncctx_create();
        if (NULL != ctx)
        {
            list_add(&ctx->list, &pool->head);
            pool->capacity++;
            pool->size++;
        }
        else
        {
            LOG_PRINT_ERROR("calloc fail, errno[%d](%s)", errno, strerror(errno));
            break;
        }
    }

    // Initialize Mutex
    while ((pthread_mutex_init(&pool->mutex, &mutexattr) != 0) &&
           (pthread_mutex_init(&pool->mutex, NULL) != 0))
    {
        // We must have memory exhaustion -- ENOMEM, or
        // in some cases EAGAIN.  Wait a bit before we try to
        // give things a chance to settle down.
        util_msleep(10);
    }
    pthread_mutexattr_destroy(&mutexattr);

    return pool;
}

void syncctxpool_destroy(syncctxpool_t *pool)
{
    if (NULL == pool)
    {
        return;
    }

    syncctx_t *pos = NULL, *n = NULL;

    list_for_each_entry_safe(pos, n, &pool->head, list)
    {
        list_del_init(&pos->list);
        syncctx_destroy(pos);
    }

    pthread_mutex_destroy(&pool->mutex);
    free(pool);
}

syncctx_t *syncctxpool_borrow(syncctxpool_t *pool)
{
    if (NULL == pool)
    {
        return NULL;
    }

    syncctx_t *idle_ctx = NULL;
    pthread_mutex_lock(&pool->mutex);
    if (pool->size > 0) // if (!list_empty(&pool->head))
    {
        idle_ctx = list_first_entry(&pool->head, syncctx_t, list);
        list_del_init(&idle_ctx->list);
        pool->size--;
    }
    else
    {
        idle_ctx = syncctx_create();
        if (NULL != idle_ctx)
        {
            // no need add to list, just return it
            pool->capacity++;
            pool->size++;
        }
        else
        {
            LOG_PRINT_ERROR("syncctx_create fail!");
        }
    }
    pthread_mutex_unlock(&pool->mutex);

    LOG_PRINT_DEBUG("borrow from SyncCtx Pool, capacity[%zu], free count[%zu]", pool->capacity, pool->size);

    return idle_ctx;
}

void syncctxpool_return(syncctxpool_t *pool, syncctx_t *ctx)
{
    if (NULL == ctx || NULL == pool)
    {
        return;
    }

    ctx->resp_data = NULL;
    ctx->resp_len = NULL;
    ctx->result = -1;
    pthread_mutex_lock(&pool->mutex);
    list_add(&ctx->list, &pool->head);
    pool->size++;
    pthread_mutex_unlock(&pool->mutex);

    LOG_PRINT_DEBUG("SyncCtx returned to pool, capacity[%zu], free count[%zu]", pool->capacity, pool->size);
}