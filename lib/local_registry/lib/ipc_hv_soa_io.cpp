#include "ipc_hv_soa_common.hpp"
#include <regex.h>

extern std::shared_ptr<ipc_hv_soa_client> g_client;

hloop_t *get_idle_loop()
{
    std::lock_guard<std::mutex> worker_lock(g_client->m_worker_mutex);
    static size_t s_cur_index = 0;
    if (++s_cur_index >= g_client->m_worker_loops.size())
    {
        s_cur_index = 0;
    }
    return g_client->m_worker_loops[s_cur_index % g_client->m_worker_loops.size()];
}

void on_close(hio_t *io)
{
    LOG_PRINT_DEBUG("on_close fd[%d] error[%d]", hio_fd(io), hio_error(io));

    if (nullptr != g_client->m_daemon_io && hio_id(io) == hio_id(g_client->m_daemon_io))
    {
        {
            std::unique_lock<std::mutex> daemo_lock(g_client->daemo_mutex);
            g_client->daemon_cond_ret = IPC_HV_SOA_COND_STATE_DISCONNECTED;
            g_client->m_daemon_io = nullptr;
            g_client->client_status = LOCAL_CLIENT_STATUS_OFFLINE;
            g_client->daemon_cond.notify_all();
        }

        {
            std::lock_guard<std::mutex> process_clients_map_lock(g_client->process_clients_map_mutex);
            g_client->process_clients_map.erase(g_client->client_id);
        }

        // todo reconnect;
        LOG_PRINT_ERROR("disconnect with daemon!");
    }
    else if (nullptr != g_client->m_listen_io && hio_id(io) == hio_id(g_client->m_listen_io))
    {
        LOG_PRINT_ERROR("listen socket closed!");
        g_client->m_listen_io = nullptr;
    }
    else
    {
        uint32_t client_id = (uint32_t)(uintptr_t)hevent_userdata(io);
        std::shared_ptr<ipc_hv_soa_process_client> client = find_process_client(client_id);
        if (nullptr == client)
        {
            LOG_PRINT_ERROR("invalid client[%d]!", client_id);
        }
        else
        {
            if ((nullptr != client->client_send_io && hio_id(io) == hio_id(client->client_send_io)))
            {
                LOG_PRINT_ERROR("disconnect with process client[%s] not send!", client->client_name.c_str());
                std::unique_lock<std::mutex> send_msg_lock(client->send_msg_mutex);
                client->send_msg_seqid = 0;
                client->send_msg_map.clear();
                client->send_msg_cond_ret = IPC_HV_SOA_COND_STATE_CONNECTED;
                client->client_send_io = nullptr;
                client->client_status = LOCAL_CLIENT_STATUS_OFFLINE;
                client->send_msg_cond.notify_all();
            }
            else if ((nullptr != client->client_recv_io && hio_id(io) == hio_id(client->client_recv_io)))
            {
                client->client_status = LOCAL_CLIENT_STATUS_OFFLINE;
                client->client_recv_io = nullptr;
                LOG_PRINT_ERROR("disconnect with process client[%s] not recv!", client->client_name.c_str());
            }
            else
            {
                client->client_status = LOCAL_CLIENT_STATUS_OFFLINE;
                LOG_PRINT_ERROR("disconnect with client[%s]!", client->client_name.c_str());
            }
        }
    }
}

