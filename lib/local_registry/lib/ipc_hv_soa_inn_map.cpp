#include "ipc_hv_soa_inn.hpp"

extern std::shared_ptr<ipc_hv_soa_client> g_client;

std::shared_ptr<ipc_hv_soa_process_client> find_process_client(uint32_t client_id)
{
    std::lock_guard<std::mutex> process_clients_map_lock(g_client->process_clients_map_mutex);
    auto it = g_client->process_clients_map.find(client_id);
    if (g_client->process_clients_map.end() == it)
    {
        LOG_PRINT_ERROR("client[%d] not find", client_id);
        return nullptr;
    }

    return it->second;
}

std::shared_ptr<ipc_hv_soa_process_client> save_process_client(uint32_t client_id, std::string client_name)
{
    int32_t ret = IPC_HV_SOA_RET_SUCCESS;

    std::shared_ptr<ipc_hv_soa_process_client> client = std::make_shared<ipc_hv_soa_process_client>();
    client->client_id = client_id;
    client->client_name = client_name;
    char client_localaddr1[LOCAL_REGISTRY_SOCKET_LEN_MAX] = {};
    snprintf(client_localaddr1, sizeof(client_localaddr1), LOCAL_REGISTEY_SOCKET_FMT1, client_id);
    client->client_localaddr1 = client_localaddr1;
    client->client_status = LOCAL_CLIENT_STATUS_INIT;
    client->client_send_io = nullptr;
    client->client_recv_io = nullptr;
    client->send_msg_seqid = 0;
    client->send_msg_map.clear();
    client->send_msg_cond_ret = -1;

    if (client_id != g_client->client_id)
    {
        {
            std::lock_guard<std::mutex> process_clients_map_lock(g_client->process_clients_map_mutex);
            g_client->process_clients_map.insert({client_id, client});
        }
        ret = connect_with_process_client(client);
        if (IPC_HV_SOA_RET_SUCCESS != ret)
        {
            LOG_PRINT_ERROR("connect_with_process_client fail, ret[%d]!", ret);
        }
    }
    else
    {
        client->client_status = LOCAL_CLIENT_STATUS_ONLINE;
        std::lock_guard<std::mutex> process_clients_map_lock(g_client->process_clients_map_mutex);
        g_client->process_clients_map.insert({client_id, client});
    }

    return client;
}

std::shared_ptr<ipc_hv_soa_process_client> find_and_save_process_client(uint32_t client_id, std::string client_name, hio_t *io)
{
    int32_t ret = IPC_HV_SOA_RET_SUCCESS;
    std::shared_ptr<ipc_hv_soa_process_client> client = nullptr;

    std::lock_guard<std::mutex> process_clients_map_lock(g_client->process_clients_map_mutex);
    auto it = g_client->process_clients_map.find(client_id);
    if (g_client->process_clients_map.end() == it || it->second == nullptr)
    {
        client = std::make_shared<ipc_hv_soa_process_client>();
        client->client_id = client_id;
        client->client_name = client_name;
        char client_localaddr1[LOCAL_REGISTRY_SOCKET_LEN_MAX] = {};
        snprintf(client_localaddr1, sizeof(client_localaddr1), LOCAL_REGISTEY_SOCKET_FMT1, client_id);
        client->client_localaddr1 = client_localaddr1;
        client->client_status = LOCAL_CLIENT_STATUS_INIT;
        client->client_send_io = nullptr;
        client->client_recv_io = nullptr;
        client->send_msg_seqid = 0;
        client->send_msg_map.clear();
        client->send_msg_cond_ret = -1;

        if (client_id != g_client->client_id)
        {
            g_client->process_clients_map.insert({client_id, client});
            ret = connect_with_process_client(client);
            if (IPC_HV_SOA_RET_SUCCESS != ret)
            {
                LOG_PRINT_ERROR("connect_with_process_client fail, ret[%d]!", ret);
            }
        }
        else
        {
            client->client_status = LOCAL_CLIENT_STATUS_ONLINE;
            g_client->process_clients_map.insert({client_id, client});
        }
    }
    else
    {
        client = it->second;
        client->client_status = LOCAL_CLIENT_STATUS_ONLINE;
    }

    void *userdata = (void *)(uintptr_t)client_id;
    hevent_set_userdata(io, userdata);
    client->client_recv_io = io;

    return client;
}

