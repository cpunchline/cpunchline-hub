#include "ipc_nng_long_inn.hpp"
#include "ipc_nng_long_daemon.hpp"

static std::unordered_map<uint32_t, ipc_nng_long_map_item_t *> g_ipc_clients_map;
static ipc_nng_long_client_t *g_ipc_client = NULL;
static pthread_mutex_t g_init_ipc_mutex = PTHREAD_MUTEX_INITIALIZER;
static int32_t ipc_init_ret = -1;

static pthread_mutex_t mtx_msg_pair = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t bc_mutex = PTHREAD_MUTEX_INITIALIZER;
const char *socket_prefix = NULL;

static int32_t server_cb_recv(ipc_nng_long_work_t *work)
{
    int ret = -1;
    nng_msg *send_msg = NULL;
    nng_msg *recv_msg = NULL;
    size_t recv_msg_size = 0;
    ipc_nng_long_msg_baseinfo_t recv_msg_baseinfo = {};

    ret = nng_aio_result(work->aio);
    if (0 != ret)
    {
        LOG_PRINT_ERROR("nng_aio_result fail, ret[%d]", ret);
        return -1;
    }

    recv_msg = nng_aio_get_msg(work->aio);
    if (NULL == recv_msg)
    {
        LOG_PRINT_ERROR("nng_aio_get_msg fail");
        nng_socket_recv(work->sock, work->aio);
        return -1;
    }

    recv_msg_size = nng_msg_len(recv_msg);
    if (recv_msg_size < sizeof(ipc_nng_long_msg_baseinfo_t))
    {
        LOG_PRINT_ERROR("invalid recv_msg_size[%zd]", recv_msg_size);
        nng_aio_set_msg(work->aio, NULL);
        nng_msg_free(recv_msg);
        nng_socket_recv(work->sock, work->aio);
        return -1;
    }

    memcpy(&recv_msg_baseinfo, nng_msg_body(recv_msg), sizeof(recv_msg_baseinfo));
    if (D_IPC_NNG_LONG_MSG_TAG != recv_msg_baseinfo.tag)
    {
        LOG_PRINT_ERROR("invalid recv_msg_baseinfo.tag[%x]", recv_msg_baseinfo.tag);
        nng_aio_set_msg(work->aio, NULL);
        nng_msg_free(recv_msg);
        nng_socket_recv(work->sock, work->aio);
        return -1;
    }

    LOG_PRINT_DEBUG(
        "recv tag[%x], msg_type[%d], src_id[0x%x], dest_id[0x%x], msg_id[0x%x], "
        "event_id[%" PRIx64 "], msg_type[%d], expire[%" PRIx64 "], result[%d], send_len[%zd], recv_len[%zd]",
        recv_msg_baseinfo.tag,
        recv_msg_baseinfo.msg_type,
        recv_msg_baseinfo.src_id,
        recv_msg_baseinfo.dest_id,
        recv_msg_baseinfo.msg_id,
        recv_msg_baseinfo.event_id,
        recv_msg_baseinfo.msg_type,
        recv_msg_baseinfo.expire,
        recv_msg_baseinfo.result,
        recv_msg_baseinfo.send_len,
        recv_msg_baseinfo.recv_len);

    switch (recv_msg_baseinfo.msg_type)
    {
        case E_IPC_NNG_LONG_MSG_TYPE_REGISTER:
            {
                auto it = g_ipc_clients_map.find(recv_msg_baseinfo.src_id);
                if (g_ipc_clients_map.end() != it)
                {
                    ipc_nng_long_map_item_t *client_map_item = it->second;
                    size_t i = 0;
                    for (i = 0; i < D_IPC_NNG_LONG_MESSAGE_PAIR_COUNT; ++i)
                    {
                        if (client_map_item->msg_pair[i].event_id == recv_msg_baseinfo.event_id)
                        {
                            LOG_PRINT_DEBUG("recv register event_id[%" PRIx64 "] resp msg from ipc_daemon", recv_msg_baseinfo.event_id);
                            nng_mtx_lock(client_map_item->msg_pair[i].mtx);
                            client_map_item->msg_pair[i].ret = recv_msg_baseinfo.result;
                            nng_cv_wake1(client_map_item->msg_pair[i].cv);
                            nng_mtx_unlock(client_map_item->msg_pair[i].mtx);
                            break;
                        }
                    }

                    if (i >= D_IPC_NNG_LONG_MESSAGE_PAIR_COUNT)
                    {
                        LOG_PRINT_ERROR("event_id[%" PRIx64 "] nobody care", recv_msg_baseinfo.event_id);
                    }
                }
                else
                {
                    LOG_PRINT_ERROR("invalid src_id[0x%x]", recv_msg_baseinfo.src_id);
                }
            }
            break;
        case E_IPC_NNG_LONG_MSG_TYPE_REQ:
        case E_IPC_NNG_LONG_MSG_TYPE_ASYNC_REQ:
            {
                auto it = g_ipc_clients_map.find(recv_msg_baseinfo.dest_id);
                if (g_ipc_clients_map.end() != it)
                {
                    uint8_t *async_resp_data = NULL;
                    ipc_nng_long_map_item_t *client_map_item = it->second;
                    nng_time sync_start = nng_clock();

                    switch (recv_msg_baseinfo.msg_type)
                    {
                        case E_IPC_NNG_LONG_MSG_TYPE_REQ:
                            recv_msg_baseinfo.msg_type = E_IPC_NNG_LONG_MSG_TYPE_REP;
                            break;
                        case E_IPC_NNG_LONG_MSG_TYPE_ASYNC_REQ:
                            recv_msg_baseinfo.msg_type = E_IPC_NNG_LONG_MSG_TYPE_ASYNC_REP;
                            break;
                        default:
                            recv_msg_baseinfo.msg_type = E_IPC_NNG_LONG_MSG_TYPE_ERR;
                            break;
                    }

                    nng_msg_alloc(&send_msg, 0);
                    nng_msg_append(send_msg, &recv_msg_baseinfo, sizeof(recv_msg_baseinfo));
                    if (recv_msg_baseinfo.recv_len > 0)
                    {
                        async_resp_data = (uint8_t *)calloc(1, recv_msg_baseinfo.recv_len);
                        client_map_item->reg_info.response_handler(&recv_msg_baseinfo, (uint8_t *)nng_msg_body(recv_msg) + sizeof(recv_msg_baseinfo), async_resp_data);
                        nng_time sync_diff = nng_clock() - sync_start;
                        if (sync_diff > 300)
                        {
                            LOG_PRINT_WARN("[0x%x]->[0x%x] ipc handle callback takes time[%" PRIx64 "]ms",
                                           recv_msg_baseinfo.src_id, recv_msg_baseinfo.dest_id, sync_diff);
                        }
                        nng_msg_append(send_msg, async_resp_data, recv_msg_baseinfo.recv_len);
                        free(async_resp_data);
                    }
                    else
                    {
                        client_map_item->reg_info.response_handler(&recv_msg_baseinfo, (uint8_t *)nng_msg_body(recv_msg) + sizeof(recv_msg_baseinfo), NULL);
                    }

                    ret = sock_aio_alloc_sendmsg(work->sock, send_msg);
                    if (0 != ret)
                    {
                        LOG_PRINT_ERROR("sock_aio_alloc_sendmsg fail, ret[%d]", ret);
                    }
                }
                else
                {
                    LOG_PRINT_ERROR("invalid dest_id[0x%x]", recv_msg_baseinfo.dest_id);
                }
            }
            break;
        case E_IPC_NNG_LONG_MSG_TYPE_STREAM_REQ:
            {
                recv_msg_baseinfo.msg_type = E_IPC_NNG_LONG_MSG_TYPE_STREAM_REP;
                ret = sock_aio_alloc_send(work->sock, &recv_msg_baseinfo, sizeof(recv_msg_baseinfo));
                if (0 != ret)
                {
                    LOG_PRINT_ERROR("sock_aio_alloc_send fail, ret[%d]", ret);
                }

                auto it = g_ipc_clients_map.find(recv_msg_baseinfo.dest_id);
                if (g_ipc_clients_map.end() != it)
                {
                    ipc_nng_long_map_item_t *client_map_item = it->second;
                    nng_time stream_start = nng_clock();
                    client_map_item->reg_info.stream_handler(&recv_msg_baseinfo, (uint8_t *)nng_msg_body(recv_msg) + sizeof(recv_msg_baseinfo));
                    nng_time stream_diff = nng_clock() - stream_start;
                    if (stream_diff > 300)
                    {
                        LOG_PRINT_WARN("[0x%x]->[0x%x] ipc handle callback takes time[%" PRIx64 "]ms",
                                       recv_msg_baseinfo.src_id, recv_msg_baseinfo.dest_id, stream_diff);
                    }
                }
                else
                {
                    LOG_PRINT_ERROR("invalid dest_id[0x%x]", recv_msg_baseinfo.dest_id);
                }
            }
            break;
        case E_IPC_NNG_LONG_MSG_TYPE_MULT_REQ:
            {
                auto it = g_ipc_clients_map.find(recv_msg_baseinfo.dest_id);
                if (g_ipc_clients_map.end() != it)
                {
                    // NOTE sizeof(ipc_nng_long_mcast_dest_t) + data
                    ipc_nng_long_map_item_t *client_map_item = it->second;
                    client_map_item->reg_info.stream_handler(&recv_msg_baseinfo, (uint8_t *)nng_msg_body(recv_msg) + sizeof(recv_msg_baseinfo));
                }
                else
                {
                    LOG_PRINT_ERROR("invalid dest_id[0x%x]", recv_msg_baseinfo.dest_id);
                }
            }
            break;
        case E_IPC_NNG_LONG_MSG_TYPE_REP:
        case E_IPC_NNG_LONG_MSG_TYPE_STREAM_REP:
        case E_IPC_NNG_LONG_MSG_TYPE_MULT_REP:
            {
                auto it = g_ipc_clients_map.find(recv_msg_baseinfo.src_id);
                if (g_ipc_clients_map.end() != it)
                {
                    ipc_nng_long_map_item_t *client_map_item = it->second;
                    for (size_t i = 0; i < D_IPC_NNG_LONG_MESSAGE_PAIR_COUNT; ++i)
                    {
                        if (client_map_item->msg_pair[i].event_id == recv_msg_baseinfo.event_id)
                        {
                            nng_mtx_lock(client_map_item->msg_pair[i].mtx);
                            if (nng_clock() > client_map_item->msg_pair[i].expire)
                            {
                                LOG_PRINT_ERROR("recv rep timeout, event_id[0x%" PRIx64 "] [0x%" PRIx64 "] is_used[%s]",
                                                client_map_item->msg_pair[i].event_id,
                                                recv_msg_baseinfo.event_id,
                                                client_map_item->msg_pair[i].is_used ? "true" : "false");
                            }
                            client_map_item->msg_pair[i].ret = recv_msg_baseinfo.result;
                            if (0 == client_map_item->msg_pair[i].ret)
                            {
                                if (0 != client_map_item->msg_pair[i].out_data_size && client_map_item->msg_pair[i].out_data_size >= recv_msg_baseinfo.recv_len)
                                {
                                    if (NULL != client_map_item->msg_pair[i].out_data)
                                    {
                                        memcpy(client_map_item->msg_pair[i].out_data, (uint8_t *)nng_msg_body(recv_msg) + sizeof(recv_msg_baseinfo), client_map_item->msg_pair[i].out_data_size);
                                    }
                                    else
                                    {
                                        LOG_PRINT_ERROR("sync resp not copy");
                                    }
                                }
                                else if (0 == client_map_item->msg_pair[i].out_data_size && 0 == recv_msg_baseinfo.recv_len)
                                {
                                    LOG_PRINT_DEBUG("no need resp");
                                }
                                else
                                {
                                    LOG_PRINT_ERROR("[%zu] > [%zd], not enough", client_map_item->msg_pair[i].out_data_size, recv_msg_baseinfo.recv_len);
                                }
                            }
                            client_map_item->msg_pair[i].cond_true = true;
                            nng_cv_wake1(client_map_item->msg_pair[i].cv);
                            nng_mtx_unlock(client_map_item->msg_pair[i].mtx);
                            break;
                        }
                        else
                        {
                            LOG_PRINT_ERROR("event_id[%" PRIx64 "] nobody care", recv_msg_baseinfo.event_id);
                        }
                    }
                }
                else
                {
                    LOG_PRINT_ERROR("invalid src_id[0x%x]", recv_msg_baseinfo.src_id);
                }
            }
            break;
        case E_IPC_NNG_LONG_MSG_TYPE_ASYNC_REP:
            {
                auto it = g_ipc_clients_map.find(recv_msg_baseinfo.src_id);
                if (g_ipc_clients_map.end() != it)
                {
                    ipc_nng_long_map_item_t *client_map_item = it->second;
                    for (size_t i = 0; i < D_IPC_NNG_LONG_MESSAGE_PAIR_COUNT; ++i)
                    {
                        if (client_map_item->msg_pair[i].event_id == recv_msg_baseinfo.event_id)
                        {
                            if (0 != client_map_item->msg_pair[i].out_data_size)
                            {
                                client_map_item->reg_info.async_handler(&recv_msg_baseinfo, (uint8_t *)nng_msg_body(recv_msg) + sizeof(recv_msg_baseinfo));
                                client_map_item->msg_pair[i].is_used = false;
                                client_map_item->msg_pair[i].event_id = 0;
                                client_map_item->msg_pair[i].expire = 0;
                            }
                            else
                            {
                                client_map_item->reg_info.async_handler(&recv_msg_baseinfo, NULL);
                                client_map_item->msg_pair[i].is_used = false;
                                client_map_item->msg_pair[i].event_id = 0;
                                client_map_item->msg_pair[i].expire = 0;
                            }
                            nng_aio_cancel(client_map_item->msg_pair[i].timer_aio);
                            break;
                        }
                        else
                        {
                            LOG_PRINT_ERROR("event_id[%" PRIx64 "] nobody care", recv_msg_baseinfo.event_id);
                        }
                    }
                }
                else
                {
                    LOG_PRINT_ERROR("invalid src_id[0x%x]", recv_msg_baseinfo.src_id);
                }
            }
            break;
        default:
            LOG_PRINT_ERROR("invalid msg_type[%d]!", recv_msg_baseinfo.msg_type);
            break;
    }

    nng_aio_set_msg(work->aio, NULL);
    nng_msg_free(recv_msg);
    nng_socket_recv(work->sock, work->aio);

    return 0;
}