void on_recv_daemon(hio_t *io, void *buf, int readbytes)
{
    LOG_PRINT_DEBUG("on_recv_daemon fd[%d], readbytes[%d]", hio_fd(io), readbytes);
    if (readbytes < (int)LOCAL_REGISTRY_MSG_HEADER_SIZE)
    {
        LOG_PRINT_ERROR("not a complete msg");
        return;
    }

    bool status = false;
    uint32_t real_readbytes = (uint32_t)((uint32_t)readbytes - LOCAL_REGISTRY_MSG_HEADER_SIZE);
    st_local_msg_header *recv_msg_header = (st_local_msg_header *)buf;
    if (real_readbytes != recv_msg_header->msg_len)
    {
        LOG_PRINT_ERROR("msg_len[%u] != real_readbytes[%u]", recv_msg_header->msg_len, real_readbytes);
        return;
    }

    LOG_PRINT_DEBUG("on_recv_daemon msg_id[%u], msg_len[%u]", recv_msg_header->msg_id, recv_msg_header->msg_len);

    char resp[LOCAL_REGISTRY_MSG_SIZE_MAX] = {};
    uint32_t resp_size = 0;
    int32_t ret = 0;
    uint8_t *buffer = (uint8_t *)buf + LOCAL_REGISTRY_MSG_HEADER_SIZE;
    pb_istream_t stream = pb_istream_from_buffer(buffer, recv_msg_header->msg_len);

    uint16_t module_id = (uint16_t)(recv_msg_header->msg_id >> 16);
    if (module_id != MODULE_ID_AUTOLIB_LOCAL_REGISTRY)
    {
        LOG_PRINT_ERROR("module_id[%u] != [%u]", module_id, MODULE_ID_AUTOLIB_LOCAL_REGISTRY);
        return;
    }
    switch (recv_msg_header->msg_id)
    {
        case LOCAL_REGISTRY_SERVICE_ID_METHOD_REGISTER_CLIENT:
            {
                resp_size = sizeof(st_register_client_resp);
                *(st_register_client_resp *)resp = st_register_client_resp_init_zero;
                status = pb_decode(&stream, st_register_client_resp_fields, resp);
                if (!status)
                {
                    LOG_PRINT_ERROR("pb_decode msg_id[%d] fail, error(%s)", recv_msg_header->msg_id, PB_GET_ERROR(&stream));
                    ret = -1;
                }
                else
                {
                    st_register_client_resp *p_resp = (st_register_client_resp *)resp;
                    LOG_PRINT_DEBUG("register client id client_id[%u], client_pid[%u]!", p_resp->client_id, p_resp->client_pid);
                    ret = 0;
                }
            }
            break;
        case LOCAL_REGISTRY_SERVICE_ID_METHOD_GET_CLIENT:
            {
                resp_size = sizeof(st_get_client_resp);
                *(st_get_client_resp *)resp = st_get_client_resp_init_zero;
                status = pb_decode(&stream, st_get_client_resp_fields, resp);
                if (!status)
                {
                    LOG_PRINT_ERROR("pb_decode msg_id[%d] fail, error(%s)", recv_msg_header->msg_id, PB_GET_ERROR(&stream));
                    ret = -1;
                }
                else
                {
                    st_get_client_resp *p_resp = (st_get_client_resp *)resp;
                    if (p_resp->has_responser_client)
                    {
                        LOG_PRINT_DEBUG("get_client resp client_id[%u], client_name[%s]!", p_resp->responser_client.client_id, p_resp->responser_client.client_name);
                    }
                    ret = 0;
                }
            }
            break;
        case LOCAL_REGISTRY_SERVICE_ID_METHOD_GET_SERVICE:
            {
                resp_size = sizeof(st_get_service_resp);
                *(st_get_service_resp *)resp = st_get_service_resp_init_zero;
                status = pb_decode(&stream, st_get_service_resp_fields, resp);
                if (!status)
                {
                    LOG_PRINT_ERROR("pb_decode msg_id[%d] fail, error(%s)", recv_msg_header->msg_id, PB_GET_ERROR(&stream));
                    ret = -1;
                }
                else
                {
                    ret = 0;
                }
            }
            break;
        case LOCAL_REGISTRY_SERVICE_ID_METHOD_SERVICE_CHANGE_STATUS:
            {
                resp_size = sizeof(st_service_change_status);
                *(st_service_change_status *)resp = st_service_change_status_init_zero;
                status = pb_decode(&stream, st_service_change_status_fields, resp);
                if (!status)
                {
                    LOG_PRINT_ERROR("pb_decode msg_id[%d] fail, error(%s)", recv_msg_header->msg_id, PB_GET_ERROR(&stream));
                }
                else
                {
                    st_service_change_status *change_status = (st_service_change_status *)resp;
                    if (change_status->has_service)
                    {
                        LOG_PRINT_DEBUG("service change status service_id[%u]-service_status[%d]",
                                        change_status->service.service_id, change_status->service.service_status);
                        auto it = find_service(change_status->service.service_id);
                        if (nullptr == it)
                        {
                            LOG_PRINT_ERROR("service_id[%u] not found", change_status->service.service_id);
                        }
                        else
                        {
                            std::shared_ptr<ipc_hv_soa_process_client> client = nullptr;
                            it->service_status = change_status->service.service_status;
                            if (change_status->has_provider_client)
                            {
                                LOG_PRINT_DEBUG("service change status provider client_id[%u]-client_name[%s]-client_status[%d]",
                                                change_status->provider_client.client_id,
                                                change_status->provider_client.client_name,
                                                change_status->provider_client.client_status);
                                auto it1 = find_process_client(change_status->provider_client.client_id);
                                if (nullptr == it1)
                                {
                                    client = save_process_client(change_status->provider_client.client_id, change_status->provider_client.client_name);
                                }
                                else
                                {
                                    client = it1;
                                }
                                it->service_provider = client;
                            }

                            if (E_IPC_HV_SOA_SERVICE_TYPE_METHOD == it->service_type && nullptr != it->service_handler)
                            {
                                PF_IPC_HV_SOA_SERVICE_STATUS_CB cb = (PF_IPC_HV_SOA_SERVICE_STATUS_CB)it->service_handler;
                                cb(it->service_id, change_status->service.service_status);
                            }
                        }
                    }
                    else
                    {
                        LOG_PRINT_ERROR("service change status no service");
                    }
                }
            }
            return;
            // break;
        case LOCAL_REGISTRY_SERVICE_ID_METHOD_LISTENER_CHANGE_TO_PROVIDER:
            {
                resp_size = sizeof(st_listener_change_to_provider);
                *(st_listener_change_to_provider *)resp = st_listener_change_to_provider_init_zero;
                status = pb_decode(&stream, st_listener_change_to_provider_fields, resp);
                if (!status)
                {
                    LOG_PRINT_ERROR("pb_decode msg_id[%d] fail, error(%s)", recv_msg_header->msg_id, PB_GET_ERROR(&stream));
                }
                else
                {
                    std::shared_ptr<ipc_hv_soa_process_client> client = nullptr;
                    st_listener_change_to_provider *event_listen = (st_listener_change_to_provider *)resp;

                    LOG_PRINT_DEBUG("service_id[%u]-listener_clients_count[%u]-reg[%s]",
                                    event_listen->service_id, event_listen->listener_clients_count, event_listen->reg ? "true" : "false");
                    for (size_t i = 0; i < event_listen->listener_clients_count; i++)
                    {
                        LOG_PRINT_DEBUG("listener_client client_id[%u]", event_listen->listener_clients[i].client_id);
                    }

                    auto it = find_service(event_listen->service_id);
                    if (nullptr == it)
                    {
                        LOG_PRINT_ERROR("service_id[%u] not found", event_listen->service_id);
                    }
                    else
                    {
                        uint32_t i = 0;
                        for (i = 0; i < event_listen->listener_clients_count; i++)
                        {
                            st_local_client_item *p_client = &(event_listen->listener_clients[i]);
                            auto it1 = find_process_client(p_client->client_id);
                            if (nullptr == it1)
                            {
                                client = save_process_client(p_client->client_id, p_client->client_name);
                            }
                            else
                            {
                                client = it1;
                            }

                            if (event_listen->reg)
                            {
                                auto it2 = it->service_listeners.find(client->client_id);
                                if (it->service_listeners.end() == it2)
                                {
                                    it->service_listeners.insert({client->client_id, client});
                                }

                                if (it->service_status == E_IPC_HV_SOA_SERVICE_TYPE_EVENT && nullptr != it->service_handler)
                                {
                                    void *event_data = nullptr;
                                    uint32_t event_data_len = 0;
                                    // new listener, it should send the first once trigger to listener
                                    PF_IPC_HV_SOA_EVENT_LISTEN_CB listener_cb = (PF_IPC_HV_SOA_EVENT_LISTEN_CB)it->service_handler;
                                    if (IPC_HV_SOA_RET_SUCCESS == listener_cb(it->service_id, &event_data, &event_data_len))
                                    {
                                        ret = ipc_hv_soa_inn_trigger_to_client(it, event_data, event_data_len, client);
                                        if (IPC_HV_SOA_RET_SUCCESS != ret)
                                        {
                                            LOG_PRINT_ERROR("ipc_hv_soa_inn_trigger_to_client fail, ret[%d]!", ret);
                                        }
                                    }
                                }
                            }
                            else
                            {
                                it->service_listeners.erase(client->client_id);
                                break;
                            }
                        }
                    }
                }
            }
            return;
            // break;
        default:
            break;
    }

    std::lock_guard<std::mutex> daemon_lock{g_client->daemo_mutex};
    g_client->daemon_cond_msgid = recv_msg_header->msg_id;
    memcpy(g_client->daemon_cond_msgdata, resp, resp_size);
    g_client->daemon_cond_ret = ret;
    g_client->daemon_cond.notify_one();
}

