#include <pthread.h>
#include <sys/queue.h>
#include "utility/utils.h"
#include "utility/threadqueue.h"

struct threadboundedqueue_node_t
{
    bool is_extern;
    size_t len;
    uint8_t *p_extern;
    uint8_t data[];
};
typedef struct threadboundedqueue_node_t threadboundedqueue_node_t;

struct threadunboundedqueue_node_t
{
    TAILQ_ENTRY(threadunboundedqueue_node_t)
    entries;
    size_t len;
    uint8_t data[];
};
typedef struct threadunboundedqueue_node_t threadunboundedqueue_node_t;

struct threadboundedqueue_t
{
    bool is_run;
    pthread_mutex_t mutex;
    pthread_cond_t cond_notfull;
    pthread_cond_t cond_notempty;
    void *nodes;
    size_t node_size;
    size_t capacity;
    size_t count;
    size_t head;
    size_t tail;
};

struct threadunboundedqueue_t
{
    bool is_run;
    pthread_mutex_t mutex;
    pthread_cond_t cond_notempty;
    TAILQ_HEAD(threadunboundedqueue_head_t, threadunboundedqueue_node_t)
    queue;
    size_t size;
};

void threadboundedqueue_uninit(threadboundedqueue_t **q)
{
    if (NULL == q || NULL == *q)
    {
        LOG_PRINT_ERROR("invalid param!");
        return;
    }

    threadboundedqueue_node_t *node = NULL;
    size_t node_total_size = sizeof(threadboundedqueue_node_t) + (*q)->node_size;
    for (size_t i = 0; i < (*q)->capacity; ++i)
    {
        node = (threadboundedqueue_node_t *)((uint8_t *)(*q)->nodes + i * node_total_size);
        if (node->is_extern && NULL != node->p_extern)
        {
            free(node->p_extern);
            node->p_extern = NULL;
        }
    }

    free((*q)->nodes);
    (*q)->nodes = NULL;
    pthread_mutex_destroy(&(*q)->mutex);
    pthread_cond_destroy(&(*q)->cond_notfull);
    pthread_cond_destroy(&(*q)->cond_notempty);
    free(*q);
    *q = NULL;
}

threadboundedqueue_t *threadboundedqueue_create(size_t node_num, size_t node_size)
{
    threadboundedqueue_t *q = NULL;

    q = (threadboundedqueue_t *)calloc(1, sizeof(threadboundedqueue_t));
    if (NULL == q)
    {
        LOG_PRINT_ERROR("calloc fail, errno(%d)[%s]", errno, strerror(errno));
        return NULL;
    }

    pthread_mutexattr_t mutexattr = {};
    pthread_mutexattr_init(&mutexattr);
    (void)pthread_mutexattr_settype(&mutexattr, PTHREAD_MUTEX_ERRORCHECK);
    while ((pthread_mutex_init(&q->mutex, &mutexattr) != 0) &&
           (pthread_mutex_init(&q->mutex, NULL) != 0))
    {
        // We must have memory exhaustion -- ENOMEM, or
        // in some cases EAGAIN.  Wait a bit before we try to
        // give things a chance to settle down.
        util_msleep(10);
    }
    pthread_mutexattr_destroy(&mutexattr);

    pthread_condattr_t condattr = {};
    pthread_condattr_init(&condattr);
    pthread_condattr_setclock(&condattr, CLOCK_MONOTONIC);
    while (pthread_cond_init(&q->cond_notfull, &condattr) != 0)
    {
        util_msleep(10);
    }
    while (pthread_cond_init(&q->cond_notempty, &condattr) != 0)
    {
        util_msleep(10);
    }
    pthread_condattr_destroy(&condattr);

    size_t node_total_size = sizeof(threadboundedqueue_node_t) + node_size;
    q->nodes = (threadboundedqueue_node_t *)calloc(node_num, node_total_size);
    if (NULL == q->nodes)
    {
        LOG_PRINT_ERROR("calloc fail, errno(%d)[%s]", errno, strerror(errno));
        free(q);
        q = NULL;
        return NULL;
    }

    for (size_t i = 0; i < node_num; ++i)
    {
        threadboundedqueue_node_t *node = (threadboundedqueue_node_t *)((uint8_t *)q->nodes + i * node_total_size);
        node->is_extern = false;
        node->len = 0;
        node->p_extern = NULL;
    }

    q->node_size = node_size;
    q->capacity = node_num;
    q->head = 0;
    q->tail = 0;
    q->is_run = true;

    return q;
}