static void server_cb(void *arg)
{
    ipc_nng_long_work_t *work = (ipc_nng_long_work_t *)arg;
    int ret = 0;

    switch (work->state)
    {
        case ipc_nng_long_work_t::INIT:
            work->state = ipc_nng_long_work_t::RECV;
            nng_socket_recv(work->sock, work->aio);
            break;
        case ipc_nng_long_work_t::RECV:
            server_cb_recv(work);
            break;
        case ipc_nng_long_work_t::WAIT:
            nng_aio_set_msg(work->aio, work->msg);
            work->msg = NULL;
            work->state = ipc_nng_long_work_t::SEND;
            nng_socket_send(work->sock, work->aio);
            break;
        case ipc_nng_long_work_t::SEND:
            ret = nng_aio_result(work->aio);
            if (0 != ret)
            {
                nng_msg_free(work->msg);
                nng_aio_set_msg(work->aio, NULL);
                LOG_PRINT_ERROR("nng_socket_send, ret[%d]", ret);
            }
            work->state = ipc_nng_long_work_t::RECV;
            nng_socket_recv(work->sock, work->aio);
            break;
        default:
            LOG_PRINT_ERROR("bad state %d!", work->state);
            break;
    }
}

static ipc_nng_long_work_t *alloc_work(nng_socket sock)
{
    ipc_nng_long_work_t *w = NULL;
    int ret = -1;

    w = (ipc_nng_long_work_t *)nng_alloc(sizeof(ipc_nng_long_work_t));
    if (NULL == w)
    {
        LOG_PRINT_ERROR("nng_alloc fail");
    }

    ret = nng_aio_alloc(&w->aio, server_cb, w);
    if (0 != ret)
    {
        LOG_PRINT_ERROR("nng_aio_alloc fail, ret[%d]", ret);
    }

    w->state = ipc_nng_long_work_t::INIT;
    w->sock = sock;

    return (w);
}