void on_recv_process(hio_t *io, void *buf, int readbytes)
{
    uint8_t *buffer = (uint8_t *)buf;
    LOG_PRINT_DEBUG("on_recv_process fd[%d], readbytes[%d]", hio_fd(io), readbytes);
    if (readbytes < (int)LOCAL_REGISTRY_MSG_HEADER_SIZE)
    {
        LOG_PRINT_ERROR("not a complete msg");
        return;
    }

    uint32_t real_readbytes = (uint32_t)((uint32_t)readbytes - LOCAL_REGISTRY_MSG_HEADER_SIZE);
    uint32_t client_id, msg_seqid, msg_type, service_id, msgdata_len;
    memcpy(&client_id, buffer + 0 * sizeof(uint32_t), sizeof(uint32_t));
    memcpy(&msg_seqid, buffer + 1 * sizeof(uint32_t), sizeof(uint32_t));
    memcpy(&msg_type, buffer + 2 * sizeof(uint32_t), sizeof(uint32_t));
    memcpy(&service_id, buffer + 3 * sizeof(uint32_t), sizeof(uint32_t));
    memcpy(&msgdata_len, buffer + 4 * sizeof(uint32_t), sizeof(uint32_t));
    if (real_readbytes != msgdata_len)
    {
        LOG_PRINT_ERROR("msg_len[%u] != real_readbytes[%u]", msgdata_len, real_readbytes);
        return;
    }

    LOG_PRINT_DEBUG("on_recv_process service_id[%u], msg_type[%u], msg_seqid[%d], msg_len[%u]", service_id, msg_type, msg_seqid, msgdata_len);

    uint8_t pstruct[LOCAL_REGISTRY_MSG_SIZE_MAX] = {};
#if NANOPB_SUPPORT_OPTION
    const pb_msgdesc_t *fields = nullptr;
    uint32_t fields_size = 0;

    uint16_t module_id = (uint16_t)(service_id >> 16);
    uint16_t msg_id = (uint16_t)(service_id & 0xFFFF);
    const st_autolib_servicemap *pmap = gst_autolib_servicemap[module_id];
    if (nullptr == pmap)
    {
        LOG_PRINT_ERROR("module_id[%u] not found", module_id);
        return;
    }
    const st_autolib_servicemap_item *pitem = &pmap->items[msg_id - 1];
    if (nullptr == pitem)
    {
        LOG_PRINT_ERROR("service_id[%u] not found", service_id);
        return;
    }

    if (msg_type == E_IPC_HV_SOA_MSG_TYPE_METHOD_RESPONSE_SYNC || msg_type == E_IPC_HV_SOA_MSG_TYPE_METHOD_RESPONSE_ASYNC)
    {
        fields = pitem->out_fields;
        fields_size = pitem->out_size;
    }
    else
    {
        fields = pitem->in_fields;
        fields_size = pitem->in_size;
    }

    if (msgdata_len > 0)
    {
        if (fields_size == 0)
        {
            LOG_PRINT_ERROR("msgdata_len[%u] > 0, but fields_size[%u] = 0", msgdata_len, fields_size);
            return;
        }

        pb_istream_t stream = pb_istream_from_buffer(buffer + LOCAL_REGISTRY_MSG_HEADER_SIZE, msgdata_len);
        bool status = pb_decode(&stream, fields, pstruct);
        if (!status)
        {
            LOG_PRINT_ERROR("pb_decode service_id[%d] fail, error(%s)", service_id, PB_GET_ERROR(&stream));
            return;
        }
        LOG_PRINT_DEBUG("pb_decode service_id[%d] success", service_id);
    }
#endif

    if (msg_type != E_IPC_HV_SOA_MSG_TYPE_METHOD_RESPONSE_SYNC)
    {
        ipc_hv_soa_process_client_data client_data = {};
        client_data.service_id = service_id;
        client_data.msg_type = msg_type;
        client_data.msg_seqid = msg_seqid;
        client_data.client_id = client_id;

        if (msgdata_len > 0)
        {
#if NANOPB_SUPPORT_OPTION
            client_data.data_len = fields_size;
            memcpy(client_data.data, pstruct, fields_size);
#else
            client_data.data_len = msgdata_len;
            memcpy(client_data.data, buffer + LOCAL_REGISTRY_MSG_HEADER_SIZE, msgdata_len);
#endif
        }
        else
        {
            client_data.data_len = 0;
            memset(client_data.data, 0x00, sizeof(client_data.data));
        }
        g_client->msg_handler_queue.Push(client_data);
    }
    else
    {
#if NANOPB_SUPPORT_OPTION
        ipc_hv_soa_inn_sync_complete(service_id, msg_seqid, pstruct, msgdata_len);
#else
        if (msgdata_len > 0)
        {
            memset(pstruct, 0x00, msgdata_len);
            memcpy(pstruct, buffer + LOCAL_REGISTRY_MSG_HEADER_SIZE, msgdata_len);
        }
        ipc_hv_soa_inn_sync_complete(service_id, msg_seqid, pstruct, msgdata_len);
#endif
    }
}

