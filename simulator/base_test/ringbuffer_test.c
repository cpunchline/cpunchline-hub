#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>

#include "dsa/ringbuffer.h"

#define FIFO_SIZE 16

struct ringbuffer_t ring_buf = {};
static uint8_t g_buffer[FIFO_SIZE] = {};

static void *consumer_proc(void *arg)
{
    unsigned int cnt;
    struct ringbuffer_t *p_ring_buf = (struct ringbuffer_t *)arg;

    cnt = 0;

    while (1)
    {
        sleep(2);
        printf("------------------------------------------\n");
        printf("get data from ring buffer.\n");

        {
            uint8_t i;

            if (ringbuffer_is_empty(p_ring_buf))
            {
                printf("buffer is empty !\n");
                sleep(1);
                continue;
            }

            if (cnt != 0 && !(cnt % 16))
                printf("\n");

            ringbuffer_out(p_ring_buf, &i, sizeof(i));

            printf("data is: %d \n", i);

            cnt++;
        }

        printf("ring buffer length: %u\n", ringbuffer_len(p_ring_buf));
        printf("------------------------------------------\n");
    }

    return NULL;
}

static void *producer_proc(void *arg)
{
    struct ringbuffer_t *p_ring_buf = (struct ringbuffer_t *)arg;
    uint8_t i;

    i = 0;
    while (1)
    {
        printf("******************************************\n");
        printf("put datas to ring buffer.\n");

        if (ringbuffer_is_full(p_ring_buf))
        {
            printf("buffer is full !\n");
            sleep(1);
            continue;
        }
        ringbuffer_in(p_ring_buf, &i, sizeof(i));
        i++;

        printf("ring buffer length: %u\n", ringbuffer_len(p_ring_buf));
        printf("******************************************\n");
        sleep(1);
    }

    return NULL;
}

static pthread_t consumer_thread(void *arg)
{
    int err;
    pthread_t tid;
    err = pthread_create(&tid, NULL, consumer_proc, arg);
    if (err != 0)
    {
        fprintf(stderr, "Failed to create consumer thread.errno:%u, reason:%s\n",
                errno, strerror(errno));
        exit(-1);
    }
    return tid;
}

static pthread_t producer_thread(void *arg)
{
    int err;
    pthread_t tid;
    err = pthread_create(&tid, NULL, producer_proc, arg);
    if (err != 0)
    {
        fprintf(stderr, "Failed to create consumer thread.errno:%u, reason:%s\n",
                errno, strerror(errno));
        exit(-1);
    }

    return tid;
}

int main(void)
{
    pthread_t produce_pid, consume_pid;

    ringbuffer_init(&ring_buf, sizeof(uint8_t), g_buffer, FIFO_SIZE);

    produce_pid = producer_thread((void *)&ring_buf);
    consume_pid = consumer_thread((void *)&ring_buf);

    for (;;)
    {
        pause();
    }

    pthread_join(produce_pid, NULL);
    pthread_join(consume_pid, NULL);

    return 0;
}