static void _ipc_nng_logger(nng_log_level level, nng_log_facility fac, const char *msgid, const char *msg)
{
    (void)fac;

    switch (level)
    {
        case NNG_LOG_ERR:
            LOG_PRINT_ERROR("[%s]: [%s]", msgid, msg);
            break;
        case NNG_LOG_WARN:
        case NNG_LOG_NOTICE:
            LOG_PRINT_WARN("[%s]: [%s]", msgid, msg);
            break;
        case NNG_LOG_INFO:
            LOG_PRINT_INFO("[%s]: [%s]", msgid, msg);
            break;
        case NNG_LOG_DEBUG:
            LOG_PRINT_DEBUG("[%s]: [%s]", msgid, msg);
            break;
        case NNG_LOG_NONE:
            break;
        default:
            break;
    }
}

static void send_request_async_timeout_handle(void *arg)
{
    int ret = 0;
    ipc_nng_long_message_pair_t *msg_pair = (ipc_nng_long_message_pair_t *)arg;

    ret = nng_aio_result(msg_pair->timer_aio);
    if (0 == ret)
    {
        // timeout
        if (msg_pair->is_used)
        {
            LOG_PRINT_ERROR("event_id[%" PRIx64 "], async timeout", msg_pair->event_id);
            msg_pair->is_used = false;
            msg_pair->event_id = 0;
            msg_pair->expire = 0;
        }
    }
    else
    {
        if (NNG_ECANCELED == ret)
        {
            // recv resp, then normal exit
            LOG_PRINT_DEBUG("event_id[%" PRIx64 "], async normal timeout", msg_pair->event_id);
            return;
        }

        LOG_PRINT_ERROR("nng_aio_result fail, ret[%d]", ret);
        return;
    }
}

void sub_sock_recv_cb(void *arg)
{
    ipc_nng_long_client_t *client = (ipc_nng_long_client_t *)arg;
    ipc_nng_long_msg_baseinfo_t msg_baseinfo = {};
    nng_msg *msg = NULL;
    int ret = 0;

    ret = nng_aio_result(client->sub_aio);
    if (0 != ret)
    {
        LOG_PRINT_ERROR("nng_aio_result fail, ret[%d]", ret);
        return;
    }

    msg = nng_aio_get_msg(client->sub_aio);
    if (NULL == msg)
    {
        LOG_PRINT_ERROR("nng_aio_get_msg fail");
        nng_aio_set_msg(client->sub_aio, NULL);
        return;
    }

    if (nng_msg_len(msg) < sizeof(ipc_nng_long_msg_baseinfo_t))
    {
        LOG_PRINT_ERROR("invalid msg len[%zd]", nng_msg_len(msg));
        nng_aio_set_msg(client->sub_aio, NULL);
        nng_msg_free(msg);
        return;
    }

    memcpy(&msg_baseinfo, nng_msg_body(msg), sizeof(msg_baseinfo));
    if (D_IPC_NNG_LONG_MSG_TAG != msg_baseinfo.tag)
    {
        LOG_PRINT_ERROR("invalid tag[%x]", msg_baseinfo.tag);
        nng_aio_set_msg(client->sub_aio, NULL);
        nng_msg_free(msg);
        return;
    }

    for (const auto &item : g_ipc_clients_map)
    {
        ipc_nng_long_map_item_t *map_item = item.second;
        if (NULL != map_item && NULL != map_item->reg_info.broadcast_handler)
        {
            map_item->reg_info.broadcast_handler(&msg_baseinfo, (uint8_t *)nng_msg_body(msg) + sizeof(msg_baseinfo));
        }
    }

    nng_aio_set_msg(client->sub_aio, NULL);
    nng_msg_free(msg);
    nng_socket_recv(client->sock_sub, client->sub_aio);

    LOG_PRINT_DEBUG("sub_sock_recv_cb exit");
}

void ipc_nng_long_finish()
{
    if (NULL == g_ipc_client)
    {
        return;
    }
    if (NULL != g_ipc_client->sub_aio)
    {
        nng_aio_stop(g_ipc_client->sub_aio);
        nng_aio_free(g_ipc_client->sub_aio);
        g_ipc_client->sub_aio = NULL;
    }
    nng_socket_close(g_ipc_client->sock_sub);
    nng_socket_close(g_ipc_client->sock_req);
    nng_socket_close(g_ipc_client->sock_pair);

    for (size_t i = 0; i < D_IPC_NNG_LONG_WORKS_COUNT; ++i)
    {
        if (NULL != g_ipc_client->works[i])
        {
            if (NULL != g_ipc_client->works[i]->aio)
            {
                nng_aio_stop(g_ipc_client->works[i]->aio);
                nng_aio_free(g_ipc_client->works[i]->aio);
                g_ipc_client->works[i]->aio = NULL;
                nng_free(g_ipc_client->works[i], sizeof(ipc_nng_long_work_t));
                g_ipc_client->works[i] = NULL;
            }
        }
    }

    delete g_ipc_client;
    g_ipc_client = NULL;

    LOG_PRINT_DEBUG("client count[%zd]", g_ipc_clients_map.size());
    for (const auto &item : g_ipc_clients_map)
    {
        ipc_nng_long_map_item_t *map_item = item.second;
        if (NULL != map_item)
        {
            for (size_t i = 0; i < D_IPC_NNG_LONG_MESSAGE_PAIR_COUNT; ++i)
            {
                nng_aio_stop(map_item->msg_pair[i].timer_aio);
                nng_aio_free(map_item->msg_pair[i].timer_aio);
                nng_mtx_free(map_item->msg_pair[i].mtx);
                nng_cv_free(map_item->msg_pair[i].cv);
            }
            delete map_item;
            map_item = NULL;
        }
    }

    g_ipc_clients_map.clear();

    nng_fini();
    // usleep(10 * 1000);
}

static ipc_nng_long_message_pair_t *ipc_nng_long_find_idle_message_pair(ipc_nng_long_message_pair_t *msg_pair)
{
    pthread_mutex_lock(&mtx_msg_pair);
    for (size_t i = 0; i < D_IPC_NNG_LONG_MESSAGE_PAIR_COUNT; ++i)
    {
        if (!msg_pair[i].is_used)
        {
            msg_pair->is_used = true;
            pthread_mutex_unlock(&mtx_msg_pair);
            return &msg_pair[i];
        }
    }

    pthread_mutex_unlock(&mtx_msg_pair);
    return NULL;
}

