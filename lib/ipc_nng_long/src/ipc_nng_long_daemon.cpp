#include "ipc_nng_long_inn.hpp"

static constexpr nng_duration PROXY_RETRY_DELAY = 10;  // ms
static constexpr nng_duration PROXY_MAX_TIMEOUT = 100; // ms
static std::unordered_map<uint32_t, ipc_nng_long_reg_item_t *> g_proxy_reg_map = {};
static nng_mtx *g_proxy_reg_map_mtx = NULL;
static std::deque<uint32_t> g_registered_module_array = {}; // registered order
extern const char *socket_prefix;

static void print_proxy_reg_map()
{
    nng_mtx_lock(g_proxy_reg_map_mtx);
    LOG_PRINT_DEBUG("register module count[%zd]", g_proxy_reg_map.size());
    for (const auto &item : g_proxy_reg_map)
    {
        ipc_nng_long_reg_item_t *reg_item = item.second;
        if (NULL != reg_item)
        {
            LOG_PRINT_DEBUG("module_id[0x%x]:%s", reg_item->module_id, reg_item->address);
        }
    }
    nng_mtx_unlock(g_proxy_reg_map_mtx);
}

static ipc_nng_long_reg_item_t *find_proxy_reg_item(uint32_t module_id)
{
    ipc_nng_long_reg_item_t *ptr = NULL;
    nng_mtx_lock(g_proxy_reg_map_mtx);
    auto it = g_proxy_reg_map.find(module_id);
    if (g_proxy_reg_map.end() != it)
    {
        ptr = it->second;
    }
    else
    {
        ptr = NULL;
    }
    nng_mtx_unlock(g_proxy_reg_map_mtx);

    return ptr;
}

static void proxy_expire_cb(void *arg)
{
    int ret = -1;
    ipc_nng_long_msg_baseinfo_t *msg_baseinfo = NULL;
    ipc_nng_long_msg_baseinfo_t send_msg_baseinfo = {};
    ipc_nng_long_expire_item_t *expire_item = (ipc_nng_long_expire_item_t *)arg;

    ret = nng_aio_result(expire_item->time_aio);
    if (0 != ret)
    {
        LOG_PRINT_ERROR("nng_aio_result fail, ret[%d]", ret);
        nng_aio_reap(expire_item->time_aio);
        delete expire_item;
        expire_item = NULL;
        return;
    }

    msg_baseinfo = (ipc_nng_long_msg_baseinfo_t *)nng_msg_body(expire_item->msg);

    ipc_nng_long_reg_item_t *find_item = find_proxy_reg_item(msg_baseinfo->dest_id);
    if (NULL != find_item)
    {
        LOG_PRINT_ERROR("proxy req src_id[%u] to dest_id[%u] msg_id[%u] by address[%s]",
                        msg_baseinfo->src_id, msg_baseinfo->dest_id, msg_baseinfo->msg_id, find_item->address);
        sock_aio_alloc_sendmsg(find_item->sock_pair, expire_item->msg);
    }
    else
    {
        if (msg_baseinfo->expire > nng_clock())
        {
            nng_duration diff = (nng_duration)(msg_baseinfo->expire - nng_clock());
            diff = PROXY_MAX_TIMEOUT > diff ? diff : PROXY_MAX_TIMEOUT;
            nng_sleep_aio(diff, expire_item->time_aio);
            return;
        }
        LOG_PRINT_ERROR("proxy req src_id[%u] to dest_id[%u] msg_id[%u] timeout",
                        msg_baseinfo->src_id, msg_baseinfo->dest_id, msg_baseinfo->msg_id);

        send_msg_baseinfo.tag = D_IPC_NNG_LONG_MSG_TAG;
        send_msg_baseinfo.src_id = msg_baseinfo->src_id;
        send_msg_baseinfo.dest_id = msg_baseinfo->dest_id;
        send_msg_baseinfo.msg_id = msg_baseinfo->msg_id;
        send_msg_baseinfo.event_id = msg_baseinfo->event_id;
        send_msg_baseinfo.result = -8;
        switch (msg_baseinfo->msg_type)
        {
            case E_IPC_NNG_LONG_MSG_TYPE_REQ:
                send_msg_baseinfo.msg_type = E_IPC_NNG_LONG_MSG_TYPE_REP;
                break;
            case E_IPC_NNG_LONG_MSG_TYPE_ASYNC_REQ:
                send_msg_baseinfo.msg_type = E_IPC_NNG_LONG_MSG_TYPE_ASYNC_REP;
                break;
            case E_IPC_NNG_LONG_MSG_TYPE_STREAM_REQ:
                send_msg_baseinfo.msg_type = E_IPC_NNG_LONG_MSG_TYPE_STREAM_REP;
                break;
            default:
                send_msg_baseinfo.msg_type = E_IPC_NNG_LONG_MSG_TYPE_ERR;
                break;
        }
        sock_aio_alloc_send(expire_item->sock_pair, &send_msg_baseinfo, sizeof(send_msg_baseinfo));
        print_proxy_reg_map();
        nng_msg_free(expire_item->msg);
    }

    nng_aio_reap(expire_item->time_aio);
    delete expire_item;
    expire_item = NULL;
}

