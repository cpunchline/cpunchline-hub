#include "ipc_hv_soa_common.hpp"

extern std::shared_ptr<ipc_hv_soa_client> g_client;

// Helper function to create process client object
static std::shared_ptr<ipc_hv_soa_process_client> _create_process_client(uint32_t client_id, const std::string &client_name)
{
    auto client = std::make_shared<ipc_hv_soa_process_client>();
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
    client->send_msg_cond_ret = IPC_HV_SOA_COND_STATE_INIT;

    return client;
}

// Helper function to insert client into map and handle connection
static int32_t _insert_and_connect_client(std::shared_ptr<ipc_hv_soa_process_client> client)
{
    int32_t ret = IPC_HV_SOA_RET_SUCCESS;

    {
        std::lock_guard<std::mutex> process_clients_map_lock(g_client->process_clients_map_mutex);
        g_client->process_clients_map.insert({client->client_id, client});
    }

    if (client->client_id != g_client->client_id)
    {
        ret = connect_with_process_client(client);
        if (IPC_HV_SOA_RET_SUCCESS != ret)
        {
            LOG_PRINT_ERROR("connect_with_process_client fail, ret[%d]!", ret);
        }
    }
    else
    {
        client->client_status = LOCAL_CLIENT_STATUS_ONLINE;
    }

    return ret;
}

std::shared_ptr<ipc_hv_soa_process_client> find_process_client(uint32_t client_id)
{
    std::lock_guard<std::mutex> process_clients_map_lock(g_client->process_clients_map_mutex);
    auto it = g_client->process_clients_map.find(client_id);

    if (g_client->process_clients_map.end() == it || it->second == nullptr)
    {
        LOG_PRINT_ERROR("client[%u] not found", client_id);
        return nullptr;
    }

    return it->second;
}

std::shared_ptr<ipc_hv_soa_process_client> save_process_client(uint32_t client_id, std::string client_name)
{
    auto client = _create_process_client(client_id, client_name);
    _insert_and_connect_client(client);
    return client;
}

std::shared_ptr<ipc_hv_soa_process_client> find_and_save_process_client(uint32_t client_id, std::string client_name, hio_t *io)
{
    std::shared_ptr<ipc_hv_soa_process_client> client = nullptr;

    std::unique_lock<std::mutex> process_clients_map_lock(g_client->process_clients_map_mutex);
    auto it = g_client->process_clients_map.find(client_id);

    if (g_client->process_clients_map.end() == it || it->second == nullptr)
    {
        // Client not found, create new one
        client = _create_process_client(client_id, client_name);
        g_client->process_clients_map.insert({client_id, client});

        // Connect if needed (unlock mutex during blocking operation)
        process_clients_map_lock.unlock();

        if (client_id != g_client->client_id)
        {
            int32_t ret = connect_with_process_client(client);
            if (IPC_HV_SOA_RET_SUCCESS != ret)
            {
                LOG_PRINT_ERROR("connect_with_process_client fail, ret[%d]!", ret);
            }
        }
        else
        {
            client->client_status = LOCAL_CLIENT_STATUS_ONLINE;
        }
    }
    else
    {
        // Client found, update status
        client = it->second;
        client->client_status = LOCAL_CLIENT_STATUS_ONLINE;
    }

    // Setup recv IO
    void *userdata = (void *)(uintptr_t)client_id;
    hevent_set_userdata(io, userdata);
    client->client_recv_io = io;

    return client;
}

std::shared_ptr<ipc_hv_soa_process_client> get_process_client(uint32_t client_id)
{
    auto client = find_process_client(client_id);
    if (nullptr != client)
    {
        return client;
    }

    // Client not found, query from daemon
    st_get_client_req req = st_get_client_req_init_zero;
    req.client_id = client_id;

    if (g_client->client_status != LOCAL_CLIENT_STATUS_ONLINE)
    {
        LOG_PRINT_ERROR("g_client is offline");
        return nullptr;
    }

    // Use sync function to simplify code
    st_get_client_resp resp = st_get_client_resp_init_zero;
    uint32_t resp_size = sizeof(st_get_client_resp);

    int32_t ret = send_msg_to_daemon_sync(
        LOCAL_REGISTRY_SERVICE_ID_METHOD_GET_CLIENT,
        E_IPC_HV_SOA_MSG_TYPE_METHOD_REQUEST_SYNC,
        &req,
        st_get_client_req_fields,
        st_get_client_req_size,
        &resp,
        &resp_size,
        LOCAL_REGISTRY_COMMUNICATION_TIMEOUT_MS);

    if (IPC_HV_SOA_RET_SUCCESS != ret)
    {
        LOG_PRINT_ERROR("send_msg_to_daemon_sync fail, ret[%d]!", ret);
        return nullptr;
    }

    if (resp.has_responser_client)
    {
        return save_process_client(resp.responser_client.client_id, resp.responser_client.client_name);
    }

    return nullptr;
}

