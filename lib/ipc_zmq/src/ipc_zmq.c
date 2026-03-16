#include "ipc_zmq_inn.h"

static ipc_zmq_manager_info_t *g_ipc_minfo[E_IPC_ZMQ_MODULE_ID_MAX] = {NULL};
static const char *socket_prefix = NULL;

int32_t response_worker(void *rep, ipc_zmq_manager_info_t *pinfo)
{
    int32_t ret = -1;
    zmq_msg_t request;
    ipc_zmq_msg_baseinfo_t msg_baseinfo = {};

    zmq_msg_init(&request);
    ret = zmq_msg_recv(&request, rep, ZMQ_DONTWAIT);
    if (-1 == ret)
    {
        LOG_PRINT_ERROR("zmq_msg_recv fail, errno[%d](%s), zmq_errno[%d](%s)!", errno, strerror(errno), zmq_errno(), zmq_strerror(zmq_errno()));
        zmq_msg_close(&request);
        error_receive_handle(rep);
        return -1;
    }
    memcpy(&msg_baseinfo, zmq_msg_data(&request), zmq_msg_size(&request));

    int more = zmq_msg_more(&request);
    zmq_msg_close(&request);
    if (0 == more)
    {
        LOG_PRINT_ERROR("recv more message error");
        error_receive_handle(rep);
    }

    zmq_msg_init(&request);
    ret = zmq_msg_recv(&request, rep, ZMQ_DONTWAIT);
    if (-1 == ret)
    {
        LOG_PRINT_ERROR("zmq_msg_recv fail, errno[%d](%s), zmq_errno[%d](%s)!", errno, strerror(errno), zmq_errno(), zmq_strerror(zmq_errno()));
        zmq_msg_close(&request);
        error_receive_handle(rep);
        return -1;
    }

    zmq_msg_t reply;

    if (NULL != pinfo->reg_info.response_handler &&
        (E_IPC_ZMQ_MSG_TYPE_SYNC == msg_baseinfo.msg_type || E_IPC_ZMQ_MSG_TYPE_ASYNC == msg_baseinfo.msg_type))
    {
        if (0 != msg_baseinfo.recv_len)
        {
            void *out_struct = (void *)calloc(1, msg_baseinfo.recv_len);
            if (NULL == out_struct)
            {
                LOG_PRINT_ERROR("calloc fail, errno[%d](%s)!", errno, strerror(errno));
                zmq_msg_close(&request);
                return -1;
            }
            memset(out_struct, 0x00, msg_baseinfo.recv_len);
            pinfo->reg_info.response_handler(&msg_baseinfo, zmq_msg_data(&request), out_struct);

            zmq_msg_init_size(&reply, msg_baseinfo.recv_len);
            memcpy(zmq_msg_data(&reply), out_struct, msg_baseinfo.recv_len);
            zmq_msg_send(&reply, rep, 0);
            if (NULL != out_struct)
            {
                free(out_struct);
                out_struct = NULL;
            }
        }
        else
        {
            pinfo->reg_info.response_handler(&msg_baseinfo, zmq_msg_data(&request), NULL);
            zmq_msg_init(&reply);
            zmq_msg_send(&reply, rep, 0);
        }
    }
    else if (NULL != pinfo->reg_info.notify_handler && E_IPC_ZMQ_MSG_TYPE_NOTIFY == msg_baseinfo.msg_type)
    {
        pinfo->reg_info.notify_handler(&msg_baseinfo, zmq_msg_data(&request));
        zmq_msg_init(&reply);
        zmq_msg_send(&reply, rep, 0);
    }
    else if (NULL != pinfo->reg_info.stream_handler && E_IPC_ZMQ_MSG_TYPE_STREAM == msg_baseinfo.msg_type)
    {
        int32_t stream_ret = IPC_ZMQ_STREAM_RESULT;
        zmq_msg_init_size(&reply, sizeof(stream_ret));
        memcpy(zmq_msg_data(&reply), &stream_ret, sizeof(stream_ret));
        zmq_msg_send(&reply, rep, 0);
        pinfo->reg_info.stream_handler(&msg_baseinfo, zmq_msg_data(&request));
    }
    else
    {
        zmq_msg_init(&reply);
        zmq_msg_send(&reply, rep, 0);
    }

    zmq_msg_close(&reply);
    zmq_msg_close(&request);

    return ret;
}

static void ipc_zmq_single_wait_response_handler(void *arg)
{
    int ret = -1;
    (void)ret;
    ipc_zmq_manager_info_t *pinfo = (ipc_zmq_manager_info_t *)arg;

    while (pinfo->is_run)
    {
        ret = ipc_zmq_poll(pinfo->rep, D_IPC_ZMQ_WAIT_CONTINUE);
        assert(0 == ret);
        response_worker(pinfo->rep, pinfo);
    }

    zmq_close(pinfo->rep);
}