static threadboundedqueue_node_t *threadboundedqueue_get_node_ptr(threadboundedqueue_t *q, size_t idx)
{
    size_t node_total_size = sizeof(threadboundedqueue_node_t) + q->node_size;
    return (threadboundedqueue_node_t *)((uint8_t *)q->nodes + idx * node_total_size);
}

int32_t threadboundedqueue_push_block(threadboundedqueue_t *q, void *data, size_t len, uint32_t timeout)
{
    int32_t ret = THREADQUEUE_RET_SUCCESS;
    if (NULL == q || NULL == data || 0 == len)
    {
        LOG_PRINT_ERROR("invalid param!");
        return THREADQUEUE_RET_ERR_ARG;
    }

    pthread_mutex_lock(&q->mutex);

    while (q->is_run && q->count >= q->capacity) // use while to avoid false wakeup
    {
        if (timeout == 0)
        {
            ret = pthread_cond_wait(&q->cond_notfull, &q->mutex);
            if (0 != ret)
            {
                pthread_mutex_unlock(&q->mutex);
                LOG_PRINT_ERROR("pthread_cond_wait fail, ret[%d](%s)", ret, strerror(ret));
                return THREADQUEUE_RET_ERR_OTHER;
            }
        }
        else
        {
            // 1. unlock
            // 2. wait the condition is met
            // ------ provider produce signal
            // 3. wakeup
            // 4. lock
            struct timespec wait_time = util_time_mono_after(timeout);
            ret = pthread_cond_timedwait(&q->cond_notfull, &q->mutex, &wait_time);
            if (0 != ret)
            {
                pthread_mutex_unlock(&q->mutex);
                if (ETIMEDOUT == ret)
                {
                    return THREADQUEUE_RET_ERR_TIMEOUT;
                }

                LOG_PRINT_ERROR("pthread_cond_timedwait fail, ret[%d](%s)", ret, strerror(ret));
                return THREADQUEUE_RET_ERR_OTHER;
            }
        }
    }

    if (!q->is_run)
    {
        pthread_mutex_unlock(&q->mutex);
        return THREADQUEUE_RET_ERR_STOP;
    }

    threadboundedqueue_node_t *node = threadboundedqueue_get_node_ptr(q, q->head);
    if (node->is_extern && NULL != node->p_extern)
    {
        free(node->p_extern);
        node->p_extern = NULL;
        node->is_extern = false;
    }

    if (len > q->node_size)
    {
        node->p_extern = (uint8_t *)calloc(1, len);
        if (NULL == node->p_extern)
        {
            LOG_PRINT_ERROR("calloc fail, errno(%d)[%s]", errno, strerror(errno));
            pthread_mutex_unlock(&q->mutex);
            return THREADQUEUE_RET_ERR_MEM;
        }
        memcpy(node->p_extern, data, len);
        node->is_extern = true;
    }
    else
    {
        memcpy(node->data, data, len);
    }

    node->len = len;

    q->head = (q->head + 1) % q->capacity;
    q->count++;
    pthread_cond_signal(&q->cond_notempty);
    pthread_mutex_unlock(&q->mutex);

    return THREADQUEUE_RET_SUCCESS;
}

int32_t threadboundedqueue_push_nonblock(threadboundedqueue_t *q, void *data, size_t len)
{
    if (NULL == q || NULL == data || 0 == len)
    {
        LOG_PRINT_ERROR("invalid param!");
        return THREADQUEUE_RET_ERR_ARG;
    }

    pthread_mutex_lock(&q->mutex);
    if (!q->is_run)
    {
        pthread_mutex_unlock(&q->mutex);
        return THREADQUEUE_RET_ERR_STOP;
    }

    if (q->count >= q->capacity)
    {
        pthread_mutex_unlock(&q->mutex);
        return THREADQUEUE_RET_ERR_FULL;
    }

    threadboundedqueue_node_t *node = threadboundedqueue_get_node_ptr(q, q->head);
    if (node->is_extern && NULL != node->p_extern)
    {
        free(node->p_extern);
        node->p_extern = NULL;
        node->is_extern = false;
    }

    if (len > q->node_size)
    {
        node->p_extern = (uint8_t *)calloc(1, len);
        if (NULL == node->p_extern)
        {
            LOG_PRINT_ERROR("calloc fail, errno(%d)[%s]", errno, strerror(errno));
            pthread_mutex_unlock(&q->mutex);
            return THREADQUEUE_RET_ERR_MEM;
        }
        memcpy(node->p_extern, data, len);
        node->is_extern = true;
    }
    else
    {
        memcpy(node->data, data, len);
    }

    node->len = len;

    q->head = (q->head + 1) % q->capacity;
    q->count++;
    pthread_cond_signal(&q->cond_notempty);
    pthread_mutex_unlock(&q->mutex);

    return THREADQUEUE_RET_SUCCESS;
}