std::shared_ptr<ipc_hv_soa_service> find_service(uint32_t service_id)
{
    std::lock_guard<std::mutex> services_map_lock(g_client->services_map_mutex);
    auto it = g_client->services_map.find(service_id);

    if (g_client->services_map.end() == it || it->second == nullptr)
    {
        return nullptr;
    }

    return it->second;
}

std::shared_ptr<ipc_hv_soa_service> save_service(uint32_t service_id, uint32_t service_type, uint32_t service_status,
                                                 void *service_handler, void *service_async_handler,
                                                 std::shared_ptr<ipc_hv_soa_process_client> client)
{
    std::shared_ptr<ipc_hv_soa_service> service_item = nullptr;

    std::lock_guard<std::mutex> services_map_lock(g_client->services_map_mutex);
    auto it = g_client->services_map.find(service_id);

    if (g_client->services_map.end() == it || it->second == nullptr)
    {
        // Create new service
        service_item = std::make_shared<ipc_hv_soa_service>();
        service_item->service_id = service_id;
        service_item->service_type = service_type;
        service_item->service_status = (nullptr == client) ? (uint32_t)LOCAL_SERVICE_STATUS_DEFAULT : service_status;
        service_item->service_handler = service_handler;
        service_item->service_async_handler = service_async_handler;
        service_item->service_provider = nullptr;
        service_item->service_listeners.clear();

        g_client->services_map.insert({service_id, service_item});
    }
    else
    {
        // Update existing service
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
    auto service_item = find_service(service_id);

    // Check if service exists and is valid
    bool need_query = (nullptr == service_item) ||
        (service_item->service_type == E_IPC_HV_SOA_SERVICE_TYPE_METHOD &&
         service_item->service_status == LOCAL_SERVICE_STATUS_DEFAULT);

    if (!need_query)
    {
        return service_item;
    }

    // Query from daemon
    if (g_client->client_status != LOCAL_CLIENT_STATUS_ONLINE)
    {
        LOG_PRINT_ERROR("g_client is offline");
        return nullptr;
    }

    st_get_service_req req = st_get_service_req_init_zero;
    req.service_id = service_id;
    req.has_listener_client = true;
    req.listener_client.client_id = g_client->client_id;
    req.listener_client.client_pid = g_client->client_pid;
    std::strncpy(req.listener_client.client_name, g_client->client_name.c_str(), sizeof(req.listener_client.client_name));
    req.listener_client.client_status = g_client->client_status;

    st_get_service_resp resp = st_get_service_resp_init_zero;
    uint32_t resp_size = sizeof(st_get_service_resp);

    int32_t ret = send_msg_to_daemon_sync(
        LOCAL_REGISTRY_SERVICE_ID_METHOD_GET_SERVICE,
        E_IPC_HV_SOA_MSG_TYPE_METHOD_REQUEST_SYNC,
        &req,
        st_get_service_req_fields,
        st_get_service_req_size,
        &resp,
        &resp_size,
        LOCAL_REGISTRY_COMMUNICATION_TIMEOUT_MS);

    if (IPC_HV_SOA_RET_SUCCESS != ret)
    {
        LOG_PRINT_ERROR("send_msg_to_daemon_sync fail, ret[%d]!", ret);
        return nullptr;
    }

    LOG_PRINT_DEBUG("get service resp service_id[%u] service_type[%u], service_status[%u]",
                    resp.service.service_id, resp.service.service_type, resp.service.service_status);

    // Get provider client if exists
    std::shared_ptr<ipc_hv_soa_process_client> client = nullptr;
    if (resp.has_provider_client)
    {
        client = get_process_client(resp.provider_client.client_id);
    }

    return save_service(resp.service.service_id, resp.service.service_type, resp.service.service_status,
                        nullptr, nullptr, client);
}