static void *ipc_zmq_reponse_worker(void *arg)
{
    int32_t ret = -1;
    (void)ret;
    char address_end[D_IPC_ZMQ_SOCKET_ADDRESS_LEN_MAX] = {};
    ipc_zmq_manager_info_t *pinfo = (ipc_zmq_manager_info_t *)arg;

    void *rep = zmq_socket(pinfo->ctx, ZMQ_REP);
    snprintf(address_end, sizeof(address_end), D_INPROC_ZMQ_BACK_REQ_RESP_ADDRESS, pinfo->reg_info.module_id);
    zmq_connect(rep, address_end);

    while (pinfo->is_run)
    {
        ret = ipc_zmq_poll(rep, D_IPC_ZMQ_WAIT_CONTINUE);
        assert(0 == ret);
        response_worker(rep, pinfo);
    }

    zmq_close(rep);

    return NULL;
}

static void ipc_zmq_mult_wait_response_handler(void *arg)
{
    int32_t ret = -1;
    int tmp_err = 0;
    char address_front[D_IPC_ZMQ_SOCKET_ADDRESS_LEN_MAX] = {};
    char address_end[D_IPC_ZMQ_SOCKET_ADDRESS_LEN_MAX] = {};
    ipc_zmq_manager_info_t *pinfo = (ipc_zmq_manager_info_t *)arg;

    void *frontend = zmq_socket(pinfo->ctx, ZMQ_ROUTER);
    snprintf(address_front, sizeof(address_front), D_IPC_ZMQ_REQ_RESP_ADDRESS, socket_prefix, pinfo->reg_info.module_id);
    ret = zmq_bind(frontend, address_front);
    assert(0 == ret);

    void *backend = zmq_socket(pinfo->ctx, ZMQ_DEALER);
    snprintf(address_end, sizeof(address_end), D_INPROC_ZMQ_BACK_REQ_RESP_ADDRESS, pinfo->reg_info.module_id);
    ret = zmq_bind(backend, address_end);
    assert(0 == ret);

    for (size_t i = 0; i < 3; ++i)
    {
        pthread_t tid = 0;
        ret = pthread_create(&tid, NULL, ipc_zmq_reponse_worker, pinfo);
        if (0 != ret)
        {
            LOG_PRINT_ERROR("pthread_create fail, errno[%d](%s)!", errno, strerror(errno));
            break;
        }
        pthread_detach(tid);
    }

    do
    {
        ret = zmq_proxy(frontend, backend, NULL);
        tmp_err = errno;
        LOG_PRINT_ERROR("zmq_proxy fail, errno[%d](%s)!", tmp_err, strerror(tmp_err));
    } while (-1 == ret && tmp_err == EINTR);
}

static void ipc_zmq_async_wait_response_handler(void *arg)
{
    int ret = -1;
    int val = 0;
    char address_req[D_IPC_ZMQ_SOCKET_ADDRESS_LEN_MAX] = {};
    ipc_zmq_manager_info_t *pinfo = (ipc_zmq_manager_info_t *)arg;
    ipc_zmq_msg_baseinfo_t msg_baseinfo = {};

    while (pinfo->is_run)
    {
        ret = ipc_zmq_poll(pinfo->dealer, D_IPC_ZMQ_WAIT_CONTINUE);
        if (0 != ret)
        {
            LOG_PRINT_ERROR("ipc_zmq_poll fail, ret[%d]", ret);
            continue;
        }

        zmq_msg_t dealer_msg;
        zmq_msg_init(&dealer_msg);
        zmq_msg_recv(&dealer_msg, pinfo->dealer, ZMQ_DONTWAIT);
        memcpy(&msg_baseinfo, zmq_msg_data(&dealer_msg), zmq_msg_size(&dealer_msg));

        void *async_req = zmq_socket(pinfo->ctx, ZMQ_REQ);
        ret = zmq_setsockopt(async_req, ZMQ_LINGER, &val, sizeof(val));
        assert(0 == ret);
        snprintf(address_req, sizeof(address_req), D_IPC_ZMQ_REQ_RESP_ADDRESS, socket_prefix, msg_baseinfo.dest_id);
        zmq_connect(async_req, address_req);
        zmq_msg_send(&dealer_msg, async_req, ZMQ_SNDMORE);
        zmq_msg_close(&dealer_msg);

        zmq_msg_init(&dealer_msg);
        zmq_msg_recv(&dealer_msg, pinfo->dealer, ZMQ_DONTWAIT);
        zmq_msg_send(&dealer_msg, async_req, 0);
        zmq_msg_close(&dealer_msg);

        ret = ipc_zmq_poll(async_req, D_IPC_ZMQ_WAIT_TIMEOUT);
        if (0 == ret)
        {
            zmq_msg_t async_resp_msg;
            zmq_msg_init(&async_resp_msg);
            zmq_msg_recv(&async_resp_msg, async_req, ZMQ_DONTWAIT);
            LOG_PRINT_DEBUG("recv async resp msg size[%zd]!", zmq_msg_size(&async_resp_msg));
            if (NULL != pinfo->reg_info.async_handler)
            {
                if (0 != zmq_msg_size(&async_resp_msg))
                {
                    pinfo->reg_info.async_handler(&msg_baseinfo, zmq_msg_data(&async_resp_msg));
                }
                else
                {
                    pinfo->reg_info.async_handler(&msg_baseinfo, NULL);
                }
                zmq_msg_close(&async_resp_msg);
            }
        }
        else
        {
            LOG_PRINT_ERROR("async req msg_id[%u] wait resp fail or timeout, ret[%d]!", msg_baseinfo.msg_id, ret);
        }
        zmq_close(async_req);
    }
}