std::shared_ptr<ipc_hv_soa_process_client> get_process_client(uint32_t client_id)
{
    int32_t ret = IPC_HV_SOA_RET_SUCCESS;
    std::shared_ptr<ipc_hv_soa_process_client> client = nullptr;
    auto it = find_process_client(client_id);
    if (nullptr == it)
    {
        st_get_client_req req = st_get_client_req_init_zero;
        req.client_id = client_id;

        if (g_client->client_status == LOCAL_CLIENT_STATUS_ONLINE)
        {
            std::unique_lock daemon_lock(g_client->daemo_mutex);
            ret = send_msg_to_daemon(LOCAL_REGISTRY_SERVICE_ID_METHOD_GET_CLIENT, &req, st_get_client_req_fields, st_get_client_req_size);
            if (IPC_HV_SOA_RET_SUCCESS != ret)
            {
                LOG_PRINT_ERROR("send_msg_to_daemon fail, ret[%d]!", ret);
                return nullptr;
            }

            // init
            g_client->daemon_cond_msgid = 0;
            memset(g_client->daemon_cond_msgdata, 0x00, sizeof(g_client->daemon_cond_msgdata));
            g_client->daemon_cond_ret = -1;

            st_get_client_resp *resp = nullptr;
            auto timeout = std::chrono::milliseconds(LOCAL_REGISTRY_COMMUNICATION_TIMEOUT_MS);
            bool ready = g_client->daemon_cond.wait_for(daemon_lock, timeout, []
                                                        {
                                                            return (g_client->daemon_cond_ret != -1);
                                                        });
            if (!ready)
            {
                ret = IPC_HV_SOA_RET_TIMEOUT;
            }
            else
            {
                if (g_client->daemon_cond_ret == 0 && g_client->daemon_cond_msgid == LOCAL_REGISTRY_SERVICE_ID_METHOD_GET_CLIENT)
                {
                    resp = (st_get_client_resp *)g_client->daemon_cond_msgdata;
                    if (resp->has_responser_client)
                    {
                        client = save_process_client(resp->responser_client.client_id, resp->responser_client.client_name);
                    }
                }
                else
                {
                    LOG_PRINT_ERROR("register client_id fail, invalid daemon_cond_ret[%d], daemon_cond_msgdid[%d]!", g_client->daemon_cond_ret, g_client->daemon_cond_msgid);
                    ret = IPC_HV_SOA_RET_FAIL;
                }
            }

            // clean
            g_client->daemon_cond_msgid = 0;
            g_client->daemon_cond_ret = -1;
        }
        else
        {
            LOG_PRINT_ERROR("g_client is offline");
            return nullptr;
        }
    }
    else
    {
        client = it;
    }

    return client;
}

std::shared_ptr<ipc_hv_soa_service> find_service(uint32_t service_id)
{
    std::lock_guard<std::mutex> services_map_lock(g_client->services_map_mutex);
    auto it = g_client->services_map.find(service_id);
    if (g_client->services_map.end() == it)
    {
        return nullptr;
    }

    return it->second;
}

std::shared_ptr<ipc_hv_soa_service> save_service(uint32_t service_id, uint32_t service_type, uint32_t service_status, void *service_handler, void *service_async_handler, std::shared_ptr<ipc_hv_soa_process_client> client)
{
    std::shared_ptr<ipc_hv_soa_service> service_item = nullptr;
    std::lock_guard<std::mutex> services_map_lock(g_client->services_map_mutex);
    auto it = g_client->services_map.find(service_id);
    if (nullptr == it)
    {
        service_item = std::make_shared<ipc_hv_soa_service>();
        service_item->service_id = service_id;
        service_item->service_type = service_type;
        if (nullptr == client)
        {
            service_item->service_status = LOCAL_SERVICE_STATUS_DEFAULT;
        }
        else
        {
            service_item->service_status = service_status;
        }
        service_item->service_handler = service_handler;
        service_item->service_async_handler = service_async_handler;
        service_item->service_provider = nullptr;
        service_item->service_listeners.clear();
        g_client->services_map.insert({service_item->service_id, service_item});
    }
    else
    {
        service_item = it->second;
        service_item->service_type = service_type;
        service_item->service_status = service_status;
        if (nullptr != service_handler)
        {
            service_item->service_handler = service_handler;
        }

        if (nullptr != service_async_handler)
        {
            service_item->service_async_handler = service_async_handler;
        }
    }

    if (nullptr != client)
    {
        service_item->service_provider = client;
    }

    return service_item;
}

