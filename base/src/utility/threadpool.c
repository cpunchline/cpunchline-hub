#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <pthread.h>
#include <stdatomic.h>
#include <inttypes.h>

#include "utility/utils.h"
#include "utility/threadqueue.h"
#include "utility/threadpool.h"

struct threaddata_t
{
    pthread_t thread_id;
    struct timespec start_time;
    struct timespec stop_time;
    void *embedded_pool;
};
typedef struct threaddata_t threaddata_t;

struct fix_threadpool_t
{
    threaddata_t *threads;
    threadboundedqueue_t *tasks;
    size_t thread_capacity;
    atomic_bool is_run;
};

int32_t fix_threadpool_destroy(fix_threadpool_t *pool)
{
    if (NULL == pool)
    {
        LOG_PRINT_ERROR("invalid param!");
        return THREADPOOL_RET_ERR_ARG;
    }

    if (!atomic_load(&pool->is_run))
    {
        LOG_PRINT_ERROR("already destroy!");
        return THREADPOOL_RET_ERR_AGAIN;
    }

    threadboundedqueue_stop(pool->tasks);
    atomic_store(&pool->is_run, false);

    for (size_t i = 0; i < pool->thread_capacity; ++i)
    {
        if (0 != pthread_join(pool->threads[i].thread_id, NULL))
        {
            LOG_PRINT_ERROR("pthread_join fail, errno[%d](%s)", errno, strerror(errno));
        }
        struct timespec now = util_time_mono();
        LOG_PRINT_INFO("thread[%" PRIu64 "]-join[%" PRId64 "-%" PRId64 "]", (uint64_t)pool->threads[i].thread_id,
                       (int64_t)now.tv_sec, (int64_t)now.tv_nsec);
    }
    threadboundedqueue_uninit(&pool->tasks);
    free(pool->threads);
    free(pool);

    return THREADPOOL_RET_SUCCESS;
}

static void *fix_threadworker(void *args)
{
    int32_t ret = 0;
    threaddata_t *thread_data = (threaddata_t *)args;

    thread_data->start_time = util_time_mono();
    LOG_PRINT_INFO("thread[%ld]-start[%ld-%ld]", thread_data->thread_id,
                   thread_data->start_time.tv_sec, thread_data->start_time.tv_nsec);

    fix_threadpool_t *pool = (fix_threadpool_t *)thread_data->embedded_pool;
    while (atomic_load(&pool->is_run))
    {
        threadtask_t task = {};
        size_t task_size = sizeof(task);
        ret = threadboundedqueue_pop_block(pool->tasks, &task, &task_size, 0);
        if (THREADQUEUE_RET_SUCCESS == ret)
        {
            task.function(task.arg);
        }
        else if (THREADQUEUE_RET_ERR_STOP == ret)
        {
            break;
        }
        else
        {
            LOG_PRINT_ERROR("threadboundedqueue_pop_block fail, ret[%d]", ret);
            sleep(1); // please repair the bug
            continue;
        }
    }

    thread_data->stop_time = util_time_mono();
    LOG_PRINT_INFO("thread[%" PRIu64 "]-stop[%" PRId64 "-%" PRId64 "]", (uint64_t)thread_data->thread_id,
                   (int64_t)thread_data->stop_time.tv_sec, (int64_t)thread_data->stop_time.tv_nsec);

    return NULL;
}