static void ipc_zmq_broadcast_proxy_handler(void *arg)
{
    int ret = -1;
    int tmp_err = 0;
    ipc_zmq_manager_info_t *pinfo = (ipc_zmq_manager_info_t *)arg;

    do
    {
        ret = zmq_proxy(pinfo->frontend, pinfo->backend, NULL);
        tmp_err = errno;
        LOG_PRINT_ERROR("zmq_proxy fail, errno[%d](%s)!", tmp_err, strerror(tmp_err));
    } while (-1 == ret && tmp_err == EINTR);

    zmq_close(pinfo->frontend);
    zmq_close(pinfo->backend);
}

static void ipc_zmq_broadcast_subscriber_handler(void *arg)
{
    int ret = -1;
    zmq_msg_t message;
    int tmp_err = 0;
    ipc_zmq_manager_info_t *pinfo = (ipc_zmq_manager_info_t *)arg;

    char backend_socket_path[D_IPC_ZMQ_SOCKET_ADDRESS_LEN_MAX] = {};

    ipc_zmq_broadcast_msg_baseinfo_t broadcast_baseinfo = {};
    while (pinfo->is_run)
    {
        ret = ipc_zmq_poll(pinfo->sub, D_IPC_ZMQ_WAIT_CONTINUE);
        if (0 != ret)
        {
            LOG_PRINT_ERROR("ipc_zmq_poll fail, ret[%d]", ret);
            continue;
        }

        memset(&broadcast_baseinfo, 0x00, sizeof(broadcast_baseinfo));
        zmq_msg_init(&message);
        ret = zmq_msg_recv(&message, pinfo->sub, ZMQ_DONTWAIT);
        if (-1 == ret)
        {
            tmp_err = errno;
            LOG_PRINT_ERROR("zmq_msg_recv fail, errno[%d](%s), zmq_errno[%d](%s)!", errno, strerror(errno), zmq_errno(), zmq_strerror(tmp_err));
            zmq_msg_close(&message);
            if (EINTR == tmp_err)
            {
                continue;
            }
            break;
        }

        int more = zmq_msg_more(&message);
        zmq_msg_close(&message);
        if (0 == more)
        {
            LOG_PRINT_ERROR("recv more message error");
            continue;
        }

        zmq_msg_init(&message);
        zmq_msg_recv(&message, pinfo->sub, ZMQ_DONTWAIT);
        memcpy(&broadcast_baseinfo, zmq_msg_data(&message), zmq_msg_size(&message));

        more = zmq_msg_more(&message);
        zmq_msg_close(&message);
        if (0 == more)
        {
            LOG_PRINT_ERROR("recv more message error");
            continue;
        }

        zmq_msg_init(&message);
        zmq_msg_recv(&message, pinfo->sub, ZMQ_DONTWAIT);
        if (NULL != pinfo->reg_info.broadcast_handler)
        {
            if (0 != zmq_msg_size(&message))
            {
                pinfo->reg_info.broadcast_handler(&broadcast_baseinfo, zmq_msg_data(&message));
            }
            else
            {
                pinfo->reg_info.broadcast_handler(&broadcast_baseinfo, NULL);
            }
        }
        zmq_msg_close(&message);
    }

    zmq_close(pinfo->sub);
    LOG_PRINT_ERROR("subscriber[%s] exit", backend_socket_path);
}