int32_t threadboundedqueue_pop_block(threadboundedqueue_t *q, void *data, size_t *len, uint32_t timeout)
{
    int32_t ret = THREADQUEUE_RET_SUCCESS;

    if (NULL == q || NULL == data || NULL == len)
    {
        LOG_PRINT_ERROR("invalid param!");
        return THREADQUEUE_RET_ERR_ARG;
    }

    pthread_mutex_lock(&q->mutex);
    while (q->is_run && 0 == q->count)
    {
        if (timeout == 0)
        {
            ret = pthread_cond_wait(&q->cond_notempty, &q->mutex);
            if (0 != ret)
            {
                pthread_mutex_unlock(&q->mutex);
                LOG_PRINT_ERROR("pthread_cond_wait fail, ret[%d](%s)", ret, strerror(ret));
                return THREADQUEUE_RET_ERR_OTHER;
            }
        }
        else
        {
            struct timespec wait_time = util_time_mono_after(timeout);
            ret = pthread_cond_timedwait(&q->cond_notempty, &q->mutex, &wait_time);
            if (0 != ret)
            {
                pthread_mutex_unlock(&q->mutex);
                if (ETIMEDOUT == ret)
                {
                    return THREADQUEUE_RET_ERR_TIMEOUT;
                }

                LOG_PRINT_ERROR("pthread_cond_timedwait fail, ret[%d](%s)", ret, strerror(ret));
                return THREADQUEUE_RET_ERR_OTHER;
            }
        }
    }

    if (!q->is_run)
    {
        pthread_mutex_unlock(&q->mutex);
        return THREADQUEUE_RET_ERR_STOP;
    }

    threadboundedqueue_node_t *node = threadboundedqueue_get_node_ptr(q, q->tail);
    if (node->len > *len)
    {
        pthread_mutex_unlock(&q->mutex);
        return THREADQUEUE_RET_ERR_LEN;
    }

    if (node->is_extern && NULL != node->p_extern)
    {
        memcpy(data, node->p_extern, node->len);
        free(node->p_extern);
        node->p_extern = NULL;
        node->is_extern = false;
    }
    else
    {
        memcpy(data, node->data, node->len);
    }

    *len = node->len;
    node->len = 0;

    q->tail = (q->tail + 1) % q->capacity;
    q->count--;
    pthread_cond_signal(&q->cond_notfull);
    pthread_mutex_unlock(&q->mutex);

    return THREADQUEUE_RET_SUCCESS;
}

int32_t threadboundedqueue_pop_nonblock(threadboundedqueue_t *q, void *data, size_t *len)
{
    if (NULL == q || NULL == data || NULL == len)
    {
        LOG_PRINT_ERROR("invalid param!");
        return THREADQUEUE_RET_ERR_ARG;
    }

    pthread_mutex_lock(&q->mutex);
    if (!q->is_run)
    {
        pthread_mutex_unlock(&q->mutex);
        return THREADQUEUE_RET_ERR_STOP;
    }

    if (0 == q->count)
    {
        pthread_mutex_unlock(&q->mutex);
        return THREADQUEUE_RET_ERR_NO_MSG;
    }

    threadboundedqueue_node_t *node = threadboundedqueue_get_node_ptr(q, q->tail);
    if (node->len > *len)
    {
        pthread_mutex_unlock(&q->mutex);
        return THREADQUEUE_RET_ERR_LEN;
    }

    if (node->is_extern && NULL != node->p_extern)
    {
        memcpy(data, node->p_extern, node->len);
        free(node->p_extern);
        node->p_extern = NULL;
        node->is_extern = false;
    }
    else
    {
        memcpy(data, node->data, node->len);
    }

    *len = node->len;
    node->len = 0;

    q->tail = (q->tail + 1) % q->capacity;
    q->count--;
    pthread_cond_signal(&q->cond_notfull);
    pthread_mutex_unlock(&q->mutex);

    return THREADQUEUE_RET_SUCCESS;
}