static int32_t _ipc_nng_long_init_reg(ipc_nng_long_msg_baseinfo_t *msg_baseinfo, ipc_nng_long_message_pair_t *msg_pair)
{
    int32_t ret = -1;
    int register_retry_count = 0;

    msg_pair->event_id = msg_baseinfo->event_id;
    msg_pair->ret = -1;

    nng_mtx_lock(msg_pair->mtx);
    do
    {
        ret = -1;
        ret = nng_send(g_ipc_client->sock_pair, msg_baseinfo, sizeof(ipc_nng_long_msg_baseinfo_t), 0);
        if (0 != ret)
        {
            LOG_PRINT_ERROR("nng_send fail, ret[%d]", ret);
            ret = -1;
            continue;
        }

        msg_pair->expire = nng_clock() + D_IPC_NNG_LONG_WAIT_TIMEOUT;
        ret = nng_cv_until(msg_pair->cv, msg_pair->expire);
        if (0 != ret)
        {
            LOG_PRINT_ERROR("nng_cv_until fail, ret[%d]", ret);
            ret = -8;
        }
        else
        {
            LOG_PRINT_DEBUG("cv_ret[%d], msg_pair->ret[%d]", ret, msg_pair->ret);
            // -3 mean already registered
            ret = (msg_pair->ret == -3) ? 0 : msg_pair->ret;
            break;
        }
    } while (register_retry_count++ < 3);
    nng_mtx_unlock(msg_pair->mtx);

    if (register_retry_count > 0)
    {
        LOG_PRINT_WARN("register to ipc_daemon, ret[%d], retry_cnt[%d]", ret, register_retry_count);
    }

    msg_pair->is_used = false;
    msg_pair->event_id = 0;
    msg_pair->expire = 0;

    return ret;
}

static int32_t _ipc_nng_long_init_conn(nng_socket sock, uint8_t *conn_buffer, size_t conn_buffer_len)
{
    int32_t ret = -1;
    int conn_retry_count = 0;
    unsigned int retry_delay = 1;
    do
    {
        ret = nng_send(sock, conn_buffer, conn_buffer_len, 0);
        if (0 != ret)
        {
            if (NNG_EAGAIN == ret || NNG_ETIMEDOUT == ret)
            {
                sleep(retry_delay);
                continue;
            }

            LOG_PRINT_ERROR("connect to ipc_daemon nng_send fail, ret[%d]", ret);
            return ret;
        }

        memset(conn_buffer, 0x00, conn_buffer_len);
        ret = nng_recv(sock, conn_buffer, &conn_buffer_len, 0);
        if (0 != ret)
        {
            LOG_PRINT_ERROR("connect to ipc_daemon nng_recv fail, ret[%d]", ret);
        }
        break;
    } while (conn_retry_count++ < 3);

    LOG_PRINT_DEBUG("connect to ipc_daemon end, retry_cnt[%d], ret[%d]", conn_retry_count, ret);

    return ret;
}

static void _ipc_nng_long_init_once()
{
    int32_t ret = -1;
    uint32_t i = 0;
    char pair_address[D_IPC_NNG_LONG_SOCKET_ADDRESS_LEN_MAX] = {};
    char rep_address[D_IPC_NNG_LONG_SOCKET_ADDRESS_LEN_MAX] = {};
    char broadcast_address[D_IPC_NNG_LONG_SOCKET_ADDRESS_LEN_MAX] = {};
    uint8_t conn_buffer[sizeof(ipc_nng_long_msg_baseinfo_t) + D_IPC_NNG_LONG_SOCKET_ADDRESS_LEN_MAX] = {};
    size_t conn_buffer_len = sizeof(conn_buffer);

    atexit(ipc_nng_long_finish);

    g_ipc_client = new ipc_nng_long_client_t();
    if (NULL == g_ipc_client)
    {
        LOG_PRINT_ERROR("new fail, errno[%d](%s)!", errno, strerror(errno));
        return;
    }

    LOG_PRINT_DEBUG("_ipc_nng_long_init_once START");

    do
    {
        ret = nng_sub0_open(&g_ipc_client->sock_sub);
        if (0 != ret)
        {
            LOG_PRINT_ERROR("nng_sub0_open fail, ret[%d]", ret);
            break;
        }
        ret = nng_aio_alloc(&g_ipc_client->sub_aio, sub_sock_recv_cb, g_ipc_client);
        if (0 != ret)
        {
            LOG_PRINT_ERROR("nng_aio_alloc fail, ret[%d]", ret);
            break;
        }

        ret = nng_sub0_socket_subscribe(g_ipc_client->sock_sub, "", 0);
        if (0 != ret)
        {
            LOG_PRINT_ERROR("nng_aio_alloc fail, ret[%d]", ret);
            break;
        }

        nng_time begin = nng_clock();
#ifdef NNG_USE_TCP
        snprintf(broadcast_address, sizeof(broadcast_address), D_IPC_NNG_LONG_BROADCAST_ADDRESS);
#else
        snprintf(broadcast_address, sizeof(broadcast_address), D_IPC_NNG_LONG_BROADCAST_ADDRESS, socket_prefix);
#endif
        ret = ipc_init_dial(g_ipc_client->sock_sub, broadcast_address);
        if (0 != ret)
        {
            LOG_PRINT_ERROR("ipc_init_dial fail, ret[%d]", ret);
            break;
        }
        nng_time sub_conn_time = nng_clock() - begin;

        nng_socket_recv(g_ipc_client->sock_sub, g_ipc_client->sub_aio);

        ret = nng_req0_open(&g_ipc_client->sock_req);
        if (0 != ret)
        {
            LOG_PRINT_ERROR("nng_req0_open fail, ret[%d]", ret);
            break;
        }

        // NNG_OPT_SENDTIMEO default -1
        // NNG_OPT_RECVTIMEO default -1
        ret = nng_socket_set_ms(g_ipc_client->sock_req, NNG_OPT_SENDTIMEO, D_IPC_NNG_LONG_WAIT_TIMEOUT);
        if (0 != ret)
        {
            LOG_PRINT_ERROR("nng_socket_set_ms fail, ret[%d]", ret);
            break;
        }
        ret = nng_socket_set_ms(g_ipc_client->sock_req, NNG_OPT_RECVTIMEO, D_IPC_NNG_LONG_WAIT_CONTINUE);
        if (0 != ret)
        {
            LOG_PRINT_ERROR("nng_socket_set_ms fail, ret[%d]", ret);
            break;
        }

        begin = nng_clock();
#ifdef NNG_USE_TCP
        snprintf(rep_address, sizeof(rep_address), D_IPC_NNG_LONG_REP_ADDRESS);
#else
        snprintf(rep_address, sizeof(rep_address), D_IPC_NNG_LONG_REP_ADDRESS, socket_prefix);
#endif
        ret = ipc_init_dial(g_ipc_client->sock_req, rep_address);
        if (0 != ret)
        {
            LOG_PRINT_ERROR("ipc_init_dial fail, ret[%d]", ret);
            break;
        }
        nng_time req_conn_time = nng_clock() - begin;

        LOG_PRINT_INFO("sub_conn_time[%" PRIu64 "]-req_conn_time[%" PRIu64 "]", sub_conn_time, req_conn_time);

        ret = nng_pair0_open(&g_ipc_client->sock_pair);
        if (0 != ret)
        {
            LOG_PRINT_ERROR("nng_pair0_open fail, ret[%d]", ret);
            break;
        }

        ret = nng_socket_set_ms(g_ipc_client->sock_pair, NNG_OPT_SENDTIMEO, D_IPC_NNG_LONG_WAIT_TIMEOUT);
        if (0 != ret)
        {
            LOG_PRINT_ERROR("nng_socket_set_ms fail, ret[%d]", ret);
            break;
        }

        uint32_t module_id = E_IPC_NNG_LONG_MODULE_ID_MAX;
        for (const auto &item : g_ipc_clients_map)
        {
            ipc_nng_long_map_item_t *map_item = item.second;
            if (NULL != map_item)
            {
                module_id = map_item->reg_info.module_id;
                break;
            }
        }

#ifdef NNG_USE_TCP
        snprintf(pair_address, sizeof(pair_address), D_IPC_NNG_LONG_PAIR_ADDRESS, (23000 + module_id));
#else
        snprintf(pair_address, sizeof(pair_address), D_IPC_NNG_LONG_PAIR_ADDRESS, socket_prefix, module_id);
#endif

        ret = nng_listen(g_ipc_client->sock_pair, pair_address, NULL, 0);
        if (0 != ret)
        {
            LOG_PRINT_ERROR("nng_listen fail, ret[%d]", ret);
            if (ret == NNG_EADDRINUSE)
            {
                LOG_PRINT_ERROR("Another instance is already running.");
                exit(0);
            }
            break;
        }

        for (i = 0; i < D_IPC_NNG_LONG_WORKS_COUNT; ++i)
        {
            g_ipc_client->works[i] = alloc_work(g_ipc_client->sock_pair);
        }

        for (i = 0; i < D_IPC_NNG_LONG_WORKS_COUNT; ++i)
        {
            server_cb(g_ipc_client->works[i]);
        }

        // FIRST clien -> daemon connect(req-rep);
        ipc_nng_long_msg_baseinfo_t *con_msg_baseinfo = (ipc_nng_long_msg_baseinfo_t *)conn_buffer;
        con_msg_baseinfo->tag = D_IPC_NNG_LONG_MSG_TAG;
        con_msg_baseinfo->src_id = module_id;
        con_msg_baseinfo->dest_id = E_IPC_NNG_LONG_MODULE_ID_DAEMON;
        con_msg_baseinfo->msg_id = E_IPC_NNG_LONG_MSG_ID_CONN;
        con_msg_baseinfo->event_id = 0;
        con_msg_baseinfo->msg_type = E_IPC_NNG_LONG_MSG_TYPE_CONN;
        con_msg_baseinfo->expire = 0;
        con_msg_baseinfo->result = 0;
        con_msg_baseinfo->send_len = D_IPC_NNG_LONG_SOCKET_ADDRESS_LEN_MAX;
        con_msg_baseinfo->recv_len = conn_buffer_len;
        memcpy(conn_buffer + sizeof(ipc_nng_long_msg_baseinfo_t), pair_address, strlen(pair_address));

        LOG_PRINT_DEBUG("connect to ipc_daemon start");
        ret = _ipc_nng_long_init_conn(g_ipc_client->sock_req, conn_buffer, conn_buffer_len);
        if (ret != 0)
        {
            LOG_PRINT_ERROR("connect to ipc_daemon fail");
            break;
        }

        ipc_init_ret = 0;
        return;
    } while (0);
    ipc_init_ret = -1;
}