void on_write(hio_t *io, const void *buf, int writebytes)
{
    (void)buf;
    if (!hio_write_is_complete(io))
    {
        return;
    }

    LOG_PRINT_DEBUG("on_write fd[%d], writebytes[%d]", hio_fd(io), writebytes);
    if (nullptr != g_client->m_daemon_io && hio_id(io) == hio_id(g_client->m_daemon_io))
    {
        hio_setcb_read(io, on_recv_daemon);
        hio_set_unpack(io, &g_client->unpack_setting);
    }
    else
    {
        hio_setcb_read(io, on_recv_process);
        hio_set_unpack(io, &g_client->unpack_setting);
    }

    hio_read(io);
}

void on_connect(hio_t *io)
{
    LOG_PRINT_DEBUG("on_connect fd[%d]", hio_fd(io));
    if (hio_is_connected(io))
    {
        hio_setcb_write(io, on_write);
        if (nullptr != g_client->m_daemon_io && hio_id(io) == hio_id(g_client->m_daemon_io))
        {
            if (nullptr == g_client->m_daemon_io)
            {
                return;
            }
            LOG_PRINT_DEBUG("connected with daemon!");
            std::unique_lock daemon_lock(g_client->daemo_mutex);
            g_client->daemon_cond_msgid = 0;
            memset(g_client->daemon_cond_msgdata, 0x00, sizeof(g_client->daemon_cond_msgdata));
            g_client->daemon_cond_ret = IPC_HV_SOA_COND_STATE_CONNECTED;
            g_client->daemon_cond.notify_one();
        }
        else
        {
            uint32_t client_id = (uint32_t)(uintptr_t)hevent_userdata(io);
            std::shared_ptr<ipc_hv_soa_process_client> client = find_process_client(client_id);
            if (nullptr == client)
            {
                LOG_PRINT_ERROR("invalid client[%d]!", client_id);
            }
            else
            {
                if (nullptr != client->client_send_io && hio_id(io) == hio_id(client->client_send_io))
                {
                    LOG_PRINT_INFO("connected with process client[%s]!", client->client_name.c_str());
                    std::unique_lock<std::mutex> send_msg_lock(client->send_msg_mutex);
                    client->send_msg_seqid = 0;
                    client->send_msg_map.clear();
                    client->send_msg_cond_ret = IPC_HV_SOA_COND_STATE_CONNECTED;
                    client->send_msg_cond.notify_one();
                }
            }
        }
    }
    else
    {
    }
}