std::shared_ptr<ipc_hv_soa_service> get_service(uint32_t service_id)
{
    int32_t ret = IPC_HV_SOA_RET_SUCCESS;
    std::shared_ptr<ipc_hv_soa_service> service_item = nullptr;

    auto it = find_service(service_id);
    if (nullptr == it ||
        (it->service_type == E_IPC_HV_SOA_SERVICE_TYPE_METHOD && it->service_status == LOCAL_SERVICE_STATUS_DEFAULT))
    {
        if (g_client->client_status == LOCAL_CLIENT_STATUS_ONLINE)
        {
            st_get_service_req get_service = st_get_service_req_init_zero;
            get_service.service_id = service_id;
            get_service.has_listener_client = true;
            get_service.listener_client.client_id = g_client->client_id;
            get_service.listener_client.client_pid = g_client->client_pid;
            std::strncpy(get_service.listener_client.client_name, g_client->client_name.c_str(), sizeof(get_service.listener_client.client_name));
            get_service.listener_client.client_status = g_client->client_status;
            std::unique_lock daemon_lock(g_client->daemo_mutex);
            ret = send_msg_to_daemon(LOCAL_REGISTRY_SERVICE_ID_METHOD_GET_SERVICE, &get_service, st_get_service_req_fields, st_get_service_req_size);
            if (IPC_HV_SOA_RET_SUCCESS != ret)
            {
                LOG_PRINT_ERROR("send_msg_to_daemon fail, ret[%d]!", ret);
                return nullptr;
            }

            // init
            g_client->daemon_cond_msgid = 0;
            g_client->daemon_cond_ret = -1;
            memset(g_client->daemon_cond_msgdata, 0x00, sizeof(g_client->daemon_cond_msgdata));

            st_get_service_resp *resp = nullptr;
            auto timeout = std::chrono::milliseconds(LOCAL_REGISTRY_COMMUNICATION_TIMEOUT_MS);
            bool ready = g_client->daemon_cond.wait_for(daemon_lock, timeout, []
                                                        {
                                                            return g_client->daemon_cond_ret != -1;
                                                        });
            if (!ready)
            {
                LOG_PRINT_ERROR("get service timeout");
                ret = IPC_HV_SOA_RET_TIMEOUT;
            }
            else
            {
                if (g_client->daemon_cond_ret == 0 && g_client->daemon_cond_msgid == LOCAL_REGISTRY_SERVICE_ID_METHOD_GET_SERVICE)
                {
                    resp = (st_get_service_resp *)g_client->daemon_cond_msgdata;
                    LOG_PRINT_DEBUG("get service resp service_id[%u] service_type[%u], service_status[%u]",
                                    resp->service.service_id, resp->service.service_type, resp->service.service_status);
                    std::shared_ptr<ipc_hv_soa_process_client> client = nullptr;
                    if (resp->has_provider_client)
                    {
                        auto it2 = find_process_client(resp->provider_client.client_id);
                        if (nullptr == it2)
                        {
                            client = save_process_client(resp->provider_client.client_id, resp->provider_client.client_name);
                        }
                        else
                        {
                            client = it2;
                        }
                    }
                    service_item = save_service(resp->service.service_id, resp->service.service_type, resp->service.service_status, nullptr, nullptr, client);
                }
                else
                {
                    LOG_PRINT_ERROR("get service fail, invalid daemon_cond_ret[%d], daemon_cond_msgid[%d]!", g_client->daemon_cond_ret, g_client->daemon_cond_msgid);
                    ret = IPC_HV_SOA_RET_FAIL;
                }
            }

            // clean
            g_client->daemon_cond_msgid = 0;
            g_client->daemon_cond_ret = -1;
        }
        else
        {
            LOG_PRINT_ERROR("g_client is offline");
            ret = IPC_HV_SOA_RET_FAIL;
        }
    }
    else
    {
        service_item = it;
    }

    return service_item;
}