int32_t _ipc_nng_long_init(ipc_nng_long_register_info_t *reg_info)
{
    static pthread_once_t init_once = PTHREAD_ONCE_INIT;
    static bool log_init = false;
    int32_t ret = 0;
    nng_err err = NNG_OK;

    get_nng_lib_version();

    if (NULL == reg_info)
    {
        LOG_PRINT_ERROR("invalid params!");
        return -1;
    }

    // nng_init_params init_params = {3, 4, 0, 0, 0, 0, 1};
    err = nng_init(NULL);
    if (NNG_OK != err)
    {
        LOG_PRINT_ERROR("nng_init fail[%s]", nng_strerror(err));
        return -1;
    }

    if (!log_init)
    {
        nng_log_set_logger(_ipc_nng_logger);
        nng_log_set_level(NNG_LOG_DEBUG);
        log_init = true;
    }

    char pair_address[D_IPC_NNG_LONG_SOCKET_ADDRESS_LEN_MAX] = {};

#ifdef NNG_USE_TCP
    snprintf(pair_address, sizeof(pair_address), D_IPC_NNG_LONG_PAIR_ADDRESS, (23000 + reg_info->module_id));
#else
    socket_prefix = getenv("XDG_RUNTIME_DIR");
    if (!socket_prefix || access(socket_prefix, W_OK) != 0)
    {
        socket_prefix = "/tmp";
    }
    snprintf(pair_address, sizeof(pair_address), D_IPC_NNG_LONG_PAIR_ADDRESS, socket_prefix, reg_info->module_id);
#endif

    if (E_IPC_NNG_LONG_MODULE_ID_DAEMON == reg_info->module_id)
    {
        ipc_nng_long_daemon_proxy();
        usleep(100 * 1000);
        return 0;
    }

    if (g_ipc_clients_map.end() != g_ipc_clients_map.find(reg_info->module_id))
    {
        LOG_PRINT_ERROR("module_id[%u] is exist, ipc inited!", reg_info->module_id);
        return -1;
    }

    ipc_nng_long_map_item_t *client_map_item = new ipc_nng_long_map_item_t();
    if (NULL == client_map_item)
    {
        LOG_PRINT_ERROR("new client_map_item fail, errno[%d](%s)!", errno, strerror(errno));
        return -1;
    }
    memcpy(&client_map_item->reg_info, reg_info, sizeof(ipc_nng_long_register_info_t));

    for (size_t i = 0; i < D_IPC_NNG_LONG_MESSAGE_PAIR_COUNT; ++i)
    {
        nng_mtx_alloc(&client_map_item->msg_pair[i].mtx);
        nng_cv_alloc(&client_map_item->msg_pair[i].cv, client_map_item->msg_pair[i].mtx);
        nng_aio_alloc(&client_map_item->msg_pair[i].timer_aio, send_request_async_timeout_handle, &client_map_item->msg_pair[i]);
    }

    g_ipc_clients_map.insert({reg_info->module_id, client_map_item});

    pthread_once(&init_once, _ipc_nng_long_init_once);
    if (0 != ipc_init_ret)
    {
        ret = ipc_init_ret;
        LOG_PRINT_ERROR("_ipc_nng_long_init_once fail, ret[%d]", ret);
        return ret;
    }

    // SECOND clien -> daemon register(pair);
    ipc_nng_long_msg_baseinfo_t reg_msg_baseinfo = {};
    reg_msg_baseinfo.tag = D_IPC_NNG_LONG_MSG_TAG;
    reg_msg_baseinfo.src_id = reg_info->module_id;
    reg_msg_baseinfo.dest_id = E_IPC_NNG_LONG_MODULE_ID_DAEMON;
    reg_msg_baseinfo.msg_id = E_IPC_NNG_LONG_MSG_ID_REGISTER;
    reg_msg_baseinfo.event_id = (uint64_t)reg_info->module_id << 48;
    reg_msg_baseinfo.msg_type = E_IPC_NNG_LONG_MSG_TYPE_REGISTER;
    reg_msg_baseinfo.expire = 0;
    reg_msg_baseinfo.result = 0;
    reg_msg_baseinfo.send_len = 0;
    reg_msg_baseinfo.recv_len = 0;

    LOG_PRINT_DEBUG("register to ipc_daemon start");
    ipc_nng_long_message_pair_t *msg_pair = ipc_nng_long_find_idle_message_pair(client_map_item->msg_pair);
    if (NULL == msg_pair)
    {
        LOG_PRINT_ERROR("no more message pair");
        return -1;
    }

    ret = _ipc_nng_long_init_reg(&reg_msg_baseinfo, msg_pair);
    if (ret != 0)
    {
        LOG_PRINT_ERROR("ipc init failed");
        return -1;
    }

    LOG_PRINT_WARN("module_id[%d] ipc init done.", reg_info->module_id);
    // usleep(100 * 1000);

    return ret;
}

int32_t ipc_nng_long_init(ipc_nng_long_register_info_t *reg_info)
{
    int32_t ret = -1;

    pthread_mutex_lock(&g_init_ipc_mutex);
    ret = _ipc_nng_long_init(reg_info);
    pthread_mutex_unlock(&g_init_ipc_mutex);
    usleep(200 * 1000);

    return ret;
}

int32_t ipc_nng_long_destroy(uint32_t module_id)
{
    if (D_IPC_NNG_LONG_CHECK_MODULE_ID_INVALID(module_id))
    {
        return -1;
    }
    return 0;
}

