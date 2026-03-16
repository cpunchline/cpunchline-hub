#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <fcntl.h>
#include <stddef.h>
#include "openssl/rand.h"
#include "openssl_print.h"

#define secure_random_bytes(buf, len) RAND_bytes_ex(NULL, buf, len, 256)
#define secure_pri_random_bytes       RAND_priv_bytes_ex(NULL, buf, len, 256)

static inline int os_random_bytes(unsigned char *buf, size_t len)
{
    if (buf == NULL || len == 0)
    {
        return -1;
    }

    int fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0)
    {
        LOG_PRINT_ERROR("open fail, errno[%d](%s)", errno, strerror(errno));
        return -1;
    }

    size_t total = 0;
    while (total < len)
    {
        ssize_t ret = read(fd, buf + total, len - total);
        if (ret <= 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            LOG_PRINT_ERROR("read fail, errno[%d](%s)", errno, strerror(errno));

            close(fd);
            return -1;
        }
        total += (size_t)ret;
    }
    close(fd);

    return 0;
}

#ifdef __cplusplus
}
#endif