int32_t ipc_zmq_init(ipc_zmq_register_info_t *reg_info)
{
    int32_t ret = 0;
    ipc_zmq_manager_info_t *pinfo = NULL;

    get_zmq_lib_version();

    if (NULL == reg_info)
    {
        LOG_PRINT_ERROR("invalid params!");
        return -1;
    }

    if (NULL != g_ipc_minfo[reg_info->module_id])
    {
        LOG_PRINT_ERROR("ipc inited!");
        return -1;
    }

    socket_prefix = getenv("XDG_RUNTIME_DIR");
    if (!socket_prefix || access(socket_prefix, W_OK) != 0)
    {
        socket_prefix = "/tmp";
    }

    char socket_path[D_IPC_ZMQ_SOCKET_ADDRESS_LEN_MAX] = {};
    char frontend_socket_path[D_IPC_ZMQ_SOCKET_ADDRESS_LEN_MAX] = {};
    char backend_socket_path[D_IPC_ZMQ_SOCKET_ADDRESS_LEN_MAX] = {};
    snprintf(socket_path, sizeof(socket_path), D_IPC_ZMQ_REQ_RESP_ADDRESS, socket_prefix, reg_info->module_id);
    snprintf(frontend_socket_path, sizeof(frontend_socket_path), D_IPC_ZMQ_BROADCAST_FRONTEND_ADDRESS, socket_prefix);
    snprintf(backend_socket_path, sizeof(backend_socket_path), D_IPC_ZMQ_BROADCAST_BACKEND_ADDRESS, socket_prefix);

    pinfo = (ipc_zmq_manager_info_t *)calloc(1, (sizeof(ipc_zmq_manager_info_t)));
    if (NULL == pinfo)
    {
        LOG_PRINT_ERROR("calloc fail, errno[%d](%s)!", errno, strerror(errno));
        return -1;
    }

    g_ipc_minfo[reg_info->module_id] = pinfo;
    memcpy(&pinfo->reg_info, reg_info, sizeof(ipc_zmq_register_info_t));

    pinfo->is_run = true;
    pinfo->ctx = zmq_ctx_new();

    if (NULL != reg_info->broadcast_handler)
    {
        // connect with manager module
        pinfo->pub = zmq_socket(pinfo->ctx, ZMQ_PUB);
        pthread_mutex_init(&pinfo->mutex_pub, NULL);
        zmq_connect(pinfo->pub, frontend_socket_path);
        zmq_pollitem_t items[1] = {
            {pinfo->pub, 0, ZMQ_POLLOUT, 0},
        };
        if (zmq_poll(items, 1, D_IPC_ZMQ_WAIT_TIMEOUT) < 0)
        {
            LOG_PRINT_ERROR("zmq_poll fail!");
            return -1;
        }

        if (E_IPC_ZMQ_MODULE_ID_MANAGER == reg_info->module_id)
        {
            pinfo->frontend = zmq_socket(pinfo->ctx, ZMQ_XSUB);
            ret = zmq_bind(pinfo->frontend, frontend_socket_path);
            assert(0 == ret);

            pinfo->backend = zmq_socket(pinfo->ctx, ZMQ_XPUB);
            ret = zmq_bind(pinfo->backend, backend_socket_path);
            assert(0 == ret);

            pinfo->broadcast_proxy_thread = zmq_threadstart(&ipc_zmq_broadcast_proxy_handler, pinfo);
            if (NULL == pinfo->broadcast_proxy_thread)
            {
                LOG_PRINT_ERROR("zmq_threadstart fail, zmq_errno[%d](%s)!", zmq_errno(), zmq_strerror(zmq_errno()));
                return -1;
            }
        }

        pinfo->sub = zmq_socket(pinfo->ctx, ZMQ_SUB);
        snprintf(backend_socket_path, sizeof(backend_socket_path), D_IPC_ZMQ_BROADCAST_BACKEND_ADDRESS, socket_prefix);
        zmq_connect(pinfo->sub, backend_socket_path);
        ret = zmq_setsockopt(pinfo->sub, ZMQ_SUBSCRIBE, D_IPC_ZMQ_BROADCAST_TOPIC_STR, strlen(D_IPC_ZMQ_BROADCAST_TOPIC_STR));
        assert(0 == ret);
        pinfo->broadcast_thread = zmq_threadstart(&ipc_zmq_broadcast_subscriber_handler, pinfo);
        if (NULL == pinfo->broadcast_thread)
        {
            LOG_PRINT_ERROR("zmq_threadstart fail, zmq_errno[%d](%s)!", zmq_errno(), zmq_strerror(zmq_errno()));
            return -1;
        }
    }

    if (NULL != reg_info->async_handler)
    {
        char async_queue_address[D_IPC_ZMQ_SOCKET_ADDRESS_LEN_MAX] = {};
        pinfo->dealer = zmq_socket(pinfo->ctx, ZMQ_DEALER);
#if 0
    // 设置接收水位线
    int rcvhwm = 1000;
    ret = zmq_setsockopt(pinfo->dealer, ZMQ_RCVHWM, &rcvhwm, sizeof(rcvhwm));
    assert(0 == ret);
#endif
        snprintf(async_queue_address, sizeof(async_queue_address), D_INPROC_ZMQ_ASYNC_QUEUE_ADDRESS, pinfo->reg_info.module_id);
        ret = zmq_bind(pinfo->dealer, async_queue_address);
        assert(0 == ret);

        pinfo->async_thread = zmq_threadstart(&ipc_zmq_async_wait_response_handler, pinfo);
        if (NULL == pinfo->async_thread)
        {
            LOG_PRINT_ERROR("zmq_threadstart fail, zmq_errno[%d](%s)!", zmq_errno(), zmq_strerror(zmq_errno()));
            return -1;
        }
    }

    if (NULL != reg_info->response_handler)
    {
        if (E_IPC_ZMQ_WORKMODE_SINGLE_WORKER == reg_info->work_mode)
        {
            char rep_address[D_IPC_ZMQ_SOCKET_ADDRESS_LEN_MAX] = {};
            pinfo->rep = zmq_socket(pinfo->ctx, ZMQ_REP);
#if 0
    // 设置接收水位线
    int rcvhwm = 1000;
    ret = zmq_setsockopt(pinfo->rep, ZMQ_RCVHWM, &rcvhwm, sizeof(rcvhwm));
    assert(0 == ret);
#endif
            snprintf(rep_address, sizeof(rep_address), D_IPC_ZMQ_REQ_RESP_ADDRESS, socket_prefix, pinfo->reg_info.module_id);
            ret = zmq_bind(pinfo->rep, rep_address);
            assert(0 == ret);
            pinfo->rep_thread = zmq_threadstart(&ipc_zmq_single_wait_response_handler, pinfo);
        }
        else
        {
            pinfo->rep_thread = zmq_threadstart(&ipc_zmq_mult_wait_response_handler, pinfo);
        }

        if (NULL == pinfo->rep_thread)
        {
            LOG_PRINT_ERROR("zmq_threadstart fail, zmq_errno[%d](%s)!", zmq_errno(), zmq_strerror(zmq_errno()));
            return -1;
        }
    }

    return ret;
}