static void pair_recv_proxy_cb(void *arg)
{
    int ret = -1;
    ipc_nng_long_reg_item_t *reg_item = (ipc_nng_long_reg_item_t *)arg;
    ipc_nng_long_msg_baseinfo_t recv_msg_baseinfo = {};
    static uint32_t statistics_recv = 0;
    nng_msg *send_msg = NULL;
    nng_msg *recv_msg = NULL;

    ret = nng_aio_result(reg_item->recv_aio);
    if (0 != ret)
    {
        LOG_PRINT_ERROR("nng_aio_result fail, ret[%d]", ret);
        if (NNG_ECLOSED == ret || NNG_ECANCELED == ret || NNG_ESTOPPED == ret)
        {
        }
        else
        {
            nng_socket_recv(reg_item->sock_pair, reg_item->recv_aio);
        }
        return;
    }

    recv_msg = nng_aio_get_msg(reg_item->recv_aio);
    if (NULL == recv_msg)
    {
        LOG_PRINT_ERROR("nng_aio_get_msg fail");
        nng_socket_recv(reg_item->sock_pair, reg_item->recv_aio);
        return;
    }

    if (nng_msg_len(recv_msg) < sizeof(recv_msg_baseinfo))
    {
        LOG_PRINT_ERROR("nng_aio_get_msg fail");
        nng_msg_free(recv_msg);
        nng_aio_set_msg(reg_item->recv_aio, NULL);
        nng_socket_recv(reg_item->sock_pair, reg_item->recv_aio);
        return;
    }

    memcpy(&recv_msg_baseinfo, nng_msg_body(recv_msg), sizeof(recv_msg_baseinfo));
    if (D_IPC_NNG_LONG_MSG_TAG != recv_msg_baseinfo.tag)
    {
        LOG_PRINT_ERROR("invalid param!");
        nng_msg_free(recv_msg);
        nng_aio_set_msg(reg_item->recv_aio, NULL);
        nng_socket_recv(reg_item->sock_pair, reg_item->recv_aio);
        return;
    }

    ipc_nng_long_msg_baseinfo_t send_msg_baseinfo = {};

    switch (recv_msg_baseinfo.msg_type)
    {
        case E_IPC_NNG_LONG_MSG_TYPE_REGISTER:
            {
                send_msg_baseinfo.tag = D_IPC_NNG_LONG_MSG_TAG;
                send_msg_baseinfo.src_id = recv_msg_baseinfo.src_id;
                send_msg_baseinfo.dest_id = recv_msg_baseinfo.dest_id;
                send_msg_baseinfo.msg_id = recv_msg_baseinfo.msg_id;
                send_msg_baseinfo.event_id = recv_msg_baseinfo.event_id;
                send_msg_baseinfo.msg_type = E_IPC_NNG_LONG_MSG_TYPE_REGISTER;
                send_msg_baseinfo.expire = 0;
                send_msg_baseinfo.send_len = 0;
                send_msg_baseinfo.recv_len = 0;

                ipc_nng_long_reg_item_t *find_item = find_proxy_reg_item(recv_msg_baseinfo.src_id);
                if (NULL != find_item)
                {
                    send_msg_baseinfo.result = -3;
                    LOG_PRINT_ERROR("module_id[%d] already registered", recv_msg_baseinfo.src_id);
                }
                else
                {
                    send_msg_baseinfo.result = 0;
                    nng_mtx_lock(g_proxy_reg_map_mtx);
                    g_proxy_reg_map.insert({recv_msg_baseinfo.src_id, reg_item});
                    nng_mtx_unlock(g_proxy_reg_map_mtx);

                    nng_mtx_lock(reg_item->mtx);
                    reg_item->list.push_back(recv_msg_baseinfo.src_id);
                    nng_mtx_unlock(reg_item->mtx);
                }

                sock_aio_alloc_send(reg_item->sock_pair, &send_msg_baseinfo, sizeof(send_msg_baseinfo));
                LOG_PRINT_WARN("recv module_id[%u][%s] register req and send resp to it", recv_msg_baseinfo.src_id, reg_item->address);

                auto it = std::find(g_registered_module_array.begin(), g_registered_module_array.end(), recv_msg_baseinfo.src_id);
                if (it != g_registered_module_array.end())
                {
                    LOG_PRINT_WARN("module_id[%u] registered before", recv_msg_baseinfo.src_id);
                }
                else
                {
                    g_registered_module_array.push_back(recv_msg_baseinfo.src_id);
                    LOG_PRINT_WARN("module_id[%u]-registered_count[%zd] new register", recv_msg_baseinfo.src_id, g_registered_module_array.size());
                }
            }
            break;
        case E_IPC_NNG_LONG_MSG_TYPE_REQ:
        case E_IPC_NNG_LONG_MSG_TYPE_STREAM_REQ:
        case E_IPC_NNG_LONG_MSG_TYPE_ASYNC_REQ:
            {
                ipc_nng_long_reg_item_t *find_item = find_proxy_reg_item(recv_msg_baseinfo.dest_id);
                if (NULL != find_item)
                {
                    statistics_recv++;
                    nng_msg_dup(&send_msg, recv_msg);
                    LOG_PRINT_ERROR("sockid[%d] address[%s] len[%zd] statistics_recv[%u]", find_item->sock_pair.id,
                                    find_item->address, nng_msg_len(send_msg), statistics_recv);
                    sock_aio_alloc_sendmsg(find_item->sock_pair, send_msg);
                }
                else
                {
                    auto it = std::find(g_registered_module_array.begin(), g_registered_module_array.end(), recv_msg_baseinfo.dest_id);
                    if (it != g_registered_module_array.end())
                    {
                        LOG_PRINT_WARN("dest_id[%u] exited", recv_msg_baseinfo.dest_id);
                        send_msg_baseinfo.tag = D_IPC_NNG_LONG_MSG_TAG;
                        send_msg_baseinfo.src_id = recv_msg_baseinfo.src_id;
                        send_msg_baseinfo.dest_id = recv_msg_baseinfo.dest_id;
                        send_msg_baseinfo.msg_id = recv_msg_baseinfo.msg_id;
                        send_msg_baseinfo.event_id = recv_msg_baseinfo.event_id;
                        switch (recv_msg_baseinfo.msg_type)
                        {
                            case E_IPC_NNG_LONG_MSG_TYPE_REQ:
                                send_msg_baseinfo.msg_type = E_IPC_NNG_LONG_MSG_TYPE_REP;
                                break;
                            case E_IPC_NNG_LONG_MSG_TYPE_ASYNC_REQ:
                                send_msg_baseinfo.msg_type = E_IPC_NNG_LONG_MSG_TYPE_ASYNC_REP;
                                break;
                            case E_IPC_NNG_LONG_MSG_TYPE_STREAM_REQ:
                                send_msg_baseinfo.msg_type = E_IPC_NNG_LONG_MSG_TYPE_STREAM_REP;
                                break;
                            default:
                                send_msg_baseinfo.msg_type = E_IPC_NNG_LONG_MSG_TYPE_ERR;
                                break;
                        }
                        send_msg_baseinfo.expire = 0;
                        send_msg_baseinfo.result = -2;
                        send_msg_baseinfo.send_len = 0;
                        send_msg_baseinfo.recv_len = 0;
                        sock_aio_alloc_send(reg_item->sock_pair, &send_msg_baseinfo, sizeof(send_msg_baseinfo));
                    }
                    else
                    {
                        LOG_PRINT_WARN("dest_id[%u] not registered", recv_msg_baseinfo.dest_id);
                        if (recv_msg_baseinfo.expire - nng_clock() > PROXY_RETRY_DELAY)
                        {
                            ipc_nng_long_expire_item_t *expire_item = new ipc_nng_long_expire_item_t();
                            expire_item->sock_pair = nng_pipe_socket(nng_msg_get_pipe(recv_msg));
                            nng_msg_dup(&expire_item->msg, recv_msg);
                            ret = nng_aio_alloc(&expire_item->time_aio, proxy_expire_cb, expire_item);
                            if (0 != ret)
                            {
                                LOG_PRINT_ERROR("nng_aio_alloc fail");
                                break;
                            }
                            nng_sleep_aio(PROXY_RETRY_DELAY, expire_item->time_aio);
                        }
                        else
                        {
                            // ERROR
                            send_msg_baseinfo.tag = D_IPC_NNG_LONG_MSG_TAG;
                            send_msg_baseinfo.src_id = recv_msg_baseinfo.src_id;
                            send_msg_baseinfo.dest_id = recv_msg_baseinfo.dest_id;
                            send_msg_baseinfo.msg_id = recv_msg_baseinfo.msg_id;
                            send_msg_baseinfo.event_id = recv_msg_baseinfo.event_id;
                            switch (recv_msg_baseinfo.msg_type)
                            {
                                case E_IPC_NNG_LONG_MSG_TYPE_REQ:
                                    send_msg_baseinfo.msg_type = E_IPC_NNG_LONG_MSG_TYPE_REP;
                                    break;
                                case E_IPC_NNG_LONG_MSG_TYPE_ASYNC_REQ:
                                    send_msg_baseinfo.msg_type = E_IPC_NNG_LONG_MSG_TYPE_ASYNC_REP;
                                    break;
                                case E_IPC_NNG_LONG_MSG_TYPE_STREAM_REQ:
                                    send_msg_baseinfo.msg_type = E_IPC_NNG_LONG_MSG_TYPE_STREAM_REP;
                                    break;
                                default:
                                    send_msg_baseinfo.msg_type = E_IPC_NNG_LONG_MSG_TYPE_ERR;
                                    break;
                            }
                            send_msg_baseinfo.expire = 0;
                            send_msg_baseinfo.result = -2;
                            send_msg_baseinfo.send_len = 0;
                            send_msg_baseinfo.recv_len = 0;
                            sock_aio_alloc_send(reg_item->sock_pair, &send_msg_baseinfo, sizeof(send_msg_baseinfo));
                        }
                    }
                }
            }
            break;
        case E_IPC_NNG_LONG_MSG_TYPE_MULT_REQ:
            {
                statistics_recv++;
                ipc_nng_long_mcast_dest_t mcast_dest = {};
                uint8_t *body = (uint8_t *)nng_msg_body(recv_msg);
                memcpy(&mcast_dest, body + sizeof(recv_msg_baseinfo), sizeof(mcast_dest));
                for (uint32_t i = 0; i < mcast_dest.num; ++i)
                {
                    recv_msg_baseinfo.dest_id = mcast_dest.module_array[i];
                    nng_msg_dup(&send_msg, recv_msg);
                    LOG_PRINT_DEBUG("send multicast to [%d]", mcast_dest.module_array[i]);
                    if (D_IPC_NNG_LONG_CHECK_MODULE_ID_INVALID(mcast_dest.module_array[i]))
                    {
                        break;
                    }

                    ipc_nng_long_reg_item_t *find_item = find_proxy_reg_item(mcast_dest.module_array[i]);
                    if (NULL != find_item)
                    {
                        LOG_PRINT_DEBUG("sockid[%d], address[%s] len[%zd], statistics_recv[%d]", find_item->sock_pair.id, find_item->address, nng_msg_len(send_msg), statistics_recv);
                        sock_aio_alloc_sendmsg(find_item->sock_pair, send_msg);
                        LOG_PRINT_DEBUG("send multicast to [%d] end", mcast_dest.module_array[i]);
                    }
                    else
                    {
                        LOG_PRINT_ERROR("send multicast to [%d] item not found!", mcast_dest.module_array[i]);
                    }
                }

                send_msg_baseinfo.tag = D_IPC_NNG_LONG_MSG_TAG;
                send_msg_baseinfo.src_id = recv_msg_baseinfo.src_id;
                send_msg_baseinfo.msg_id = recv_msg_baseinfo.msg_id;
                send_msg_baseinfo.event_id = recv_msg_baseinfo.event_id;
                send_msg_baseinfo.msg_type = E_IPC_NNG_LONG_MSG_TYPE_REP;
                send_msg_baseinfo.expire = 0;
                send_msg_baseinfo.result = 0;
                send_msg_baseinfo.send_len = 0;
                send_msg_baseinfo.recv_len = 0;
                sock_aio_alloc_send(reg_item->sock_pair, &send_msg_baseinfo, sizeof(send_msg_baseinfo));
            }
            break;
        case E_IPC_NNG_LONG_MSG_TYPE_REP:
        case E_IPC_NNG_LONG_MSG_TYPE_STREAM_REP:
        case E_IPC_NNG_LONG_MSG_TYPE_ASYNC_REP:
            {
                ipc_nng_long_reg_item_t *find_item = find_proxy_reg_item(recv_msg_baseinfo.src_id);
                if (NULL != find_item)
                {
                    nng_msg_dup(&send_msg, recv_msg);
                    LOG_PRINT_DEBUG("sockid[%d], address[%s] len[%zd], statistics_recv[%d]", find_item->sock_pair.id, find_item->address, nng_msg_len(send_msg), statistics_recv);
                    sock_aio_alloc_sendmsg(find_item->sock_pair, send_msg);
                }
                else
                {
                    LOG_PRINT_ERROR("invalid src_id[%u]!", recv_msg_baseinfo.src_id);
                }
            }
            break;
        default:
            LOG_PRINT_ERROR("invalid msg_type[%d]", recv_msg_baseinfo.msg_type);
            break;
    }

    nng_msg_free(recv_msg);
    nng_aio_set_msg(reg_item->recv_aio, NULL);
    nng_socket_recv(reg_item->sock_pair, reg_item->recv_aio);
}

