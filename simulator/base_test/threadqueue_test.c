#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include "utility/threadqueue.h"

threadboundedqueue_t *p = NULL;
threadunboundedqueue_t *q = NULL;

static void *producer_p(void *arg)
{
    (void)arg;
    int32_t ret = THREADQUEUE_RET_SUCCESS;
    size_t data = 0;
    while (1)
    {
        data++;
        ret = threadboundedqueue_push_block(p, &data, sizeof(data), 0);
        if (THREADQUEUE_RET_SUCCESS != ret)
        {
            printf("threadboundedqueue_push_block fail, ret[%d]\n", ret);
            exit(1);
        }
        printf("threadboundedqueue_push_block success, data[%zu]\n", data);
        usleep(100 * 1000);
    }
}

static void *consumer_p(void *arg)
{
    (void)arg;
    int32_t ret = THREADQUEUE_RET_SUCCESS;
    size_t data = 0;

    while (1)
    {
        size_t data_len = sizeof(data);
        ret = threadboundedqueue_pop_block(p, &data, &data_len, 1000);
        if (THREADQUEUE_RET_SUCCESS != ret)
        {
            printf("threadboundedqueue_pop_block fail, ret[%d]\n", ret);
            continue;
        }

        printf("threadboundedqueue_pop_block success, data[%zu], data_len[%zu]\n", data, data_len);
    }

    return NULL;
}

static void *producer_q(void *arg)
{
    (void)arg;
    int32_t ret = THREADQUEUE_RET_SUCCESS;
    size_t data = 0;
    while (1)
    {
        data++;
        ret = threadunboundedqueue_push(q, &data, sizeof(size_t));
        if (THREADQUEUE_RET_SUCCESS != ret)
        {
            printf("threadunboundedqueue_push_node fail, ret[%d]\n", ret);
            exit(1);
        }
        printf("threadunboundedqueue_push_node success, data[%zu]\n", data);
        usleep(100 * 1000);
    }

    return NULL;
}

static void *consumer_q(void *arg)
{
    (void)arg;
    int32_t ret = THREADQUEUE_RET_SUCCESS;
    size_t data = 0;
    size_t data_len = sizeof(data);
    while (1)
    {
        ret = threadunboundedqueue_pop_block(q, &data, &data_len, 1000);
        if (THREADQUEUE_RET_SUCCESS != ret)
        {
            printf("threadunboundedqueue_pop_node_block fail, ret[%d]\n", ret);
            continue;
        }

        printf("threadunboundedqueue_pop_node_block success, data[%zu], data_len[%zu]\n", data, data_len);
    }

    return NULL;
}

int main(void)
{
    srand((unsigned)time(NULL));

    int ret = 0;

    p = threadboundedqueue_create(5, sizeof(size_t));
    if (NULL == p)
    {
        return -1;
    }

    pthread_t producer_p_tid;
    ret = pthread_create(&producer_p_tid, NULL, producer_p, NULL);
    if (0 != ret)
    {
        printf("pthread_create producer fail, ret[%d]\n", ret);
        return -1;
    }

    pthread_t consumer_p_tid;
    ret = pthread_create(&consumer_p_tid, NULL, consumer_p, NULL);
    if (0 != ret)
    {
        printf("pthread_create consumer fail, ret[%d]\n", ret);
        return -1;
    }

    q = threadunboundedqueue_create();
    if (NULL == q)
    {
        return -1;
    }

    pthread_t producer_q_tid;
    ret = pthread_create(&producer_q_tid, NULL, producer_q, NULL);
    if (0 != ret)
    {
        printf("pthread_create producer fail, ret[%d]\n", ret);
        return -1;
    }

    pthread_t consumer_q_tid;
    ret = pthread_create(&consumer_q_tid, NULL, consumer_q, NULL);
    if (0 != ret)
    {
        printf("pthread_create consumer fail, ret[%d]\n", ret);
        return -1;
    }

    pthread_join(producer_p_tid, NULL);
    pthread_join(consumer_p_tid, NULL);
    pthread_join(producer_q_tid, NULL);
    pthread_join(consumer_q_tid, NULL);

    return 0;
}