bool threadboundedqueue_is_full(threadboundedqueue_t *q)
{
    bool ret = false;
    pthread_mutex_lock(&q->mutex);
    ret = (q->count >= q->capacity) ? true : false;
    pthread_mutex_unlock(&q->mutex);

    return ret;
}

bool threadboundedqueue_is_empty(threadboundedqueue_t *q)
{
    bool ret = false;
    pthread_mutex_lock(&q->mutex);
    ret = (q->count == 0) ? true : false;
    pthread_mutex_unlock(&q->mutex);

    return ret;
}

bool threadboundedqueue_stop(threadboundedqueue_t *q)
{
    pthread_mutex_lock(&q->mutex);
    if (!q->is_run)
    {
        pthread_mutex_unlock(&q->mutex);
        return false;
    }
    q->is_run = false;
    pthread_cond_broadcast(&q->cond_notempty);
    pthread_cond_broadcast(&q->cond_notfull);
    pthread_mutex_unlock(&q->mutex);

    return true;
}

void threadunboundedqueue_uninit(threadunboundedqueue_t **q)
{
    if (NULL == q || NULL == *q)
    {
        LOG_PRINT_ERROR("invalid param!");
        return;
    }

    threadunboundedqueue_node_t *node = TAILQ_FIRST(&(*q)->queue);
    while (!TAILQ_EMPTY(&(*q)->queue))
    {
        node = TAILQ_FIRST(&(*q)->queue);
        node->len = 0;
        TAILQ_REMOVE(&(*q)->queue, node, entries);
        free(node);
        node = NULL;
    }

    pthread_mutex_destroy(&(*q)->mutex);
    pthread_cond_destroy(&(*q)->cond_notempty);
    free(*q);
    *q = NULL;
}

threadunboundedqueue_t *threadunboundedqueue_create(void)
{
    threadunboundedqueue_t *q = NULL;

    q = (threadunboundedqueue_t *)calloc(1, sizeof(threadunboundedqueue_t));
    if (NULL == q)
    {
        LOG_PRINT_ERROR("calloc fail, errno(%d)[%s]", errno, strerror(errno));
        return NULL;
    }

    pthread_mutexattr_t mutexattr = {};
    pthread_mutexattr_init(&mutexattr);
    (void)pthread_mutexattr_settype(&mutexattr, PTHREAD_MUTEX_ERRORCHECK);
    while ((pthread_mutex_init(&q->mutex, &mutexattr) != 0) &&
           (pthread_mutex_init(&q->mutex, NULL) != 0))
    {
        // We must have memory exhaustion -- ENOMEM, or
        // in some cases EAGAIN.  Wait a bit before we try to
        // give things a chance to settle down.
        util_msleep(10);
    }
    pthread_mutexattr_destroy(&mutexattr);

    pthread_condattr_t condattr = {};
    pthread_condattr_init(&condattr);
    pthread_condattr_setclock(&condattr, CLOCK_MONOTONIC);
    while (pthread_cond_init(&q->cond_notempty, &condattr) != 0)
    {
        util_msleep(10);
    }
    pthread_condattr_destroy(&condattr);

    TAILQ_INIT(&q->queue);
    q->size = 0;
    q->is_run = true;

    return q;
}

bool threadunboundedqueue_is_empty(threadunboundedqueue_t *q)
{
    bool ret = false;
    pthread_mutex_lock(&q->mutex);
    ret = (q->size == 0) ? true : false;
    pthread_mutex_unlock(&q->mutex);

    return ret;
}

bool threadunboundedqueue_stop(threadunboundedqueue_t *q)
{
    pthread_mutex_lock(&q->mutex);
    if (!q->is_run)
    {
        pthread_mutex_unlock(&q->mutex);
        return false;
    }
    q->is_run = false;
    pthread_cond_broadcast(&q->cond_notempty);
    pthread_mutex_unlock(&q->mutex);

    return true;
}