int32_t ipc_nng_long_send_notify(uint32_t src_id, uint32_t dest_id, uint32_t msg_id, const void *notify_data, size_t notify_data_len)
{
    D_IPC_NNG_LONG_CHECK_IPC_INIT();
    if (D_IPC_NNG_LONG_CHECK_MODULE_ID_INVALID(src_id) || D_IPC_NNG_LONG_CHECK_MODULE_ID_INVALID(dest_id) || src_id == dest_id)
    {
        LOG_PRINT_ERROR("invalid src_id[%u]->dest_id[%u]", src_id, dest_id);
        return -1;
    }

    auto it = g_ipc_clients_map.find(src_id);
    if (g_ipc_clients_map.end() == it)
    {
        LOG_PRINT_ERROR("src_id[%u] unregister", src_id);
        return -1;
    }

    ipc_nng_long_map_item_t *map_item = it->second;

#if D_IPC_NNG_LONG_EACH_SLEEP_100MS
    D_IPC_NNG_LONG_CHECK_DEST_SAME(map_item->dest, dest_id);
#endif
    D_IPC_NNG_LONG_NESTED_CALL(dest_id, msg_id);

    ipc_nng_long_message_pair_t *msg_pair = ipc_nng_long_find_idle_message_pair(map_item->msg_pair);
    if (NULL == msg_pair)
    {
        LOG_PRINT_ERROR("not find idle message pair");
        return -1;
    }

    size_t timeout = D_IPC_NNG_LONG_WAIT_TIMEOUT;
    uint64_t event_id = D_IPC_NNG_LONG_GENERATION_EVENT_ID(D_IPC_NNG_LONG_PROTOCOL_SYNC, src_id, dest_id, msg_id);
    LOG_PRINT_DEBUG("event_id[0x%" PRIx64 "] socket_id[%u]", event_id, g_ipc_client->sock_pair.id);

    int32_t ret = -1;
    nng_msg *send_msg = NULL;

    ipc_nng_long_msg_baseinfo_t msg_baseinfo = {};
    nng_time expire_used = {};
    nng_time expire = nng_clock() + timeout;

    msg_baseinfo.tag = D_IPC_NNG_LONG_MSG_TAG;
    msg_baseinfo.src_id = src_id;
    msg_baseinfo.dest_id = dest_id;
    msg_baseinfo.msg_id = msg_id;
    msg_baseinfo.event_id = event_id;
    msg_baseinfo.msg_type = E_IPC_NNG_LONG_MSG_TYPE_REQ;
    msg_baseinfo.expire = expire;
    msg_baseinfo.result = 0;
    msg_baseinfo.send_len = notify_data_len;
    msg_baseinfo.recv_len = 0;

    nng_mtx_lock(msg_pair->mtx);
    do
    {
        msg_pair->is_used = true;
        msg_pair->event_id = event_id;
        msg_pair->cond_true = false;
        msg_pair->expire = expire;
        msg_pair->out_data = NULL;
        msg_pair->out_data_size = msg_baseinfo.recv_len;
        msg_pair->ret = -1;

        nng_msg_alloc(&send_msg, 0);
        nng_msg_append(send_msg, &msg_baseinfo, sizeof(msg_baseinfo));
        if (NULL != notify_data)
        {
            nng_msg_append(send_msg, notify_data, msg_baseinfo.send_len);
        }

        ret = nng_sendmsg(g_ipc_client->sock_pair, send_msg, 0);
        if (0 != ret)
        {
            LOG_PRINT_ERROR("nng_sendmsg fail, ret[%d]", ret);
            nng_msg_free(send_msg);
        }

        expire_used = nng_clock() + timeout;

        if ((expire_used - expire) > D_IPC_NNG_LONG_SEND_BLOCK_TIMEOUT)
        {
            LOG_PRINT_WARN("nng send block [%zu]", expire_used - expire);
        }
        msg_pair->expire = expire_used;

        while ((!msg_pair->cond_true) && (msg_pair->expire))
        {
            ret = nng_cv_until(msg_pair->cv, msg_pair->expire);
            if (NNG_ETIMEDOUT == ret)
            {
                ret = -8;
            }
            else if (0 == ret)
            {
                ret = msg_pair->ret;
            }
            else
            {
                LOG_PRINT_ERROR("nng_cv_until fail, ret[%d]", ret);
            }
        }
    } while (0);

    msg_pair->is_used = false;
    msg_pair->event_id = 0;
    msg_pair->expire = 0;
    msg_pair->cond_true = false;
    nng_mtx_unlock(msg_pair->mtx);

    return ret;
}

int32_t ipc_nng_long_send_sync(uint32_t src_id, uint32_t dest_id, uint32_t msg_id, const void *sync_req_data, size_t sync_req_data_len, void *sync_resp_data, size_t *sync_resp_data_len, size_t sync_resp_data_max_len, size_t timeout)
{
    D_IPC_NNG_LONG_CHECK_IPC_INIT();
    if (D_IPC_NNG_LONG_CHECK_MODULE_ID_INVALID(src_id) || D_IPC_NNG_LONG_CHECK_MODULE_ID_INVALID(dest_id) || src_id == dest_id)
    {
        LOG_PRINT_ERROR("invalid src_id[%u]->dest_id[%u]", src_id, dest_id);
        return -1;
    }

    auto it = g_ipc_clients_map.find(src_id);
    if (g_ipc_clients_map.end() == it)
    {
        LOG_PRINT_ERROR("src_id[%u] unregister", src_id);
        return -1;
    }

    ipc_nng_long_map_item_t *map_item = it->second;

#if D_IPC_NNG_LONG_EACH_SLEEP_100MS
    D_IPC_NNG_LONG_CHECK_DEST_SAME(map_item->dest, dest_id);
#endif
    D_IPC_NNG_LONG_NESTED_CALL(dest_id, msg_id);

    ipc_nng_long_message_pair_t *msg_pair = ipc_nng_long_find_idle_message_pair(map_item->msg_pair);
    if (NULL == msg_pair)
    {
        LOG_PRINT_ERROR("not find idle message pair");
        return -1;
    }

    uint64_t event_id = D_IPC_NNG_LONG_GENERATION_EVENT_ID(D_IPC_NNG_LONG_PROTOCOL_SYNC, src_id, dest_id, msg_id);
    LOG_PRINT_DEBUG("event_id[0x%" PRIx64 "], timeout[%zd]ms, socket_id[%u]", event_id, timeout, g_ipc_client->sock_pair.id);

    int32_t ret = -1;
    nng_msg *send_msg = NULL;
    ipc_nng_long_msg_baseinfo_t msg_baseinfo = {};
    nng_time expire_used = {};
    nng_time expire = nng_clock() + timeout;

    msg_baseinfo.tag = D_IPC_NNG_LONG_MSG_TAG;
    msg_baseinfo.src_id = src_id;
    msg_baseinfo.dest_id = dest_id;
    msg_baseinfo.msg_id = msg_id;
    msg_baseinfo.event_id = event_id;
    msg_baseinfo.msg_type = E_IPC_NNG_LONG_MSG_TYPE_REQ;
    msg_baseinfo.expire = expire;
    msg_baseinfo.result = 0;
    msg_baseinfo.send_len = sync_req_data_len;
    msg_baseinfo.recv_len = sync_resp_data_max_len;

    nng_mtx_lock(msg_pair->mtx);
    do
    {
        msg_pair->is_used = true;
        msg_pair->event_id = event_id;
        msg_pair->cond_true = false;
        msg_pair->expire = expire;
        msg_pair->out_data = sync_resp_data;
        msg_pair->out_data_size = msg_baseinfo.recv_len;
        msg_pair->ret = -1;

        nng_msg_alloc(&send_msg, 0);
        nng_msg_append(send_msg, &msg_baseinfo, sizeof(msg_baseinfo));
        if (NULL != sync_req_data)
        {
            nng_msg_append(send_msg, sync_req_data, msg_baseinfo.send_len);
        }

        ret = nng_sendmsg(g_ipc_client->sock_pair, send_msg, 0);
        if (0 != ret)
        {
            LOG_PRINT_ERROR("nng_sendmsg fail, ret[%d]", ret);
            nng_msg_free(send_msg);
        }

        expire_used = nng_clock() + timeout;

        if ((expire_used - expire) > D_IPC_NNG_LONG_SEND_BLOCK_TIMEOUT)
        {
            LOG_PRINT_WARN("nng send block [%zu]", expire_used - expire);
        }
        msg_pair->expire = expire_used;

        while ((!msg_pair->cond_true) && (msg_pair->expire))
        {
            ret = nng_cv_until(msg_pair->cv, msg_pair->expire);
            if (NNG_ETIMEDOUT == ret)
            {
                ret = -8;
            }
            else if (0 == ret)
            {
                ret = msg_pair->ret;
                if (NULL != sync_resp_data_len)
                {
                    *sync_resp_data_len = msg_pair->out_data_size;
                }
            }
            else
            {
                LOG_PRINT_ERROR("nng_cv_until fail, ret[%d]", ret);
            }
        }
    } while (0);

    msg_pair->is_used = false;
    msg_pair->event_id = 0;
    msg_pair->expire = 0;
    msg_pair->cond_true = false;
    nng_mtx_unlock(msg_pair->mtx);

    return ret;
}