int32_t ipc_zmq_destroy(uint32_t module_id)
{
    if (module_id <= E_IPC_ZMQ_MODULE_ID_INVALID || module_id >= E_IPC_ZMQ_MODULE_ID_MAX)
    {
        return -1;
    }

    ipc_zmq_manager_info_t *pinfo = g_ipc_minfo[module_id];
    if (NULL != pinfo)
    {
        pinfo->is_run = false;
        zmq_ctx_term(pinfo->ctx);
        pthread_mutex_destroy(&pinfo->mutex_pub);
        if (NULL != pinfo->rep_thread)
        {
            zmq_threadclose(pinfo->rep_thread);
        }
        if (NULL != pinfo->async_thread)
        {
            zmq_threadclose(pinfo->async_thread);
        }
        free(pinfo);
        pinfo = NULL;
        g_ipc_minfo[module_id] = NULL;
    }

    return 0;
}

int32_t ipc_zmq_send_notify(uint32_t src_id, uint32_t dest_id, uint32_t msg_id, const void *notify_data,
                            size_t notify_data_len)
{
    int32_t ret = -1;
    int val = 0;
    void *notify;
    char address[D_IPC_ZMQ_SOCKET_ADDRESS_LEN_MAX] = {};
    ipc_zmq_msg_baseinfo_t msg_baseinfo = {};
    ipc_zmq_manager_info_t *pinfo = g_ipc_minfo[src_id];

    notify = zmq_socket(pinfo->ctx, ZMQ_REQ);
    if (NULL == notify)
    {
        LOG_PRINT_ERROR("zmq_socket fail, zmq_errno[%d](%s)!", zmq_errno(), zmq_strerror(zmq_errno()));
        return -1;
    }

    ret = zmq_setsockopt(notify, ZMQ_LINGER, &val, sizeof(val));
    assert(0 == ret);

    snprintf(address, sizeof(address), D_IPC_ZMQ_REQ_RESP_ADDRESS, socket_prefix, dest_id);
    ret = zmq_connect(notify, address);
    if (-1 == ret)
    {
        LOG_PRINT_ERROR("zmq_connect fail, zmq_errno[%d](%s)!", zmq_errno(), zmq_strerror(zmq_errno()));
        zmq_close(notify);
        return -1;
    }

    zmq_msg_t notify_msg;

    msg_baseinfo.src_id = src_id;
    msg_baseinfo.dest_id = dest_id;
    msg_baseinfo.msg_type = E_IPC_ZMQ_MSG_TYPE_NOTIFY;
    msg_baseinfo.msg_id = msg_id;
    msg_baseinfo.send_len = notify_data_len;
    msg_baseinfo.recv_len = 0;

    zmq_msg_init_size(&notify_msg, sizeof(msg_baseinfo));
    memcpy(zmq_msg_data(&notify_msg), &msg_baseinfo, sizeof(msg_baseinfo));
    ret = zmq_msg_send(&notify_msg, notify, ZMQ_SNDMORE);
    if (-1 == ret)
    {
        LOG_PRINT_ERROR("zmq_msg_send fail, zmq_errno[%d](%s)!", zmq_errno(), zmq_strerror(zmq_errno()));
        zmq_msg_close(&notify_msg);
        zmq_close(notify);
        return -1;
    }
    zmq_msg_close(&notify_msg);

    if (notify_data_len > 0)
    {
        zmq_msg_init_size(&notify_msg, notify_data_len);
        memcpy(zmq_msg_data(&notify_msg), notify_data, notify_data_len);
    }
    else
    {
        zmq_msg_init(&notify_msg);
    }

    ret = zmq_msg_send(&notify_msg, notify, 0);
    if (-1 == ret)
    {
        LOG_PRINT_ERROR("zmq_msg_send fail, zmq_errno[%d](%s)!", zmq_errno(), zmq_strerror(zmq_errno()));
        zmq_msg_close(&notify_msg);
        zmq_close(notify);
        return -1;
    }
    zmq_msg_close(&notify_msg);

    ret = ipc_zmq_poll(notify, D_IPC_ZMQ_WAIT_TIMEOUT);
    LOG_PRINT_DEBUG("ipc_zmq_poll ret[%d]", ret);
    if (0 == ret)
    {
        zmq_msg_t notify_resp_msg;
        zmq_msg_init(&notify_resp_msg);
        ret = zmq_msg_recv(&notify_resp_msg, notify, ZMQ_DONTWAIT);
        if (-1 == ret)
        {
            LOG_PRINT_ERROR("zmq_msg_recv fail, zmq_errno[%d](%s)!", zmq_errno(), zmq_strerror(zmq_errno()));
        }
        zmq_msg_close(&notify_resp_msg);
    }

    zmq_disconnect(notify, address);
    zmq_close(notify);

    return 0;
}

