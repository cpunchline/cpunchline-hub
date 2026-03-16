#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "nng/nng.h"

#define SERVER "server"
#define CLIENT "client"

// 单向广播
// pub端: ./build/simulator/nng_test/nng_pubsub_test_app server ipc:///tmp/ipc_pubsub_test
// sub端: ./build/simulator/nng_test/nng_pubsub_test_app client ipc:///tmp/ipc_pubsub_test client

static void fatal(const char *func, int rv)
{
    fprintf(stderr, "%s: %s\n", func, nng_strerror((nng_err)rv));
}

static char *date(void)
{
    time_t now = time(&now);
    struct tm *info = localtime(&now);
    char *text = asctime(info);
    text[0] = 'A';
    text[strlen(text) - 1] = '\0'; // remove '\n'
    return (text);
}

static char *date2(void)
{
    time_t now = time(&now);
    struct tm *info = localtime(&now);
    char *text = asctime(info);
    text[strlen(text) - 1] = '\0'; // remove '\n'
    return (text);
}

static int server(const char *url)
{
    nng_socket sock;
    int rv;

    if ((rv = nng_pub0_open(&sock)) != 0)
    {
        fatal("nng_pub0_open", rv);
    }
    if ((rv = nng_listen(sock, url, NULL, 0)) < 0)
    {
        fatal("nng_listen", rv);
    }

    int i = 0;
    for (;;)
    {
        char *d = date();
        printf("SERVER: PUBLISHING DATE %s\n", d);
        if ((rv = nng_send(sock, d, strlen(d) + 1, 0)) != 0)
        {
            fatal("nng_send", rv);
        }
        i++;
        if (i > 20)
        {
            break;
        }
        sleep(1);
    }

    for (;;)
    {
        char *d = date2();
        printf("SERVER: PUBLISHING DATE %s\n", d);
        if ((rv = nng_send(sock, d, strlen(d) + 1, 0)) != 0)
        {
            fatal("nng_send", rv);
        }
        sleep(1);
    }

    return (0);
}

static int client(const char *url, const char *name)
{
    nng_socket sock;
    int rv;

    if ((rv = nng_sub0_open(&sock)) != 0)
    {
        fatal("nng_sub0_open", rv);
    }

    // subscribe to everything (empty means all topics)
    if ((rv = nng_sub0_socket_subscribe(sock, "A", 1)) != 0)
    {
        fatal("nng_setopt", rv);
    }
    if ((rv = nng_dial(sock, url, NULL, 0)) != 0)
    {
        fatal("nng_dial", rv);
    }
    for (;;)
    {
        char *buf = NULL;
        size_t sz;
        if ((rv = nng_recv(sock, &buf, &sz, 0)) != 0)
        {
            fatal("nng_recv", rv);
        }
        printf("CLIENT (%s): RECEIVED %s\n", name, buf);
        nng_free(buf, sz);
    }

    return (0);
}

int main(const int argc, const char **argv)
{
    if ((argc >= 2) && (strcmp(SERVER, argv[1]) == 0))
        return (server(argv[2]));

    if ((argc >= 3) && (strcmp(CLIENT, argv[1]) == 0))
        return (client(argv[2], argv[3]));

    fprintf(stderr, "Usage: pubsub %s|%s <URL> <ARG> ...\n",
            SERVER, CLIENT);
    return 1;
}