static void unreg_cb(void *arg)
{
    LOG_PRINT_WARN("unreg");
    ipc_nng_long_reg_item_t *reg_item = (ipc_nng_long_reg_item_t *)arg;
    if (NULL == reg_item || D_IPC_NNG_LONG_MSG_TAG != reg_item->tag)
    {
        return;
    }

    // clean reg_item
    LOG_PRINT_WARN("clean module_id[%u], closed[%d], tag[%x]", reg_item->module_id, reg_item->closed, reg_item->tag);
    if (reg_item->closed)
    {
        return;
    }

    reg_item->closed = true;
    reg_item->tag = 0;
    if (NULL != reg_item->recv_aio)
    {
        nng_aio_stop(reg_item->recv_aio);
        nng_aio_free(reg_item->recv_aio);
        reg_item->recv_aio = NULL;
    }
    nng_socket_close(reg_item->sock_pair);

    nng_mtx_lock(g_proxy_reg_map_mtx);
    nng_mtx_lock(reg_item->mtx);
    for (const auto &list_item : reg_item->list)
    {
        g_proxy_reg_map.erase(list_item);
    }
    reg_item->list.clear();
    nng_mtx_unlock(reg_item->mtx);
    nng_mtx_unlock(g_proxy_reg_map_mtx);

    nng_mtx_free(reg_item->mtx);
    nng_aio_reap(reg_item->unreg_aio);
    delete reg_item;
    LOG_PRINT_WARN("reg_item_clean exit");
}