int32_t ipc_zmq_send_sync(uint32_t src_id, uint32_t dest_id, uint32_t msg_id, const void *sync_req_data,
                          size_t sync_req_data_len, void *sync_resp_data, size_t *sync_resp_data_len,
                          size_t sync_resp_data_max_len, long timeout)
{
    int32_t ret = -1;
    int val = 0;
    void *req = NULL;
    char address[D_IPC_ZMQ_SOCKET_ADDRESS_LEN_MAX] = {};
    ipc_zmq_msg_baseinfo_t msg_baseinfo = {};
    ipc_zmq_manager_info_t *pinfo = g_ipc_minfo[src_id];

    req = zmq_socket(pinfo->ctx, ZMQ_REQ);
    if (NULL == req)
    {
        LOG_PRINT_ERROR("zmq_socket fail, zmq_errno[%d](%s)!", zmq_errno(), zmq_strerror(zmq_errno()));
        return -1;
    }

    do
    {
        ret = zmq_setsockopt(req, ZMQ_LINGER, &val, sizeof(val));
        assert(0 == ret);

        snprintf(address, sizeof(address), D_IPC_ZMQ_REQ_RESP_ADDRESS, socket_prefix, dest_id);
        ret = zmq_connect(req, address);
        if (-1 == ret)
        {
            LOG_PRINT_ERROR("zmq_connect fail, zmq_errno[%d](%s)!", zmq_errno(), zmq_strerror(zmq_errno()));
            break;
        }

        msg_baseinfo.src_id = src_id;
        msg_baseinfo.dest_id = dest_id;
        msg_baseinfo.msg_type = E_IPC_ZMQ_MSG_TYPE_SYNC;
        msg_baseinfo.msg_id = msg_id;
        msg_baseinfo.send_len = sync_req_data_len;
        msg_baseinfo.recv_len = sync_resp_data_max_len;

        int send_timeout = 10;
        ret = zmq_setsockopt(req, ZMQ_SNDTIMEO, &send_timeout, sizeof(send_timeout));
        assert(0 == ret);

        ret = zmq_send(req, &msg_baseinfo, sizeof(msg_baseinfo), ZMQ_SNDMORE);
        if (-1 == ret)
        {
            LOG_PRINT_ERROR("zmq_send fail, zmq_errno[%d](%s)!", zmq_errno(), zmq_strerror(zmq_errno()));
            break;
        }

        ret = zmq_send(req, sync_req_data, sync_req_data_len, 0);
        if (-1 == ret)
        {
            LOG_PRINT_ERROR("zmq_send fail, zmq_errno[%d](%s)!", zmq_errno(), zmq_strerror(zmq_errno()));
            break;
        }

        ret = ipc_zmq_poll(req, timeout);
        LOG_PRINT_DEBUG("ipc_zmq_poll ret[%d]", ret);
        if (0 != ret)
        {
            LOG_PRINT_ERROR("ipc_zmq_poll ret[%d]", ret);
            break;
        }

        zmq_msg_t sync_resp_msg;
        zmq_msg_init(&sync_resp_msg);
        ret = zmq_msg_recv(&sync_resp_msg, req, ZMQ_DONTWAIT);
        if (-1 == ret)
        {
            LOG_PRINT_ERROR("zmq_msg_recv fail, zmq_errno[%d](%s)!", zmq_errno(), zmq_strerror(zmq_errno()));
            zmq_msg_close(&sync_resp_msg);
            break;
        }

        if (zmq_msg_size(&sync_resp_msg) > sync_resp_data_max_len)
        {
            LOG_PRINT_ERROR("sync resp is over limit[%zu]!", zmq_msg_size(&sync_resp_msg));
            zmq_msg_close(&sync_resp_msg);
            ret = -1;
            break;
        }

        *sync_resp_data_len = zmq_msg_size(&sync_resp_msg);
        memcpy(sync_resp_data, zmq_msg_data(&sync_resp_msg), *sync_resp_data_len);
        zmq_msg_close(&sync_resp_msg);
        ret = 0;
    } while (0);

    zmq_disconnect(req, address);
    zmq_close(req);

    return ret;
}

