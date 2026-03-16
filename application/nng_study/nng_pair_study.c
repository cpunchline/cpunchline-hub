#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "nng/nng.h"

#define NODE0 "node0" // listen
#define NODE1 "node1" // dial

typedef struct
{
    nng_socket sock;
    char *name;
} module_t;

// 一对一双向通信, 类似对讲机
// pair left端: ./build/simulator/nng_test/nng_pair_test_app node0 ipc:///tmp/ipc_pair_test
// pair right端: ./build/simulator/nng_test/nng_pair_test_app node1 ipc:///tmp/ipc_pair_test

static void fatal(const char *func, int rv)
{
    fprintf(stderr, "%s: %s\n", func, nng_strerror((nng_err)rv));
    exit(1);
}

static int send_msg(char *sender, nng_socket sock, char *send_buffer)
{
    int rv;

    printf("%s: SENDING [%s]\n", sender, send_buffer);
    if ((rv = nng_send(sock, send_buffer, strlen(send_buffer), 0)) != 0)
    {
        fatal("nng_send", rv);
    }

    return (rv);
}

static int recv_msg(char *recver, nng_socket sock, char *recv_buffer)
{
    int rv;
    size_t sz;

    if ((rv = nng_recv(sock, &recv_buffer, &sz, 0)) == 0)
    {
        printf("%s: RECEIVED [%s]\n", recver, recv_buffer);
        nng_free(recv_buffer, sz);
    }

    return (rv);
}

static void *recv_thread_func(void *arg)
{
    module_t *module = (module_t *)arg;
    char *recv_buffer = NULL;
    while (1)
    {
        recv_msg(module->name, module->sock, recv_buffer);
    }

    return NULL;
}

static int node0(const char *url)
{
    int rv;
    nng_socket sock;
    pthread_t thread;
    char send_buffer[1024] = {0};

    if ((rv = nng_pair0_open(&sock)) != 0)
    {
        fatal("nng_pair0_open", rv);
    }

    if ((rv = nng_listen(sock, url, NULL, 0)) != 0)
    {
        fatal("nng_listen", rv);
    }

    // nng_socket_set_ms(sock, NNG_OPT_RECVTIMEO, 10000); // 接收超时时间 SO_RCVTIMEO
    // nng_socket_set_ms(sock, NNG_OPT_SENDTIMEO, 10000); // 发送超时时间 SO_SNDTIMEO
    nng_socket_set_ms(sock, NNG_OPT_RECVBUF, 1024); // 接收缓冲区 SO_RCVBUF
    nng_socket_set_ms(sock, NNG_OPT_SENDBUF, 1024); // 发送缓冲区 SO_SNDBUF

    module_t node0 = {sock, NODE0};
    rv = pthread_create(&thread, NULL, recv_thread_func, (void *)&node0);
    if (rv != 0)
    {
        printf("pthread_create error, [%s]\n", strerror(rv));
        return -1;
    }
    pthread_detach(thread);

    while (1)
    {
        time_t current_time = time(NULL);
        strftime(send_buffer, sizeof(send_buffer), "node0 send: %Y-%m-%d %H:%M:%S", localtime(&current_time));
        send_msg(NODE0, sock, send_buffer);
        sleep(1);
    }
}

static int node1(const char *url)
{
    nng_socket sock;
    int rv;
    pthread_t thread;
    char send_buffer[1024] = {0};

    if ((rv = nng_pair0_open(&sock)) != 0)
    {
        fatal("nng_pair0_open", rv);
    }

    if ((rv = nng_dial(sock, url, NULL, NNG_FLAG_NONBLOCK)) != 0)
    {
        fatal("nng_dial", rv);
    }

    nng_socket_set_ms(sock, NNG_OPT_RECVBUF, 1024); // 接收缓冲区 SO_RCVBUF
    nng_socket_set_ms(sock, NNG_OPT_SENDBUF, 1024); // 发送缓冲区 SO_SNDBUF
    nng_socket_set_ms(sock, NNG_OPT_RECONNMINT, 100);

    module_t node1 = {sock, NODE1};
    rv = pthread_create(&thread, NULL, recv_thread_func, &node1);
    if (rv != 0)
    {
        printf("pthread_create error, [%s]\n", strerror(rv));
        return -1;
    }
    pthread_detach(thread);

    while (1)
    {
        time_t current_time = time(NULL);
        strftime(send_buffer, sizeof(send_buffer), "node1 send: %Y-%m-%d %H:%M:%S", localtime(&current_time));
        send_msg(NODE1, sock, send_buffer);
        sleep(1);
    }
}

int main(int argc, char **argv)
{
    if ((argc > 1) && (strcmp(NODE0, argv[1]) == 0))
        return (node0(argv[2]));

    if ((argc > 1) && (strcmp(NODE1, argv[1]) == 0))
        return (node1(argv[2]));

    fprintf(stderr, "Usage: pair %s|%s <URL> <ARG> ...\n", NODE0, NODE1);
    return 1;
}
