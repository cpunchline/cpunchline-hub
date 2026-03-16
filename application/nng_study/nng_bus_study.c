#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "nng/nng.h"

// 网状连接通信, 每个加入节点都可以发送/接受广播消息
// note0: ./build/simulator/nng_test/nng_bus_test_app node0 ipc:///tmp/ipc_bus_node0 ipc:///tmp/ipc_bus_node1 ipc:///tmp/ipc_bus_node2
// note1: ./build/simulator/nng_test/nng_bus_test_app note1 ipc:///tmp/ipc_bus_node1
// note2: ./build/simulator/nng_test/nng_bus_test_app note2 ipc:///tmp/ipc_bus_node2

static void fatal(const char *func, int rv)
{
    fprintf(stderr, "%s: %s\n", func, nng_strerror((nng_err)rv));
    exit(1);
}

static int node(int argc, char **argv)
{
    nng_socket sock;
    int rv;
    size_t sz;

    if ((rv = nng_bus0_open(&sock)) != 0)
    {
        fatal("nng_bus0_open", rv);
    }
    if ((rv = nng_listen(sock, argv[2], NULL, 0)) != 0)
    {
        fatal("nng_listen", rv);
    }

    sleep(1); // wait for peers to bind
    if (argc >= 3)
    {
        for (int x = 3; x < argc; x++)
        {
            if ((rv = nng_dial(sock, argv[x], NULL, 0)) != 0)
            {
                fatal("nng_dial", rv);
            }
        }
    }

    sleep(1); // wait for connects to establish

    // SEND
    sz = strlen(argv[1]) + 1; // '\0' too
    printf("%s: SENDING '%s' ONTO BUS\n", argv[1], argv[1]);
    if ((rv = nng_send(sock, argv[1], sz, 0)) != 0)
    {
        fatal("nng_send", rv);
    }

    // RECV
    for (;;)
    {
        char *buf = NULL;
        size_t buf_size;
        if ((rv = nng_recv(sock, &buf, &buf_size, 0)) != 0)
        {
            if (rv == NNG_ETIMEDOUT)
            {
                fatal("nng_recv", rv);
            }
        }
        printf("%s: RECEIVED '%s' FROM BUS\n", argv[1], buf);
        nng_free(buf, buf_size);
    }
    nng_socket_close(sock);
    return (0);
}

int main(int argc, char **argv)
{
    if (argc >= 3)
    {
        return (node(argc, argv));
    }
    fprintf(stderr, "Usage: bus <NODE_NAME> <URL> <URL> ...\n");
    return 1;
}