fix_threadpool_t *fix_threadpool_create(size_t thread_capacity, size_t queue_size)
{
    int32_t ret = 0;
    fix_threadpool_t *pool = NULL;
    size_t created_success_thread = 0;

    pool = (fix_threadpool_t *)calloc(1, sizeof(fix_threadpool_t));
    if (NULL == pool)
    {
        LOG_PRINT_ERROR("calloc fail, errno[%d](%s)!", errno, strerror(errno));
        return NULL;
    }

    pool->threads = (threaddata_t *)calloc(thread_capacity, sizeof(threaddata_t));
    if (NULL == pool->threads)
    {
        LOG_PRINT_ERROR("calloc fail, errno[%d](%s)!", errno, strerror(errno));
        free(pool);
        return NULL;
    }

    pool->tasks = threadboundedqueue_create(queue_size, sizeof(threadtask_t));
    if (NULL == pool->tasks)
    {
        LOG_PRINT_ERROR("threadboundedqueue_create fail, errno[%d](%s)!", errno, strerror(errno));
        free(pool->threads);
        free(pool);
        return NULL;
    }

    pool->thread_capacity = thread_capacity;
    atomic_store(&pool->is_run, true);

    for (size_t i = 0; i < thread_capacity; ++i)
    {
        pool->threads[i].embedded_pool = pool;
        ret = pthread_create(&pool->threads[i].thread_id, NULL, fix_threadworker, &pool->threads[i]);
        if (0 != ret)
        {
            LOG_PRINT_ERROR("pthread_create fail, ret[%d] errno[%d](%s)", ret, errno, strerror(errno));
            threadboundedqueue_uninit(&pool->tasks);
            atomic_store(&pool->is_run, false);

            for (size_t j = 0; j < created_success_thread; ++j)
            {
                if (0 != pthread_join(pool->threads[j].thread_id, NULL))
                {
                    LOG_PRINT_ERROR("pthread_join fail, errno[%d](%s)", errno, strerror(errno));
                }
            }

            free(pool->threads);
            free(pool);

            return NULL;
        }
        char thread_name[16] = {};
        snprintf(thread_name, sizeof(thread_name), "tp_%u", (uint32_t)i);
        pthread_setname_np(pool->threads[i].thread_id, thread_name);
        ++created_success_thread;
    }

    return pool;
}

int32_t fix_threadpool_addtask(fix_threadpool_t *pool, void (*function)(void *), void *arg)
{
    if (!pool || !function)
    {
        LOG_PRINT_ERROR("invalid param!");
        return THREADPOOL_RET_ERR_ARG;
    }

    if (!atomic_load(&pool->is_run))
    {
        LOG_PRINT_ERROR("already destroy!");
        return THREADPOOL_RET_ERR_AGAIN;
    }

    int32_t ret = 0;
    threadtask_t task = {};
    task.function = function;
    task.arg = arg;

    ret = threadboundedqueue_push_nonblock(pool->tasks, &task, sizeof(task));
    if (THREADQUEUE_RET_SUCCESS != ret)
    {
        LOG_PRINT_ERROR("threadboundedqueue_push_nonblock fail, ret[%d]", ret);
        if (THREADQUEUE_RET_ERR_FULL == ret)
        {
            ret = THREADPOOL_RET_ERR_QUEUE_FULL;
        }
        else
        {
            ret = THREADPOOL_RET_ERR_OTHER;
        }
    }
    else
    {
        ret = THREADPOOL_RET_SUCCESS;
    }

    return ret;
}

int32_t fix_threadpool_addtasks(fix_threadpool_t *pool, threadtask_t *tasks, size_t tasks_size)
{
    if (!pool || !tasks || 0 == tasks_size)
    {
        LOG_PRINT_ERROR("invalid param!");
        return THREADPOOL_RET_ERR_ARG;
    }

    if (!atomic_load(&pool->is_run))
    {
        LOG_PRINT_ERROR("already destroy!");
        return THREADPOOL_RET_ERR_AGAIN;
    }

    int32_t ret = 0;
    for (size_t i = 0; i < tasks_size; ++i)
    {
        ret = threadboundedqueue_push_nonblock(pool->tasks, &tasks[i], sizeof(tasks[i]));
        if (THREADQUEUE_RET_SUCCESS != ret)
        {
            LOG_PRINT_ERROR("threadboundedqueue_push_nonblock fail, ret[%d]", ret);
            if (THREADQUEUE_RET_ERR_FULL == ret)
            {
                ret = THREADPOOL_RET_ERR_QUEUE_FULL;
                break;
            }
            else
            {
                ret = THREADPOOL_RET_ERR_OTHER;
                break;
            }
        }
        else
        {
            ret = THREADPOOL_RET_SUCCESS;
        }
    }

    return ret;
}

const char *fix_threadpool_strerror(int err)
{
    switch (err)
    {
#define X(code, name, msg)      \
    case THREADPOOL_RET_##name: \
        return msg;
        THREADPOOL_FOREACH_ERR(X)
#undef X
        default:
            return "Unknown thread queue error";
    }
}