int32_t threadunboundedqueue_push(threadunboundedqueue_t *q, void *data, size_t len)
{
    if (NULL == q || NULL == data || 0 == len)
    {
        LOG_PRINT_ERROR("invalid param!");
        return THREADQUEUE_RET_ERR_ARG;
    }

    threadunboundedqueue_node_t *node = (threadunboundedqueue_node_t *)calloc(1, sizeof(threadunboundedqueue_node_t) + len);
    if (NULL == node)
    {
        LOG_PRINT_ERROR("calloc fail, errno(%d)[%s]", errno, strerror(errno));
        return THREADQUEUE_RET_ERR_MEM;
    }
    if (len > 0)
    {
        memcpy(node->data, data, len);
    }
    node->len = len;

    pthread_mutex_lock(&q->mutex);
    if (!q->is_run)
    {
        pthread_mutex_unlock(&q->mutex);
        return THREADQUEUE_RET_ERR_STOP;
    }

    TAILQ_INSERT_TAIL(&q->queue, node, entries);
    q->size++;

    pthread_cond_signal(&q->cond_notempty);
    pthread_mutex_unlock(&q->mutex);

    return THREADQUEUE_RET_SUCCESS;
}

int32_t threadunboundedqueue_pop_block(threadunboundedqueue_t *q, void *data, size_t *len, uint32_t timeout)
{
    int32_t ret = THREADQUEUE_RET_SUCCESS;

    if (NULL == q || NULL == data || NULL == len)
    {
        LOG_PRINT_ERROR("invalid param!");
        return THREADQUEUE_RET_ERR_ARG;
    }

    pthread_mutex_lock(&q->mutex);
    while (q->is_run && 0 == q->size)
    {
        if (timeout == 0)
        {
            ret = pthread_cond_wait(&q->cond_notempty, &q->mutex);
            if (0 != ret)
            {
                pthread_mutex_unlock(&q->mutex);
                LOG_PRINT_ERROR("pthread_cond_wait fail, ret[%d](%s)", ret, strerror(ret));
                return THREADQUEUE_RET_ERR_OTHER;
            }
        }
        else
        {
            struct timespec wait_time = util_time_mono_after(timeout);
            ret = pthread_cond_timedwait(&q->cond_notempty, &q->mutex, &wait_time);
            if (0 != ret)
            {
                pthread_mutex_unlock(&q->mutex);
                if (ETIMEDOUT == ret)
                {
                    return THREADQUEUE_RET_ERR_TIMEOUT;
                }

                LOG_PRINT_ERROR("pthread_cond_timedwait fail, ret[%d](%s)", ret, strerror(ret));
                return THREADQUEUE_RET_ERR_OTHER;
            }
        }
    }

    if (!q->is_run)
    {
        pthread_mutex_unlock(&q->mutex);
        return THREADQUEUE_RET_ERR_STOP;
    }

    threadunboundedqueue_node_t *node = TAILQ_FIRST(&q->queue);
    if (node->len > *len)
    {
        pthread_mutex_unlock(&q->mutex);
        return THREADQUEUE_RET_ERR_LEN;
    }

    memcpy(data, node->data, node->len);
    *len = node->len;

    TAILQ_REMOVE(&q->queue, node, entries);
    q->size--;

    pthread_mutex_unlock(&q->mutex);

    free(node);
    node = NULL;

    return THREADQUEUE_RET_SUCCESS;
}

int32_t threadunboundedqueue_pop_nonblock(threadunboundedqueue_t *q, void *data, size_t *len)
{
    if (NULL == q || NULL == data || NULL == len)
    {
        LOG_PRINT_ERROR("invalid param!");
        return THREADQUEUE_RET_ERR_ARG;
    }

    pthread_mutex_lock(&q->mutex);
    if (!q->is_run)
    {
        pthread_mutex_unlock(&q->mutex);
        return THREADQUEUE_RET_ERR_STOP;
    }

    if (0 == q->size)
    {
        pthread_mutex_unlock(&q->mutex);
        return THREADQUEUE_RET_ERR_NO_MSG;
    }

    threadunboundedqueue_node_t *node = TAILQ_FIRST(&q->queue);
    if (node->len > *len)
    {
        pthread_mutex_unlock(&q->mutex);
        return THREADQUEUE_RET_ERR_LEN;
    }

    memcpy(data, node->data, node->len);
    *len = node->len;

    TAILQ_REMOVE(&q->queue, node, entries);
    q->size--;

    pthread_mutex_unlock(&q->mutex);

    free(node);
    node = NULL;

    return THREADQUEUE_RET_SUCCESS;
}

const char *threadqueue_strerror(int err)
{
    switch (err)
    {
#define X(code, name, msg)       \
    case THREADQUEUE_RET_##name: \
        return msg;
        THREADQUEUE_FOREACH_ERR(X)
#undef X
        default:
            return "Unknown thread queue error";
    }
}