static void pair_connect_cb(nng_pipe p, nng_pipe_ev ev, void *arg)
{
    (void)p;
    (void)ev;
    ipc_nng_long_reg_item_t *reg_item = (ipc_nng_long_reg_item_t *)arg;
    LOG_PRINT_WARN("client[0x%x] connect", reg_item->module_id);
}

static void pair_disconnect_cb(nng_pipe p, nng_pipe_ev ev, void *arg)
{
    (void)ev;
    int reason = 0;
    ipc_nng_long_reg_item_t *reg_item = (ipc_nng_long_reg_item_t *)arg;

    if (0 != nng_dialer_close(nng_pipe_dialer(p)))
    {
        LOG_PRINT_ERROR("nng_dialer_close fail!");
    }
    LOG_PRINT_WARN("client[0x%x] disconnect, reason[%d]", reg_item->module_id, reason);
    nng_sleep_aio(PROXY_RETRY_DELAY, reg_item->unreg_aio);
}

static void rep_sock_recv_cb(void *arg)
{
    int32_t ret = -1;
    ipc_nng_long_service_t *service = (ipc_nng_long_service_t *)arg;
    ipc_nng_long_msg_baseinfo_t send_msg_baseinfo = {};
    ipc_nng_long_msg_baseinfo_t recv_msg_baseinfo = {};
    nng_msg *recv_msg = NULL;
    nng_msg *pub_msg = NULL;

    ret = nng_aio_result(service->rep_recv_aio);
    if (0 != ret)
    {
        LOG_PRINT_ERROR("nng_aio_result fail, ret[%d]", ret);
        return;
    }

    recv_msg = nng_aio_get_msg(service->rep_recv_aio);
    if (NULL == recv_msg)
    {
        LOG_PRINT_ERROR("nng_aio_get_msg fail");
        return;
    }

    if (nng_msg_len(recv_msg) < sizeof(ipc_nng_long_msg_baseinfo_t))
    {
        LOG_PRINT_ERROR("invalid msg len[%zd]", nng_msg_len(recv_msg));
        nng_msg_free(recv_msg);
        nng_aio_set_msg(service->rep_recv_aio, NULL);
        return;
    }

    memcpy(&recv_msg_baseinfo, nng_msg_body(recv_msg), sizeof(recv_msg_baseinfo));
    send_msg_baseinfo.tag = D_IPC_NNG_LONG_MSG_TAG;
    send_msg_baseinfo.src_id = recv_msg_baseinfo.src_id;
    send_msg_baseinfo.dest_id = recv_msg_baseinfo.dest_id;
    send_msg_baseinfo.msg_id = recv_msg_baseinfo.msg_id;
    send_msg_baseinfo.event_id = recv_msg_baseinfo.event_id;
    send_msg_baseinfo.expire = 0;
    send_msg_baseinfo.result = 0;
    send_msg_baseinfo.send_len = 0;
    send_msg_baseinfo.recv_len = 0;

    do
    {
        if (recv_msg_baseinfo.msg_type == E_IPC_NNG_LONG_MSG_TYPE_CONN)
        {
            send_msg_baseinfo.msg_type = E_IPC_NNG_LONG_MSG_TYPE_CONN;
            ipc_nng_long_reg_item_t *reg_item = new ipc_nng_long_reg_item_t();
            if (NULL == reg_item)
            {
                LOG_PRINT_ERROR("new fail");
                break;
            }
            reg_item->tag = D_IPC_NNG_LONG_MSG_TAG;
            reg_item->module_id = recv_msg_baseinfo.src_id;

            ret = nng_mtx_alloc(&reg_item->mtx);
            if (0 != ret)
            {
                LOG_PRINT_ERROR("nng_mtx_alloc fail, ret[%d]", ret);
                break;
            }
            nng_mtx_lock(reg_item->mtx);
            reg_item->list.clear();
            nng_mtx_unlock(reg_item->mtx);

            ret = nng_pair0_open(&reg_item->sock_pair);
            if (0 != ret)
            {
                LOG_PRINT_ERROR("nng_pair0_open fail, ret[%d]", ret);
                break;
            }

            ret = nng_socket_set_ms(reg_item->sock_pair, NNG_OPT_SENDTIMEO, D_IPC_NNG_LONG_WAIT_TIMEOUT);
            if (0 != ret)
            {
                LOG_PRINT_ERROR("nng_socket_set_ms fail, ret[%d]", ret);
                return;
            }

            ret = nng_socket_set_ms(reg_item->sock_pair, NNG_OPT_RECVTIMEO, D_IPC_NNG_LONG_WAIT_CONTINUE);
            if (0 != ret)
            {
                LOG_PRINT_ERROR("nng_socket_set_ms fail, ret[%d]", ret);
                return;
            }

            ret = nng_aio_alloc(&reg_item->recv_aio, pair_recv_proxy_cb, reg_item);
            if (0 != ret)
            {
                LOG_PRINT_ERROR("nng_aio_alloc fail, ret[%d]", ret);
                break;
            }

            ret = nng_aio_alloc(&reg_item->unreg_aio, unreg_cb, reg_item);
            if (0 != ret)
            {
                LOG_PRINT_ERROR("nng_aio_alloc fail, ret[%d]", ret);
                break;
            }

            // 回调需要确保在dial前注册
            ret = nng_pipe_notify(reg_item->sock_pair, NNG_PIPE_EV_ADD_POST, pair_connect_cb, reg_item);
            if (0 != ret)
            {
                LOG_PRINT_ERROR("nng_pipe_notify fail, ret[%d]", ret);
                break;
            }

            ret = nng_pipe_notify(reg_item->sock_pair, NNG_PIPE_EV_REM_POST, pair_disconnect_cb, reg_item);
            if (0 != ret)
            {
                LOG_PRINT_ERROR("nng_pipe_notify fail, ret[%d]", ret);
                break;
            }

            memcpy(reg_item->address, (uint8_t *)nng_msg_body(recv_msg) + sizeof(recv_msg_baseinfo), recv_msg_baseinfo.send_len);
            LOG_PRINT_DEBUG("connsct address[%s]", reg_item->address);
            ret = ipc_init_dial(reg_item->sock_pair, reg_item->address);
            if (0 != ret)
            {
                LOG_PRINT_ERROR("ipc_init_dial fail, ret[%d]", ret);
                break;
            }

            nng_socket_recv(reg_item->sock_pair, reg_item->recv_aio);
        }
        else if (E_IPC_NNG_LONG_MSG_TYPE_BROADCAST == recv_msg_baseinfo.msg_type)
        {
            send_msg_baseinfo.msg_type = E_IPC_NNG_LONG_MSG_TYPE_BROADCAST;
            nng_msg_dup(&pub_msg, recv_msg);
            ret = nng_sendmsg(service->sock_pub, pub_msg, 0);
            if (0 != ret)
            {
                LOG_PRINT_ERROR("nng_sendmsg fail, ret[%d]", ret);
                break;
            }
        }
    } while (0);

    nng_msg_free(recv_msg);
    nng_aio_set_msg(service->rep_recv_aio, NULL);

    sock_aio_alloc_send(service->sock_rep, &send_msg_baseinfo, sizeof(send_msg_baseinfo));
    nng_socket_recv(service->sock_rep, service->rep_recv_aio);
}

