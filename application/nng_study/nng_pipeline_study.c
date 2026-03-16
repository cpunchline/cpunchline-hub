#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "nng/nng.h"

// 单向通信, 类似与生产者消费者模型的消息队列, 消息从推方流向拉方.
// push端: ./build/simulator/nng_test/nng_pipeline_test_app node1 ipc:///tmp/ipc_pipeline_test hello
// pull端: ./build/simulator/nng_test/nng_pipeline_test_app node0 ipc:///tmp/ipc_pipeline_test

#define NODE0 "node0"
#define NODE1 "node1"

static void fatal(const char *func, int rv)
{
    fprintf(stderr, "%s: %s\n", func, nng_strerror((nng_err)rv));
    exit(1);
}

static int node0(const char *url)
{
    nng_socket sock;
    int rv;

    if ((rv = nng_pull0_open(&sock)) != 0)
    {
        fatal("nng_pull0_open", rv);
    }
    if ((rv = nng_listen(sock, url, NULL, 0)) != 0)
    {
        fatal("nng_listen", rv);
    }
    for (;;)
    {
        char *buf = NULL;
        size_t sz;
        if ((rv = nng_recv(sock, &buf, &sz, 0)) != 0)
        {
            fatal("nng_recv", rv);
        }
        printf("NODE0: RECEIVED \"%s\"\n", buf);
        nng_free(buf, sz);
    }

    return 0;
}

static int node1(const char *url, char *msg)
{
    nng_socket sock;
    int rv;
    if ((rv = nng_push0_open(&sock)) != 0)
    {
        fatal("nng_push0_open", rv);
    }
    if ((rv = nng_dial(sock, url, NULL, 0)) != 0)
    {
        fatal("nng_dial", rv);
    }
    printf("NODE1: SENDING \"%s\"\n", msg);
    if ((rv = nng_send(sock, msg, strlen(msg) + 1, 0)) != 0)
    {
        fatal("nng_send", rv);
    }
    sleep(1); // wait for messages to flush before shutting down
    nng_socket_close(sock);
    return (0);
}

int main(int argc, char **argv)
{
    if ((argc > 1) && (strcmp(NODE0, argv[1]) == 0))
        return (node0(argv[2]));

    if ((argc > 2) && (strcmp(NODE1, argv[1]) == 0))
        return (node1(argv[2], argv[3]));

    fprintf(stderr, "Usage: pipeline %s|%s <URL> <ARG> ...'\n",
            NODE0, NODE1);
    return (1);
}
