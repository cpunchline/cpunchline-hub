/* Goal: receive a non-trivial amount of data over TCP from a remote host, and have everything arrive. While doing so, study
   the role of various system calls, ioctls and socket options.

   Run as 'program-b' and it will listen on port 9876

   Bert Hubert, Netherlabs Computer Consulting BV - bert.hubert@netherlabs.nl
*/

#include <sys/types.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

static void setSocketReusable(int fd)
{
    int tmp = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char *)&tmp, sizeof tmp);
}

int main(void)
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    setSocketReusable(sock);

    struct sockaddr_in local;
    local.sin_family = AF_INET;
    inet_pton(AF_INET, "0.0.0.0", &local.sin_addr);
    local.sin_port = ntohs(9876);

    if (bind(sock, (struct sockaddr *)&local, sizeof(local)) < 0)
    {
        perror("Binding socket");
        exit(1);
    }
    listen(sock, 5);

    socklen_t socklen = sizeof(local);
    int client = accept(sock, (struct sockaddr *)&local, &socklen);
    if (0 > write(client, "welcome\r\n", 9))
    {
        perror("Writing welcome message");
        exit(1);
    }

    if (client < 0)
    {
        perror("Accepting connection");
        exit(1);
    }
    char buffer[4096];
    ssize_t bytesRead = 0;
    ssize_t res;
    for (;;)
    {
        res = read(client, buffer, sizeof(buffer));
        if (res < 0)
        {
            printf("Read error after %zd bytes: %s\n", bytesRead, strerror(errno));
            exit(1);
        }
        if (!res)
        {
            printf("EOF\n");
            break;
        }
        bytesRead += res;
    }
    printf("%zd\n", bytesRead);
    return 0;
}