static void rep_connect_cb(nng_pipe p, nng_pipe_ev ev, void *arg)
{
    (void)p;
    (void)ev;
    (void)arg;
    LOG_PRINT_DEBUG("CONNECT");
}

static void rep_disconnect_cb(nng_pipe p, nng_pipe_ev ev, void *arg)
{
    (void)p;
    (void)ev;
    (void)arg;
    LOG_PRINT_DEBUG("DISCONNECT");
}

static void broadcast_connect_cb(nng_pipe p, nng_pipe_ev ev, void *arg)
{
    (void)p;
    (void)ev;
    (void)arg;
    LOG_PRINT_DEBUG("CONNECT");
}

static void broadcast_disconnect_cb(nng_pipe p, nng_pipe_ev ev, void *arg)
{
    (void)p;
    (void)ev;
    (void)arg;
    LOG_PRINT_DEBUG("DISCONNECT");
}

void ipc_nng_long_daemon_proxy()
{
    int ret = -1;
    char rep_address[D_IPC_NNG_LONG_SOCKET_ADDRESS_LEN_MAX] = {};
    char broadcast_address[D_IPC_NNG_LONG_SOCKET_ADDRESS_LEN_MAX] = {};
    ipc_nng_long_service_t *service = new ipc_nng_long_service_t();

    nng_mtx_alloc(&g_proxy_reg_map_mtx);

    nng_mtx_lock(g_proxy_reg_map_mtx);
    g_proxy_reg_map.clear();
    nng_mtx_unlock(g_proxy_reg_map_mtx);

    service->name = "daemon_proxy";

    ret = nng_rep0_open(&service->sock_rep);
    if (0 != ret)
    {
        LOG_PRINT_ERROR("nng_rep0_open fail, ret[%d]!", ret);
        nng_mtx_free(g_proxy_reg_map_mtx);
        delete service;
        service = NULL;
        return;
    }

    ret = nng_pub0_open(&service->sock_pub);
    if (0 != ret)
    {
        LOG_PRINT_ERROR("nng_pub0_open fail, ret[%d]!", ret);
        nng_socket_close(service->sock_rep);
        nng_mtx_free(g_proxy_reg_map_mtx);
        delete service;
        service = NULL;
        return;
    }

    ret = nng_socket_set_ms(service->sock_rep, NNG_OPT_SENDTIMEO, D_IPC_NNG_LONG_WAIT_TIMEOUT);
    if (0 != ret)
    {
        LOG_PRINT_ERROR("nng_socket_set_ms fail, ret[%d]", ret);
        nng_socket_close(service->sock_pub);
        nng_socket_close(service->sock_rep);
        nng_mtx_free(g_proxy_reg_map_mtx);
        delete service;
        service = NULL;
        return;
    }

    // 回调需要确保在listen前注册
    nng_pipe_notify(service->sock_rep, NNG_PIPE_EV_ADD_POST, rep_connect_cb, &service->sock_rep);
    nng_pipe_notify(service->sock_rep, NNG_PIPE_EV_REM_POST, rep_disconnect_cb, &service->sock_rep);
    nng_pipe_notify(service->sock_pub, NNG_PIPE_EV_ADD_POST, broadcast_connect_cb, &service->sock_pub);
    nng_pipe_notify(service->sock_pub, NNG_PIPE_EV_REM_POST, broadcast_disconnect_cb, &service->sock_pub);

#ifdef NNG_USE_TCP
    snprintf(broadcast_address, sizeof(broadcast_address), D_IPC_NNG_LONG_BROADCAST_ADDRESS);
#else
    snprintf(broadcast_address, sizeof(broadcast_address), D_IPC_NNG_LONG_BROADCAST_ADDRESS, socket_prefix);
#endif

    ret = nng_listen(service->sock_pub, broadcast_address, NULL, 0);
    if (0 != ret)
    {
        if (ret == NNG_EADDRINUSE)
        {
            LOG_PRINT_ERROR("Another instance is already running.");
            exit(0);
        }
        LOG_PRINT_ERROR("nng_listen fail, ret[%d]", ret);
        nng_socket_close(service->sock_pub);
        nng_socket_close(service->sock_rep);
        nng_mtx_free(g_proxy_reg_map_mtx);
        delete service;
        service = NULL;
        return;
    }

#ifdef NNG_USE_TCP
    snprintf(rep_address, sizeof(rep_address), D_IPC_NNG_LONG_REP_ADDRESS);
#else
    snprintf(rep_address, sizeof(rep_address), D_IPC_NNG_LONG_REP_ADDRESS, socket_prefix);
#endif

    ret = nng_listen(service->sock_rep, rep_address, NULL, 0);
    if (0 != ret)
    {
        if (ret == NNG_EADDRINUSE)
        {
            LOG_PRINT_ERROR("Another instance is already running.");
            exit(0);
        }
        LOG_PRINT_ERROR("nng_listen fail, ret[%d]", ret);
        nng_socket_close(service->sock_pub);
        nng_socket_close(service->sock_rep);
        nng_mtx_free(g_proxy_reg_map_mtx);
        delete service;
        service = NULL;
        return;
    }

    ret = nng_aio_alloc(&service->rep_recv_aio, rep_sock_recv_cb, service);
    if (0 != ret)
    {
        LOG_PRINT_ERROR("nng_aio_alloc fail, ret[%d]", ret);
        nng_socket_close(service->sock_pub);
        nng_socket_close(service->sock_rep);
        nng_mtx_free(g_proxy_reg_map_mtx);
        delete service;
        return;
    }

    nng_socket_recv(service->sock_rep, service->rep_recv_aio);
}