void on_post_event_cb(hevent_t *ev)
{
    hloop_t *loop = ev->loop;
    hio_t *io = (hio_t *)hevent_userdata(ev);
    hio_attach(loop, io);

    hio_setcb_close(io, on_close);
    hio_setcb_read(io, on_recv_process);
    hio_set_unpack(io, &g_client->unpack_setting);
    hio_read(io);
}

static bool parse_ipc(char *s, uint32_t *src, char *name, uint32_t *tgt)
{
    char *p2 = strrchr(s, '-');
    if (!p2 || strncmp(p2, "-2.ipc", 6))
        return false;

    char *p1 = p2 - 1;
    while (p1 > s && *p1 != '-')
        p1--;
    if (p1 == s)
    {
        return false;
    }

    const char *start = strrchr(s, '/');
    start = start ? start + 1 : s;

    char *p0 = (char *)start;
    while (p0 < p1 && *p0 != '-')
        p0++;
    if (p0 == p1 || p0 == start)
    {
        return false;
    }

    char *end;
    unsigned long v1 = strtoul(start, &end, 10);
    if (end != p0)
    {
        return false; // 必须解析完整个 src 段
    }

    *src = (uint32_t)v1;
    *tgt = (uint32_t)strtoul(p1 + 1, &end, 10);
    if (end != p2)
    {
        return false;
    }

    long len = p1 - p0 - 1;
    memcpy(name, p0 + 1, (size_t)len);
    name[len] = 0;
    return true;
}