int32_t ipc_nng_long_send_async(uint32_t src_id, uint32_t dest_id, uint32_t msg_id, const void *async_req_data, size_t async_req_data_len, size_t async_resp_data_len, size_t timeout)
{
    D_IPC_NNG_LONG_CHECK_IPC_INIT();
    if (D_IPC_NNG_LONG_CHECK_MODULE_ID_INVALID(src_id) || D_IPC_NNG_LONG_CHECK_MODULE_ID_INVALID(dest_id) || src_id == dest_id)
    {
        LOG_PRINT_ERROR("invalid src_id[%u]->dest_id[%u]", src_id, dest_id);
        return -1;
    }

    auto it = g_ipc_clients_map.find(src_id);
    if (g_ipc_clients_map.end() == it)
    {
        LOG_PRINT_ERROR("src_id[%u] unregister", src_id);
        return -1;
    }

    ipc_nng_long_map_item_t *map_item = it->second;

#if D_IPC_NNG_LONG_EACH_SLEEP_100MS
    D_IPC_NNG_LONG_CHECK_DEST_SAME(map_item->dest, dest_id);
#endif
    D_IPC_NNG_LONG_NESTED_CALL(dest_id, msg_id);

    ipc_nng_long_message_pair_t *msg_pair = ipc_nng_long_find_idle_message_pair(map_item->msg_pair);
    if (NULL == msg_pair)
    {
        LOG_PRINT_ERROR("not find idle message pair");
        return -1;
    }

    uint64_t event_id = D_IPC_NNG_LONG_GENERATION_EVENT_ID(D_IPC_NNG_LONG_PROTOCOL_ASYNC, src_id, dest_id, msg_id);
    LOG_PRINT_DEBUG("event_id[0x%" PRIx64 "], timeout[%zd]ms, socket_id[%u]", event_id, timeout, g_ipc_client->sock_pair.id);

    int32_t ret = -1;
    nng_msg *send_msg = NULL;
    ipc_nng_long_msg_baseinfo_t msg_baseinfo = {};
    nng_time expire = nng_clock() + timeout;

    msg_baseinfo.tag = D_IPC_NNG_LONG_MSG_TAG;
    msg_baseinfo.src_id = src_id;
    msg_baseinfo.dest_id = dest_id;
    msg_baseinfo.msg_id = msg_id;
    msg_baseinfo.event_id = event_id;
    msg_baseinfo.msg_type = E_IPC_NNG_LONG_MSG_TYPE_ASYNC_REQ;
    msg_baseinfo.expire = expire;
    msg_baseinfo.result = 0;
    msg_baseinfo.send_len = async_req_data_len;
    msg_baseinfo.recv_len = async_resp_data_len;

    msg_pair->is_used = true;
    msg_pair->event_id = event_id;
    msg_pair->cond_true = false;
    msg_pair->expire = expire;
    msg_pair->out_data = NULL;
    msg_pair->out_data_size = msg_baseinfo.recv_len;
    msg_pair->ret = -1;

    nng_msg_alloc(&send_msg, 0);
    nng_msg_append(send_msg, &msg_baseinfo, sizeof(msg_baseinfo));
    if (NULL != async_req_data)
    {
        nng_msg_append(send_msg, async_req_data, msg_baseinfo.send_len);
    }

    nng_sleep_aio((nng_duration)timeout, msg_pair->timer_aio);
    ret = nng_sendmsg(g_ipc_client->sock_pair, send_msg, 0);
    if (0 != ret)
    {
        LOG_PRINT_ERROR("nng_sendmsg fail, ret[%d]", ret);
        nng_msg_free(send_msg);
    }

    return ret;
}

int32_t ipc_nng_long_send_stream(uint32_t src_id, uint32_t dest_id, uint32_t msg_id, const void *stream_data, size_t stream_data_len, size_t timeout)
{
    D_IPC_NNG_LONG_CHECK_IPC_INIT();
    if (D_IPC_NNG_LONG_CHECK_MODULE_ID_INVALID(src_id) || D_IPC_NNG_LONG_CHECK_MODULE_ID_INVALID(dest_id) || src_id == dest_id)
    {
        LOG_PRINT_ERROR("invalid src_id[%u]->dest_id[%u]", src_id, dest_id);
        return -1;
    }

    auto it = g_ipc_clients_map.find(src_id);
    if (g_ipc_clients_map.end() == it)
    {
        LOG_PRINT_ERROR("src_id[%u] unregister", src_id);
        return -1;
    }

    ipc_nng_long_map_item_t *map_item = it->second;

#ifdef D_IPC_NNG_LONG_EACH_SLEEP_100MS
    D_IPC_NNG_LONG_CHECK_DEST_SAME(map_item->dest, dest_id);
#endif
    D_IPC_NNG_LONG_NESTED_CALL(dest_id, msg_id);

    ipc_nng_long_message_pair_t *msg_pair = ipc_nng_long_find_idle_message_pair(map_item->msg_pair);
    if (NULL == msg_pair)
    {
        LOG_PRINT_ERROR("not find idle message pair");
        return -1;
    }

    uint64_t event_id = D_IPC_NNG_LONG_GENERATION_EVENT_ID(D_IPC_NNG_LONG_PROTOCOL_STREAM, src_id, dest_id, msg_id);
    LOG_PRINT_DEBUG("event_id[0x%" PRIx64 "], timeout[%zd]ms, socket_id[%u]", event_id, timeout, g_ipc_client->sock_pair.id);

    int32_t ret = -1;
    nng_msg *send_msg = NULL;
    ipc_nng_long_msg_baseinfo_t msg_baseinfo = {};
    nng_time expire_used = {};
    nng_time expire = nng_clock() + timeout;

    msg_baseinfo.tag = D_IPC_NNG_LONG_MSG_TAG;
    msg_baseinfo.src_id = src_id;
    msg_baseinfo.dest_id = dest_id;
    msg_baseinfo.msg_id = msg_id;
    msg_baseinfo.event_id = event_id;
    msg_baseinfo.msg_type = E_IPC_NNG_LONG_MSG_TYPE_STREAM_REQ;
    msg_baseinfo.expire = expire;
    msg_baseinfo.result = 0;
    msg_baseinfo.send_len = stream_data_len;
    msg_baseinfo.recv_len = 0;

    nng_mtx_lock(msg_pair->mtx);
    do
    {
        msg_pair->is_used = true;
        msg_pair->event_id = event_id;
        msg_pair->cond_true = false;
        msg_pair->out_data = NULL;
        msg_pair->out_data_size = 0;
        msg_pair->ret = -1;

        nng_msg_alloc(&send_msg, 0);
        nng_msg_append(send_msg, &msg_baseinfo, sizeof(msg_baseinfo));
        if (NULL != stream_data)
        {
            nng_msg_append(send_msg, stream_data, msg_baseinfo.send_len);
        }

        ret = nng_sendmsg(g_ipc_client->sock_pair, send_msg, 0);
        if (0 != ret)
        {
            LOG_PRINT_ERROR("nng_sendmsg fail, ret[%d]", ret);
            nng_msg_free(send_msg);
        }

        expire_used = nng_clock() + timeout;

        if ((expire_used - expire) > D_IPC_NNG_LONG_SEND_BLOCK_TIMEOUT)
        {
            LOG_PRINT_WARN("nng send block [%zu]", expire_used - expire);
        }
        msg_pair->expire = expire_used;

        while ((!msg_pair->cond_true) && (msg_pair->expire))
        {
            ret = nng_cv_until(msg_pair->cv, msg_pair->expire);
            if (NNG_ETIMEDOUT == ret)
            {
                ret = -8;
            }
            else if (0 == ret)
            {
                ret = msg_pair->ret;
            }
            else
            {
                LOG_PRINT_ERROR("nng_cv_until fail, ret[%d]", ret);
            }
        }
    } while (0);

    msg_pair->is_used = false;
    msg_pair->event_id = 0;
    msg_pair->expire = 0;
    msg_pair->cond_true = false;
    nng_mtx_unlock(msg_pair->mtx);

    return ret;
}