int32_t ipc_zmq_send_async(uint32_t src_id, uint32_t dest_id, uint32_t msg_id, const void *async_req_data,
                           size_t async_req_data_len, size_t async_resp_data_len)
{
    int32_t ret = -1;
    int val = 0;
    void *dealer = NULL;
    ipc_zmq_msg_baseinfo_t msg_baseinfo = {};
    ipc_zmq_manager_info_t *pinfo = g_ipc_minfo[src_id];
    char async_queue_address[D_IPC_ZMQ_SOCKET_ADDRESS_LEN_MAX] = {};

    msg_baseinfo.src_id = src_id;
    msg_baseinfo.dest_id = dest_id;
    msg_baseinfo.msg_type = E_IPC_ZMQ_MSG_TYPE_ASYNC;
    msg_baseinfo.msg_id = msg_id;
    msg_baseinfo.send_len = async_req_data_len;
    msg_baseinfo.recv_len = async_resp_data_len;

    dealer = zmq_socket(pinfo->ctx, ZMQ_DEALER);
    if (NULL == dealer)
    {
        LOG_PRINT_ERROR("zmq_socket fail, zmq_errno[%d](%s)!", zmq_errno(), zmq_strerror(zmq_errno()));
        return -1;
    }

    do
    {
        ret = zmq_setsockopt(dealer, ZMQ_LINGER, &val, sizeof(val));
        assert(0 == ret);

        snprintf(async_queue_address, sizeof(async_queue_address), D_INPROC_ZMQ_ASYNC_QUEUE_ADDRESS, pinfo->reg_info.module_id);
        ret = zmq_connect(dealer, async_queue_address);
        if (-1 == ret)
        {
            LOG_PRINT_ERROR("zmq_connect fail, zmq_errno[%d](%s)!", zmq_errno(), zmq_strerror(zmq_errno()));
            break;
        }

        int send_timeout = 10;
        ret = zmq_setsockopt(dealer, ZMQ_SNDTIMEO, &send_timeout, sizeof(send_timeout));
        assert(0 == ret);

        ret = zmq_send(dealer, &msg_baseinfo, sizeof(msg_baseinfo), ZMQ_SNDMORE);
        if (-1 == ret)
        {
            LOG_PRINT_ERROR("zmq_send fail, zmq_errno[%d](%s)!", zmq_errno(), zmq_strerror(zmq_errno()));
            break;
        }

        ret = zmq_send(dealer, async_req_data, async_req_data_len, 0);
        if (-1 == ret)
        {
            LOG_PRINT_ERROR("zmq_send fail, zmq_errno[%d](%s)!", zmq_errno(), zmq_strerror(zmq_errno()));
            break;
        }
        ret = 0;
    } while (0);

    zmq_close(dealer);

    return ret;
}