void on_accept(hio_t *io)
{
    char localaddrstr[SOCKADDR_STRLEN] = {0};
    char peeraddrstr[SOCKADDR_STRLEN] = {0};
    const char *p_localaddrstr = SOCKADDR_STR(hio_localaddr(io), localaddrstr);
    const char *p_peeraddrstr = SOCKADDR_STR(hio_peeraddr(io), peeraddrstr);
    LOG_PRINT_DEBUG("on_accept connfd=%d [%s] <= [%s]", hio_fd(io),
                    p_localaddrstr,
                    p_peeraddrstr);
    uint32_t src_id = 0, target_id = 0;
    char src_clientname[LOCAL_REGISTRY_CLIENT_NAME_MAX] = {0};
    if (parse_ipc(peeraddrstr, &src_id, src_clientname, &target_id))
    {
        LOG_PRINT_DEBUG("src_id[%d], src_clientname[%s], target_id[%d]", src_id, src_clientname, target_id);
        std::shared_ptr<ipc_hv_soa_process_client> client = nullptr;
        find_and_save_process_client(src_id, src_clientname, io);
    }
    else
    {
        LOG_PRINT_ERROR("parse_ipc fail!");
    }

    hio_detach(io);

    hloop_t *idle_loop = get_idle_loop();

    hevent_t ev = {};
    ev.loop = idle_loop;
    ev.cb = on_post_event_cb;
    ev.userdata = io;
    hloop_post_event(idle_loop, &ev);
}