int32_t ipc_nng_long_send_broadcast(uint32_t src_id, uint32_t msg_id, const void *broadcast_data, size_t broadcast_data_len)
{
    D_IPC_NNG_LONG_CHECK_IPC_INIT();
    if (D_IPC_NNG_LONG_CHECK_MODULE_ID_INVALID(src_id))
    {
        LOG_PRINT_ERROR("invalid src_id[%u]", src_id);
        return -1;
    }

    auto it = g_ipc_clients_map.find(src_id);
    if (g_ipc_clients_map.end() == it)
    {
        LOG_PRINT_ERROR("src_id[%u] unregister", src_id);
        return -1;
    }

    pthread_mutex_lock(&bc_mutex);

    int32_t ret = -1;
    nng_msg *send_msg = NULL;
    ipc_nng_long_msg_baseinfo_t msg_baseinfo = {};

    msg_baseinfo.tag = D_IPC_NNG_LONG_MSG_TAG;
    msg_baseinfo.src_id = src_id;
    msg_baseinfo.msg_id = msg_id;
    msg_baseinfo.event_id = 0;
    msg_baseinfo.msg_type = E_IPC_NNG_LONG_MSG_TYPE_BROADCAST;
    msg_baseinfo.expire = 0;
    msg_baseinfo.result = 0;
    msg_baseinfo.send_len = broadcast_data_len;
    msg_baseinfo.recv_len = 0;

    nng_msg_alloc(&send_msg, 0);
    nng_msg_append(send_msg, &msg_baseinfo, sizeof(msg_baseinfo));
    if (NULL != broadcast_data)
    {
        nng_msg_append(send_msg, broadcast_data, msg_baseinfo.send_len);
    }

    ret = nng_sendmsg(g_ipc_client->sock_req, send_msg, 0);
    if (0 != ret)
    {
        LOG_PRINT_ERROR("nng_sendmsg fail, ret[%d]", ret);
        nng_msg_free(send_msg);
    }

    pthread_mutex_unlock(&bc_mutex);

    return ret;
}

int32_t ipc_nng_long_send_multicast(uint32_t src_id, const ipc_nng_long_mcast_dest_t *dest, uint32_t msg_id, const void *mcast_data, uint32_t mcast_data_len, size_t timeout)
{
    D_IPC_NNG_LONG_CHECK_IPC_INIT();
    if (D_IPC_NNG_LONG_CHECK_MODULE_ID_INVALID(src_id))
    {
        LOG_PRINT_ERROR("invalid src_id[%u]", src_id);
        return -1;
    }

    if (NULL == dest)
    {
        LOG_PRINT_ERROR("dest is NULL");
        return -1;
    }

    if (0 == dest->num || dest->num > E_IPC_NNG_LONG_MODULE_ID_MAX)
    {
        LOG_PRINT_ERROR("invalid dest->num[%u]", dest->num);
        return -1;
    }

    for (uint32_t i = 0; i < dest->num; ++i)
    {
        if (D_IPC_NNG_LONG_CHECK_MODULE_ID_INVALID(dest->module_array[i]))
        {
            LOG_PRINT_ERROR("invalid dest[%u]", dest->module_array[i]);
            return -1;
        }
    }

    auto it = g_ipc_clients_map.find(src_id);
    if (g_ipc_clients_map.end() == it)
    {
        LOG_PRINT_ERROR("src_id[%u] unregister", src_id);
        return -1;
    }

    ipc_nng_long_map_item_t *map_item = it->second;

    ipc_nng_long_message_pair_t *msg_pair = ipc_nng_long_find_idle_message_pair(map_item->msg_pair);
    if (NULL == msg_pair)
    {
        LOG_PRINT_ERROR("not find idle message pair");
        return -1;
    }

    uint64_t event_id = D_IPC_NNG_LONG_GENERATION_EVENT_ID(D_IPC_NNG_LONG_PROTOCOL_STREAM, src_id, E_IPC_NNG_LONG_MODULE_ID_MAX, msg_id);
    LOG_PRINT_DEBUG("event_id[0x%" PRIx64 "], timeout[%zd]ms, socket_id[%u]", event_id, timeout, g_ipc_client->sock_pair.id);

    int32_t ret = -1;
    nng_msg *send_msg = NULL;
    ipc_nng_long_msg_baseinfo_t msg_baseinfo = {};
    nng_time expire_used = {};
    nng_time expire = nng_clock() + timeout;

    msg_baseinfo.tag = D_IPC_NNG_LONG_MSG_TAG;
    msg_baseinfo.src_id = src_id;
    msg_baseinfo.dest_id = 0;
    msg_baseinfo.msg_id = msg_id;
    msg_baseinfo.event_id = event_id;
    msg_baseinfo.msg_type = E_IPC_NNG_LONG_MSG_TYPE_MULT_REQ;
    msg_baseinfo.expire = 0;
    msg_baseinfo.result = 0;
    msg_baseinfo.send_len = sizeof(ipc_nng_long_mcast_dest_t) + mcast_data_len;
    msg_baseinfo.recv_len = 0;

    nng_mtx_lock(msg_pair->mtx);
    do
    {
        msg_pair->is_used = true;
        msg_pair->event_id = event_id;
        msg_pair->cond_true = false;
        msg_pair->out_data = NULL;
        msg_pair->out_data_size = 0;
        msg_pair->ret = -1;

        nng_msg_alloc(&send_msg, 0);
        nng_msg_append(send_msg, &msg_baseinfo, sizeof(msg_baseinfo));
        nng_msg_append(send_msg, dest, sizeof(ipc_nng_long_mcast_dest_t));
        if (NULL != mcast_data)
        {
            nng_msg_append(send_msg, mcast_data, msg_baseinfo.send_len);
        }

        ret = nng_sendmsg(g_ipc_client->sock_pair, send_msg, 0);
        if (0 != ret)
        {
            LOG_PRINT_ERROR("nng_sendmsg fail, ret[%d]", ret);
            nng_msg_free(send_msg);
        }

        expire_used = nng_clock() + timeout;
        if ((expire_used - expire) > D_IPC_NNG_LONG_SEND_BLOCK_TIMEOUT)
        {
            LOG_PRINT_WARN("nng send block [%zu]", expire_used - expire);
        }
        msg_pair->expire = expire_used;

        while ((!msg_pair->cond_true) && (msg_pair->expire))
        {
            ret = nng_cv_until(msg_pair->cv, msg_pair->expire);
            if (NNG_ETIMEDOUT == ret)
            {
                ret = -8;
            }
            else if (0 == ret)
            {
                ret = msg_pair->ret;
            }
            else
            {
                LOG_PRINT_ERROR("nng_cv_until fail, ret[%d]", ret);
            }
        }
    } while (0);

    msg_pair->is_used = false;
    msg_pair->event_id = 0;
    msg_pair->expire = 0;
    msg_pair->cond_true = false;
    nng_mtx_unlock(msg_pair->mtx);

    return ret;
}