int32_t ipc_zmq_send_stream(uint32_t src_id, uint32_t dest_id, uint32_t msg_id, const void *stream_data, size_t stream_data_len, long timeout)
{
    int32_t ret = -1;
    int val = 0;
    char address[D_IPC_ZMQ_SOCKET_ADDRESS_LEN_MAX] = {};
    ipc_zmq_msg_baseinfo_t msg_baseinfo = {};
    ipc_zmq_manager_info_t *pinfo = g_ipc_minfo[src_id];
    int32_t stream_ret = INT32_MAX;

    void *req = zmq_socket(pinfo->ctx, ZMQ_REQ);
    if (NULL == req)
    {
        LOG_PRINT_ERROR("zmq_socket fail, zmq_errno[%d](%s)!", zmq_errno(), zmq_strerror(zmq_errno()));
        return -1;
    }

    do
    {
        ret = zmq_setsockopt(req, ZMQ_LINGER, &val, sizeof(val));
        assert(0 == ret);

        snprintf(address, sizeof(address), D_IPC_ZMQ_REQ_RESP_ADDRESS, socket_prefix, dest_id);

        ret = zmq_connect(req, address);
        if (-1 == ret)
        {
            LOG_PRINT_ERROR("zmq_connect fail, zmq_errno[%d](%s)!", zmq_errno(), zmq_strerror(zmq_errno()));
            break;
        }

        msg_baseinfo.src_id = src_id;
        msg_baseinfo.dest_id = dest_id;
        msg_baseinfo.msg_type = E_IPC_ZMQ_MSG_TYPE_STREAM;
        msg_baseinfo.msg_id = msg_id;
        msg_baseinfo.send_len = stream_data_len;
        msg_baseinfo.recv_len = sizeof(stream_ret);

        int send_timeout = 10;
        ret = zmq_setsockopt(req, ZMQ_SNDTIMEO, &send_timeout, sizeof(send_timeout));
        assert(0 == ret);

        ret = zmq_send(req, &msg_baseinfo, sizeof(msg_baseinfo), ZMQ_SNDMORE);
        if (-1 == ret)
        {
            LOG_PRINT_ERROR("zmq_send fail, zmq_errno[%d](%s)!", zmq_errno(), zmq_strerror(zmq_errno()));
            break;
        }

        ret = zmq_send(req, stream_data, stream_data_len, ZMQ_DONTWAIT);
        if (-1 == ret)
        {
            LOG_PRINT_ERROR("zmq_send fail, zmq_errno[%d](%s)!", zmq_errno(), zmq_strerror(zmq_errno()));
            break;
        }

        ret = ipc_zmq_poll(req, timeout);
        if (0 != ret)
        {
            LOG_PRINT_ERROR("ipc_zmq_poll ret[%d]", ret);
            break;
        }

        zmq_msg_t sync_resp_msg;
        zmq_msg_init(&sync_resp_msg);
        ret = zmq_msg_recv(&sync_resp_msg, req, ZMQ_DONTWAIT);
        if (-1 == ret)
        {
            LOG_PRINT_ERROR("zmq_msg_recv fail, zmq_errno[%d](%s)!", zmq_errno(), zmq_strerror(zmq_errno()));
            zmq_msg_close(&sync_resp_msg);
            break;
        }

        if (zmq_msg_size(&sync_resp_msg) > sizeof(stream_ret))
        {
            ret = -1;
            LOG_PRINT_ERROR("sync resp is over limit[%zu]!", zmq_msg_size(&sync_resp_msg));
            zmq_msg_close(&sync_resp_msg);
            break;
        }

        memcpy(&stream_ret, zmq_msg_data(&sync_resp_msg), sizeof(stream_ret));
        zmq_msg_close(&sync_resp_msg);

        if (stream_ret != IPC_ZMQ_STREAM_RESULT)
        {
            LOG_PRINT_ERROR("invalid stream ret[%d]!\n", stream_ret);
            ret = -2;
        }
        else
        {
            ret = 0;
        }
    } while (0);

    zmq_disconnect(req, address);
    zmq_close(req);

    return ret;
}

int32_t ipc_zmq_send_broadcast(uint32_t src_id, uint32_t msg_id, const void *broadcast_data, size_t broadcast_data_len)
{
    const uint8_t topic_buff[] = {D_IPC_ZMQ_BROADCAST_TOPIC_STR};
    ipc_zmq_broadcast_msg_baseinfo_t bmsg_baseinfo = {};
    ipc_zmq_manager_info_t *pinfo = g_ipc_minfo[src_id];

    bmsg_baseinfo.sub = 1;
    bmsg_baseinfo.msg_id = msg_id;
    bmsg_baseinfo.src_id = src_id;
    bmsg_baseinfo.msg_type = E_IPC_ZMQ_MSG_TYPE_BROADCAST;

    pthread_mutex_lock(&pinfo->mutex_pub);
    zmq_send(pinfo->pub, topic_buff, sizeof(topic_buff), ZMQ_SNDMORE);
    zmq_send(pinfo->pub, &bmsg_baseinfo, sizeof(bmsg_baseinfo), ZMQ_SNDMORE);
    zmq_send(pinfo->pub, broadcast_data, broadcast_data_len, 0);
    pthread_mutex_unlock(&pinfo->mutex_pub);

    return 0;
}
