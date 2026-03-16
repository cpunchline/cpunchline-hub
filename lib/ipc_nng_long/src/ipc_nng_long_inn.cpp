#include <atomic>
#include "ipc_nng_long_inn.hpp"

static void _get_nng_lib_version()
{
    LOG_PRINT_INFO("NNG Version[%s]", nng_version());
}

void get_nng_lib_version()
{
    static pthread_once_t print_once = PTHREAD_ONCE_INIT;
    pthread_once(&print_once, _get_nng_lib_version);
}

uint16_t get_index()
{
    static std::atomic_uint16_t index = 1;
    return atomic_fetch_add(&index, 1) & UINT16_MAX;
}

bool get_process_name(char *process_name)
{
    char path[128] = {};
    ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (len != -1)
    {
        path[len] = '\0';
        strcpy(process_name, basename(path));
        LOG_PRINT_DEBUG("process_name[%s]-basename[%s]", path, process_name);
        return true;
    }

    LOG_PRINT_ERROR("readlink fail, errno[%d]", errno);

    return false;
}

int32_t ipc_init_dial(nng_socket sock, const char *url)
{
    int retry_cnt = 0;
    int ret = -1;

    do
    {
        ret = nng_dial(sock, url, NULL, 0);
        if (0 != ret)
        {
            LOG_PRINT_ERROR("nng_dial fail, ret[%d](%s)", ret, strerror(ret));
        }
        else
        {
            break;
        }
    } while (retry_cnt++ < 10);

    if (retry_cnt >= 10)
    {
        ret = -1;
    }

    return ret;
}

static void sock_send_cb(void *arg)
{
    nng_aio *aio = (nng_aio *)(((ipc_nng_long_sock_aio_t *)arg)->aio);
    int32_t ret = -1;

    ret = nng_aio_result(aio);
    if (0 != ret)
    {
        LOG_PRINT_ERROR("nng_aio_result fail, ret[%d]", ret);
        nng_msg_free(nng_aio_get_msg(aio));
        return;
    }
    nng_aio_reap(aio);

    if (NULL != arg)
    {
        nng_free(arg, sizeof(ipc_nng_long_sock_aio_t *));
    }
    else
    {
        LOG_PRINT_ERROR("arg is null");
    }
}

int32_t sock_aio_alloc_sendmsg(nng_socket sock, nng_msg *msg)
{
    int32_t ret = 0;
    if (NULL == msg)
    {
        return -1;
    }

    ipc_nng_long_sock_aio_t *sock_aio = NULL;

    sock_aio = (ipc_nng_long_sock_aio_t *)nng_alloc(sizeof(ipc_nng_long_sock_aio_t));
    if (NULL == sock_aio)
    {
        return -1;
    }
    ret = nng_aio_alloc(&sock_aio->aio, sock_send_cb, sock_aio);
    if (ret != 0)
    {
        LOG_PRINT_ERROR("nng_aio_alloc fai, ret[%d]", ret);
        return -1;
    }
    nng_aio_set_timeout(sock_aio->aio, D_IPC_NNG_LONG_WAIT_TIMEOUT);
    nng_aio_set_msg(sock_aio->aio, msg);
    nng_socket_send(sock, sock_aio->aio);

    return 0;
}

int32_t sock_aio_alloc_send(nng_socket sock, const void *data, size_t len)
{
    nng_msg *send_msg = NULL;
    int32_t ret = -1;

    if (NULL == data)
    {
        return -1;
    }

    ret = nng_msg_alloc(&send_msg, 0);
    if (0 != ret)
    {
        LOG_PRINT_ERROR("nng_msg_alloc fail, ret[%d]", ret);
        return ret;
    }

    ret = nng_msg_append(send_msg, data, len);
    if (0 != ret)
    {
        LOG_PRINT_ERROR("nng_msg_append fail, ret[%d]", ret);
        nng_msg_free(send_msg);
    }
    else
    {
        ret = sock_aio_alloc_sendmsg(sock, send_msg);
    }

    return ret;
}
