#include "ipc_zmq_inn.h"

void get_zmq_lib_version()
{
    int major = -1, minor = -1, patch = -1;
    zmq_version(&major, &minor, &patch);
    LOG_PRINT_INFO("ZeroMQ Version[%d.%d.%d]-%d", major, minor, patch, ZMQ_VERSION);
}

int32_t ipc_zmq_poll(void *socket, long timeout)
{
    if (NULL == socket)
    {
        LOG_PRINT_ERROR("invalid params!");
        return -1;
    }

    int32_t ret = -1;
    int32_t zmq_poll_ret = -1;
    struct timespec start = {};
    struct timespec end = {};

#if ZMQ_VERSION < 40201
    zmq_pollitem_t items[1] = {
        {
         socket,
         0,
         ZMQ_POLLIN | ZMQ_POLLERR,
         0,
         },
    };
#else
    zmq_pollitem_t items[1] = {
        {
         socket,
         0,
         ZMQ_POLLIN | ZMQ_POLLERR | ZMQ_POLLPRI,
         0,
         },
    };
#endif

    clock_gettime(CLOCK_MONOTONIC, &start);
    while (1)
    {
        zmq_poll_ret = zmq_poll(items, 1, timeout);
        if (zmq_poll_ret < 0)
        {
            LOG_PRINT_ERROR("zmq_poll fail, timeout[%ld], errno=%d(%s), zmq_errno=%d(%s)", timeout, errno, strerror(errno), zmq_errno(), zmq_strerror(zmq_errno()));
            if (EINTR == errno)
            {
                if (timeout < 0)
                {
                    continue;
                }
                clock_gettime(CLOCK_MONOTONIC, &end);
                timeout -= (end.tv_sec - start.tv_sec) * IPC_ZMQ_S_TO_MS + (end.tv_nsec - start.tv_nsec) / IPC_ZMQ_MS_TO_NS;
                if (timeout > 0)
                {
                    continue;
                }
            }
            ret = -1;
        }
        else if (0 == zmq_poll_ret)
        {
            LOG_PRINT_DEBUG("zmq_poll timeout, revents[%u]", items[0].revents);
            ret = -8;
        }
        else
        {
            if (items[0].revents & ZMQ_POLLIN)
            {
                ret = 0;
            }
            else
            {
                LOG_PRINT_ERROR("zmq_poll fail, zmq_poll_ret[%d], revents[%u]", zmq_poll_ret, items[0].revents);
                ret = -1;
            }
        }
        break;
    }

    return ret;
}

void error_receive_handle(void *rep)
{
    int64_t more = 0;
    size_t len = sizeof(int64_t);

    do
    {
        zmq_getsockopt(rep, ZMQ_RCVMORE, &more, &len);
        if (1 == more)
        {
            zmq_msg_t request;
            zmq_msg_init(&request);
            zmq_msg_recv(&request, rep, 0);
            zmq_msg_close(&request);
            continue;
        }
    } while (more);

    zmq_msg_t reply;
    zmq_msg_init(&reply);
    zmq_msg_send(&reply, rep, 0);
    zmq_msg_close(&reply);
}

void ipc_zmq_msg_free(void *data_, void *hint_)
{
    (void)hint_;
    if (NULL != data_)
    {
        free(data_);
        data_ = NULL;
    }
}