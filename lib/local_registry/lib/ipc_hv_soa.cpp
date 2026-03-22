#include "ipc_hv_soa_common.hpp"

std::atomic_bool g_init_flag = false;
std::shared_ptr<ipc_hv_soa_client> g_client = nullptr;

// Helper function to setup unpack settings
static void setup_unpack_setting(void)
{
    g_client->unpack_setting.mode = UNPACK_BY_LENGTH_FIELD;
    g_client->unpack_setting.package_max_length = LOCAL_REGISTRY_MSG_HEADER_SIZE + LOCAL_REGISTRY_MSG_SIZE_MAX;
    g_client->unpack_setting.body_offset = LOCAL_REGISTRY_MSG_HEADER_SIZE;
    g_client->unpack_setting.length_field_offset = LOCAL_REGISTRY_MSG_HEADER_SIZE - sizeof(uint32_t);
    g_client->unpack_setting.length_field_bytes = sizeof(uint32_t);
    g_client->unpack_setting.length_adjustment = 0;
    g_client->unpack_setting.length_field_coding = ENCODE_BY_LITTEL_ENDIAN;
}

// Helper function to create and run event loop
static hloop_t *_create_event_loop(const char *loop_type, std::thread &loop_thread)
{
    hloop_t *loop = hloop_new(HLOOP_FLAG_AUTO_FREE);
    if (nullptr == loop)
    {
        LOG_PRINT_ERROR("%s: hloop_new fail!", loop_type);
        return nullptr;
    }

    loop_thread = std::thread([loop, loop_type]()
                              {
                                  LOG_PRINT_DEBUG("%s engine[%s]-pid[%ld]-tid[%ld]",
                                                  loop_type, hio_engine(), hloop_pid(loop), hloop_tid(loop));
                                  hloop_run(loop);
                              });

    while (HLOOP_STATUS_RUNNING != hloop_status(loop))
    {
        hv_delay(1);
    }

    return loop;
}

static void _ipc_hv_logger(int level, const char *buf, int len)
{
    (void)len;
    switch (level)
    {
        case LOG_LEVEL_FATAL:
            LOG_PRINT_CRIT("%s", buf);
            break;
        case LOG_LEVEL_ERROR:
            LOG_PRINT_ERROR("%s", buf);
            break;
        case LOG_LEVEL_WARN:
            LOG_PRINT_WARN("%s", buf);
            break;
        case LOG_LEVEL_INFO:
            LOG_PRINT_INFO("%s", buf);
            break;
        case LOG_LEVEL_DEBUG:
        case LOG_LEVEL_SILENT:
        case LOG_LEVEL_VERBOSE:
            LOG_PRINT_DEBUG("%s", buf);
            break;
        default:
            break;
    }
}

int32_t ipc_hv_soa_init(uint32_t *client_id)
{
    if (nullptr == client_id)
    {
        return IPC_HV_SOA_RET_ERR_ARG;
    }

    // Setup logging
    hlog_set_handler(_ipc_hv_logger);
    hlog_set_level(LOG_LEVEL_INFO);
    hlog_set_format("HV-%L %s");

    // Check if local_registry is running
    if (0 != access(LOCAL_REGISTRY_SOCKET_PATH, F_OK))
    {
        LOG_PRINT_ERROR("local_registry is not running!");
        return IPC_HV_SOA_RET_FAIL;
    }

    if (g_init_flag)
    {
        LOG_PRINT_DEBUG("ipc hv soa already inited!");
        return IPC_HV_SOA_RET_SUCCESS;
    }

    g_client = std::make_shared<ipc_hv_soa_client>();

    // Setup unpack settings
    setup_unpack_setting();

    // Clear maps
    {
        std::lock_guard<std::mutex> process_clients_map_lock(g_client->process_clients_map_mutex);
        g_client->process_clients_map.clear();
    }
    {
        std::lock_guard<std::mutex> services_map_lock(g_client->services_map_mutex);
        g_client->services_map.clear();
    }
    {
        std::lock_guard<std::mutex> m_timers_map_lock(g_client->m_timers_map_mutex);
        g_client->m_timers_map.clear();
    }

    // Create event loops
    g_client->m_main_loop = _create_event_loop("Main", g_client->m_main_loop_thread);
    if (nullptr == g_client->m_main_loop)
    {
        LOG_PRINT_ERROR("Failed to create main loop");
        return IPC_HV_SOA_RET_FAIL;
    }

    g_client->m_timer_loop = _create_event_loop("Timer", g_client->m_timer_loop_thread);
    if (nullptr == g_client->m_timer_loop)
    {
        LOG_PRINT_ERROR("Failed to create timer loop");
        return IPC_HV_SOA_RET_FAIL;
    }

    // Create worker loops
    for (size_t i = 0; i < LOCAL_REGISTRY_CLIENT_HV_LOOP_NUM_MAX; ++i)
    {
        hloop_t *loop = _create_event_loop("Worker", g_client->m_worker_threads.emplace_back());
        if (nullptr == loop)
        {
            LOG_PRINT_ERROR("Failed to create worker loop");
            // Cleanup on failure
            for (auto &existing_loop : g_client->m_worker_loops)
            {
                hloop_free(&existing_loop);
            }
            exit(-1);
        }
        g_client->m_worker_loops.emplace_back(loop);
    }

    // Start message handler thread
    g_client->msg_handler_thread = std::thread(process_msg_handler);

    // Initialize client info
    g_client->client_pid = getpid();

    char client_name[LOCAL_REGISTRY_CLIENT_NAME_MAX] = {};
    if (0 != util_get_exec_name(g_client->client_pid, client_name, sizeof(client_name)))
    {
        LOG_PRINT_ERROR("util_get_exec_name fail!");
        return IPC_HV_SOA_RET_FAIL;
    }
    g_client->client_name = client_name;

    char client_localaddr[LOCAL_REGISTRY_SOCKET_LEN_MAX] = {};
    snprintf(client_localaddr, sizeof(client_localaddr), LOCAL_REGISTEY_SOCKET_FMT, g_client->client_pid, client_name);
    g_client->client_localaddr = client_localaddr;

    // Connect and register with daemon
    int32_t ret = connect_with_daemon();
    if (IPC_HV_SOA_RET_SUCCESS != ret)
    {
        LOG_PRINT_ERROR("connect_with_daemon fail, ret[%d]!", ret);
        return ret;
    }

    ret = register_client_req();
    if (IPC_HV_SOA_RET_SUCCESS != ret)
    {
        LOG_PRINT_ERROR("register_client_req fail, ret[%d]!", ret);
        return ret;
    }

    // Setup local socket for listening to process clients
    char client_localaddr1[LOCAL_REGISTRY_SOCKET_LEN_MAX] = {};
    snprintf(client_localaddr1, sizeof(client_localaddr1), LOCAL_REGISTEY_SOCKET_FMT1, g_client->client_id);
    g_client->client_localaddr1 = client_localaddr1;

    unlink(client_localaddr1);
    g_client->m_listen_io = hio_create_socket(g_client->m_main_loop, client_localaddr1, -1, HIO_TYPE_SOCK_STREAM, HIO_SERVER_SIDE);
    if (nullptr == g_client->m_listen_io)
    {
        LOG_PRINT_ERROR("hio_create_socket fail!");
        return IPC_HV_SOA_RET_FAIL;
    }

    hio_setcb_close(g_client->m_listen_io, on_close);
    hio_setcb_accept(g_client->m_listen_io, on_accept);
    hio_accept(g_client->m_listen_io);

    // Register self in process clients map
    auto it = save_process_client(g_client->client_id, g_client->client_name);
    *client_id = it->client_id;

    g_init_flag = true;
    return IPC_HV_SOA_RET_SUCCESS;
}

int32_t ipc_hv_soa_destroy(uint32_t client_id)
{
    int32_t ret = IPC_HV_SOA_RET_SUCCESS;

    LOG_PRINT_DEBUG("ipv hv soa destroy");
    if (nullptr != g_client && g_client->client_id == client_id)
    {
        if (nullptr != g_client->m_daemon_io)
        {
            hio_close(g_client->m_daemon_io);
        }

        if (nullptr != g_client->m_listen_io)
        {
            hio_close(g_client->m_listen_io);
        }

        hloop_stop(g_client->m_main_loop);
        if (g_client->m_main_loop_thread.joinable())
        {
            g_client->m_main_loop_thread.join();
        }

        hloop_stop(g_client->m_timer_loop);
        if (g_client->m_timer_loop_thread.joinable())
        {
            g_client->m_timer_loop_thread.join();
        }

        for (size_t i = 0; i < g_client->m_worker_loops.size(); ++i)
        {
            if (nullptr != g_client->m_worker_loops[i])
            {
                hloop_stop(g_client->m_worker_loops[i]);
            }
        }

        for (auto &thread : g_client->m_worker_threads)
        {
            if (thread.joinable())
            {
                thread.join();
            }
        }
    }

    {
        std::lock_guard<std::mutex> process_clients_map_lock(g_client->process_clients_map_mutex);
        for (auto &ptr : g_client->process_clients_map)
        {
            if (nullptr != ptr.second->client_send_io)
            {
                hio_close(ptr.second->client_send_io);
            }

            if (nullptr != ptr.second->client_recv_io)
            {
                hio_close(ptr.second->client_recv_io);
            }

            ptr.second->send_msg_map.clear();
        }
        g_client->process_clients_map.clear();
    }

    g_client->msg_handler_queue.Clear();
    g_client->msg_handler_queue.Quit();
    if (g_client->msg_handler_thread.joinable())
    {
        g_client->msg_handler_thread.join();
    }

    return ret;
}

int32_t ipc_hv_soa_provider_service_offer(ipc_hv_soa_provider_service_t *provider_services, uint32_t provider_services_size)
{
    if (nullptr == g_client || nullptr == provider_services || 0 == provider_services_size)
    {
        return IPC_HV_SOA_RET_ERR_ARG;
    }

    if (provider_services_size >= LOCAL_REGISTRY_CLIENT_PROVIDE_SERVICES_ONCE_COUNT_MAX)
    {
        LOG_PRINT_ERROR("one clinet supporet services limit[%u]", LOCAL_REGISTRY_CLIENT_PROVIDE_SERVICES_ONCE_COUNT_MAX);
        return IPC_HV_SOA_RET_ERR_LIMIT;
    }

    int32_t ret = IPC_HV_SOA_RET_SUCCESS;
    auto it = find_process_client(g_client->client_id);
    if (nullptr == it)
    {
        LOG_PRINT_ERROR("client[%d] not register", g_client->client_id);
        return IPC_HV_SOA_RET_FAIL;
    }

    st_register_service register_service = st_register_service_init_zero;
    for (size_t i = 0; i < provider_services_size; ++i)
    {
        register_service.produce_services[i].service_id = provider_services[i].service_id;
        register_service.produce_services[i].service_type = provider_services[i].service_type;
        register_service.produce_services[i].service_status = LOCAL_SERVICE_STATUS_AVAILABLE;
        if (provider_services[i].service_type == LOCAL_SERVICE_TYPE_EVENT)
        {
            save_service(provider_services[i].service_id, provider_services[i].service_type, register_service.produce_services[i].service_status, (void *)provider_services[i].u.event_provider_cb, nullptr, it);
        }
        else if (provider_services[i].service_type == LOCAL_SERVICE_TYPE_METHOD)
        {
            save_service(provider_services[i].service_id, provider_services[i].service_type, register_service.produce_services[i].service_status, (void *)provider_services[i].u.method_provider_cb, nullptr, it);
        }
        else
        {
            LOG_PRINT_ERROR("service_type[%d] not support", provider_services[i].service_type);
            return IPC_HV_SOA_RET_ERR_ARG;
        }
    }

    register_service.produce_services_count = (pb_size_t)provider_services_size;
    register_service.has_provider_client = true;
    register_service.provider_client.client_id = g_client->client_id;
    register_service.provider_client.client_pid = g_client->client_pid;
    std::strncpy(register_service.provider_client.client_name, g_client->client_name.c_str(), sizeof(register_service.provider_client.client_name));
    register_service.provider_client.client_status = g_client->client_status;
    register_service.reg = true;

    if (g_client->client_status == LOCAL_CLIENT_STATUS_ONLINE)
    {
        std::unique_lock daemon_lock(g_client->daemo_mutex);
        ret = send_msg_to_daemon(LOCAL_REGISTRY_SERVICE_ID_METHOD_REGISTER_SERVICE, &register_service, st_register_service_fields, st_register_service_size);
        if (IPC_HV_SOA_RET_SUCCESS != ret)
        {
            LOG_PRINT_ERROR("send_msg_to_daemon fail, ret[%d]!", ret);
        }
    }
    else
    {
        LOG_PRINT_ERROR("g_client is offline");
        ret = IPC_HV_SOA_RET_FAIL;
    }

    return ret;
}

int32_t ipc_hv_soa_provider_service_revoke(uint32_t *provider_services, uint32_t provider_services_size)
{
    if (nullptr == g_client || nullptr == provider_services || 0 == provider_services_size)
    {
        return IPC_HV_SOA_RET_ERR_ARG;
    }

    if (provider_services_size >= LOCAL_REGISTRY_CLIENT_PROVIDE_SERVICES_ONCE_COUNT_MAX)
    {
        LOG_PRINT_ERROR("one clinet supporet services limit[%u]", LOCAL_REGISTRY_CLIENT_PROVIDE_SERVICES_ONCE_COUNT_MAX);
        return IPC_HV_SOA_RET_ERR_LIMIT;
    }

    int32_t ret = IPC_HV_SOA_RET_SUCCESS;

    st_register_service unregister_service = st_register_service_init_zero;
    {
        for (size_t i = 0; i < provider_services_size; ++i)
        {
            auto it = find_service(provider_services[i]);
            if (nullptr == it)
            {
                LOG_PRINT_ERROR("service[%d] not find", provider_services[i]);
                return IPC_HV_SOA_RET_FAIL;
            }

            unregister_service.produce_services[i].service_id = it->service_id;
            unregister_service.produce_services[i].service_type = it->service_type;
            unregister_service.produce_services[i].service_status = LOCAL_SERVICE_STATUS_UNAVAILABLE;
            it->service_status = LOCAL_SERVICE_STATUS_UNAVAILABLE;
        }
        unregister_service.produce_services_count = (pb_size_t)provider_services_size;
        unregister_service.has_provider_client = true;
        unregister_service.provider_client.client_id = g_client->client_id;
        unregister_service.provider_client.client_pid = g_client->client_pid;
        std::strncpy(unregister_service.provider_client.client_name, g_client->client_name.c_str(), sizeof(unregister_service.provider_client.client_name));
        unregister_service.provider_client.client_status = g_client->client_status;
    }
    unregister_service.reg = false;

    if (g_client->client_status == LOCAL_CLIENT_STATUS_ONLINE)
    {
        std::unique_lock daemon_lock(g_client->daemo_mutex);
        ret = send_msg_to_daemon(LOCAL_REGISTRY_SERVICE_ID_METHOD_REGISTER_SERVICE, &unregister_service, st_register_service_fields, st_register_service_size);
        if (IPC_HV_SOA_RET_SUCCESS != ret)
        {
            LOG_PRINT_ERROR("send_msg_to_daemon fail, ret[%d]!", ret);
        }
    }
    else
    {
        LOG_PRINT_ERROR("g_client is offline");
        ret = IPC_HV_SOA_RET_FAIL;
    }

    return ret;
}

int32_t ipc_hv_soa_provider_service_set_status(uint32_t *provider_services, uint32_t provider_services_size, uint32_t provider_services_status)
{
    if (nullptr == g_client || nullptr == provider_services || 0 == provider_services_size)
    {
        return IPC_HV_SOA_RET_ERR_ARG;
    }

    int32_t ret = IPC_HV_SOA_RET_SUCCESS;

    if (provider_services_size > LOCAL_REGISTRY_CLIENT_PROVIDE_SERVICES_ONCE_COUNT_MAX)
    {
        return IPC_HV_SOA_RET_ERR_LIMIT;
    }

    st_service_set_status set_status = st_service_set_status_init_zero;
    set_status.services_count = (pb_size_t)provider_services_size;
    for (size_t i = 0; i < provider_services_size; ++i)
    {
        set_status.services[i].service_id = provider_services[i];
        // service_type not used
        set_status.services[i].service_status = provider_services_status;
    }
    set_status.has_provider_client = true;
    set_status.provider_client.client_id = g_client->client_id;
    set_status.provider_client.client_pid = g_client->client_pid;
    std::strncpy(set_status.provider_client.client_name, g_client->client_name.c_str(), sizeof(set_status.provider_client.client_name));
    set_status.provider_client.client_status = g_client->client_status;

    if (g_client->client_status == LOCAL_CLIENT_STATUS_ONLINE)
    {
        std::unique_lock daemon_lock(g_client->daemo_mutex);
        ret = send_msg_to_daemon(LOCAL_REGISTRY_SERVICE_ID_METHOD_SERVICE_SET_STATUS, &set_status, st_service_set_status_fields, st_service_set_status_size);
        if (IPC_HV_SOA_RET_SUCCESS != ret)
        {
            LOG_PRINT_ERROR("send_msg_to_daemon fail, ret[%d]!", ret);
        }
    }
    else
    {
        LOG_PRINT_ERROR("g_client is offline");
        ret = IPC_HV_SOA_RET_FAIL;
    }

    return ret;
}

int32_t ipc_hv_soa_listener_service_subscribe(ipc_hv_soa_listener_service_t *listener_services, uint32_t listener_services_size)
{
    if (!g_init_flag)
    {
        return IPC_HV_SOA_RET_ERR_ARG;
    }

    if (nullptr == g_client || nullptr == listener_services || 0 == listener_services_size)
    {
        return IPC_HV_SOA_RET_ERR_ARG;
    }

    int32_t ret = IPC_HV_SOA_RET_SUCCESS;

    auto it = find_process_client(g_client->client_id);
    if (nullptr == it)
    {
        LOG_PRINT_ERROR("client[%d] not register", g_client->client_id);
        return IPC_HV_SOA_RET_FAIL;
    }

    st_listen_service subscribe_service = st_listen_service_init_zero;
    subscribe_service.listen_services_count = (pb_size_t)listener_services_size;
    for (size_t i = 0; i < listener_services_size; ++i)
    {
        subscribe_service.listen_services[i].service_id = listener_services[i].service_id;
        subscribe_service.listen_services[i].service_type = listener_services[i].service_type;
        subscribe_service.listen_services[i].service_status = LOCAL_SERVICE_STATUS_DEFAULT;

        // save to services_map
        std::shared_ptr<ipc_hv_soa_service> service_item = nullptr;
        if (listener_services[i].service_type == E_IPC_HV_SOA_SERVICE_TYPE_EVENT)
        {
            service_item = save_service(listener_services[i].service_id, listener_services[i].service_type, subscribe_service.listen_services[i].service_status, (void *)listener_services[i].u.event_listener_cb, nullptr, nullptr);
        }
        else if (listener_services[i].service_type == LOCAL_SERVICE_TYPE_METHOD)
        {
            service_item = save_service(listener_services[i].service_id, listener_services[i].service_type, subscribe_service.listen_services[i].service_status, (void *)listener_services[i].u.method_listener_cb, nullptr, nullptr);
        }
        else
        {
            LOG_PRINT_ERROR("service_type[%d] not support", listener_services[i].service_type);
            return IPC_HV_SOA_RET_ERR_ARG;
        }
    }
    subscribe_service.has_listener_client = true;
    subscribe_service.listener_client.client_id = g_client->client_id;
    subscribe_service.listener_client.client_pid = g_client->client_pid;
    std::strncpy(subscribe_service.listener_client.client_name, g_client->client_name.c_str(), sizeof(subscribe_service.listener_client.client_name));
    subscribe_service.listener_client.client_status = g_client->client_status;
    subscribe_service.reg = true;

    if (it->client_status == LOCAL_CLIENT_STATUS_ONLINE)
    {
        std::unique_lock daemon_lock(g_client->daemo_mutex);
        ret = send_msg_to_daemon(LOCAL_REGISTRY_SERVICE_ID_METHOD_LISTEN_SERVICE, &subscribe_service, st_listen_service_fields, st_listen_service_size);
        if (IPC_HV_SOA_RET_SUCCESS != ret)
        {
            LOG_PRINT_ERROR("send_msg_to_daemon fail, ret[%d]!", ret);
        }
    }
    else
    {
        LOG_PRINT_ERROR("g_client is offline");
        ret = IPC_HV_SOA_RET_FAIL;
    }

    return ret;
}

int32_t ipc_hv_soa_listener_service_unsubscribe(uint32_t *listener_services, uint32_t listener_services_size)
{
    if (!g_init_flag)
    {
        return IPC_HV_SOA_RET_ERR_ARG;
    }

    if (nullptr == g_client || nullptr == listener_services || 0 == listener_services_size)
    {
        return IPC_HV_SOA_RET_ERR_ARG;
    }

    int32_t ret = IPC_HV_SOA_RET_SUCCESS;

    st_listen_service unsubscribe = st_listen_service_init_zero;
    unsubscribe.listen_services_count = (pb_size_t)listener_services_size;
    for (size_t i = 0; i < listener_services_size; ++i)
    {
        auto it = find_service(listener_services[i]);
        if (nullptr == it)
        {
            LOG_PRINT_ERROR("service[%d] not find", listener_services[i]);
            return IPC_HV_SOA_RET_FAIL;
        }
        unsubscribe.listen_services[i].service_id = it->service_id;
        unsubscribe.listen_services[i].service_type = it->service_type;
        // service_status not used
    }
    unsubscribe.has_listener_client = true;
    unsubscribe.listener_client.client_id = g_client->client_id;
    unsubscribe.listener_client.client_pid = g_client->client_pid;
    std::strncpy(unsubscribe.listener_client.client_name, g_client->client_name.c_str(), sizeof(unsubscribe.listener_client.client_name));
    unsubscribe.listener_client.client_status = g_client->client_status;
    unsubscribe.reg = false;

    if (g_client->client_status == LOCAL_CLIENT_STATUS_ONLINE)
    {
        std::unique_lock daemon_lock(g_client->daemo_mutex);
        ret = send_msg_to_daemon(LOCAL_REGISTRY_SERVICE_ID_METHOD_LISTEN_SERVICE, &unsubscribe, st_listen_service_fields, st_listen_service_size);
        if (IPC_HV_SOA_RET_SUCCESS != ret)
        {
            LOG_PRINT_ERROR("send_msg_to_daemon fail, ret[%d]!", ret);
        }
    }
    else
    {
        LOG_PRINT_ERROR("g_client is offline");
        ret = IPC_HV_SOA_RET_FAIL;
    }

    return ret;
}

int32_t ipc_hv_soa_method_notify(uint32_t service_id, void *method_req_data, uint32_t method_req_data_len)
{
    if (!g_init_flag)
    {
        return IPC_HV_SOA_RET_ERR_ARG;
    }

    int32_t ret = IPC_HV_SOA_RET_SUCCESS;

    auto it = get_service(service_id);
    if (nullptr == it)
    {
        LOG_PRINT_ERROR("service[%d] not find", service_id);
        return IPC_HV_SOA_RET_ERR_ARG;
    }

    if (nullptr == it->service_provider)
    {
        LOG_PRINT_ERROR("service[%d] is no provider", service_id);
        return IPC_HV_SOA_RET_ERR_ARG;
    }

    if (it->service_type != E_IPC_HV_SOA_SERVICE_TYPE_METHOD || it->service_status != LOCAL_SERVICE_STATUS_AVAILABLE)
    {
        LOG_PRINT_ERROR("invalid service[%d]", service_id);
        return IPC_HV_SOA_RET_ERR_ARG;
    }

    uint32_t msg_seqid = it->service_provider->send_msg_seqid++;
    if (nullptr == method_req_data || method_req_data_len == 0)
    {
        ret = send_msg_to_process(it->service_provider, g_client->client_id, msg_seqid, E_IPC_HV_SOA_MSG_TYPE_METHOD_NOTIFY, service_id, 0, nullptr);
    }
    else
    {
        ret = send_msg_to_process(it->service_provider, g_client->client_id, msg_seqid, E_IPC_HV_SOA_MSG_TYPE_METHOD_NOTIFY, service_id, method_req_data_len, method_req_data);
    }

    if (IPC_HV_SOA_RET_SUCCESS != ret)
    {
        LOG_PRINT_ERROR("ipc_hv_soa_method_notify[%d] fail", service_id);
    }

    return ret;
}

int32_t ipc_hv_soa_method_sync(uint32_t service_id, void *method_req_data, uint32_t method_req_data_len, void *method_resp_data, uint32_t *method_resp_data_len, uint32_t timeout_ms)
{
    if (!g_init_flag)
    {
        return IPC_HV_SOA_RET_ERR_ARG;
    }

    int32_t ret = IPC_HV_SOA_RET_SUCCESS;

    auto it = get_service(service_id);
    if (nullptr == it)
    {
        LOG_PRINT_ERROR("service[%d] not find", service_id);
        return IPC_HV_SOA_RET_ERR_ARG;
    }

    if (nullptr == it->service_provider)
    {
        LOG_PRINT_ERROR("service[%d] is no provider", service_id);
        return IPC_HV_SOA_RET_ERR_ARG;
    }

    if (it->service_type != E_IPC_HV_SOA_SERVICE_TYPE_METHOD || it->service_status != LOCAL_SERVICE_STATUS_AVAILABLE)
    {
        LOG_PRINT_ERROR("invalid service[%d]", service_id);
        return IPC_HV_SOA_RET_ERR_ARG;
    }

    uint32_t msg_seqid = it->service_provider->send_msg_seqid++;
    if (nullptr == method_req_data || method_req_data_len == 0)
    {
        ret = send_msg_to_process_sync(it->service_provider, g_client->client_id, msg_seqid, service_id, 0, nullptr, method_resp_data, method_resp_data_len, timeout_ms);
    }
    else
    {
        ret = send_msg_to_process_sync(it->service_provider, g_client->client_id, msg_seqid, service_id, method_req_data_len, method_req_data, method_resp_data, method_resp_data_len, timeout_ms);
    }

    return ret;
}

int32_t ipc_hv_soa_method_async(uint32_t service_id, void *method_req_data, uint32_t method_req_data_len, PF_IPC_HV_SOA_METHOD_ASYNC_CB async_cb)
{
    if (!g_init_flag)
    {
        return IPC_HV_SOA_RET_ERR_ARG;
    }

    int32_t ret = IPC_HV_SOA_RET_SUCCESS;

    auto it = get_service(service_id);
    if (nullptr == it)
    {
        LOG_PRINT_ERROR("service[%d] not find", service_id);
        return IPC_HV_SOA_RET_ERR_ARG;
    }

    if (nullptr == it->service_provider)
    {
        LOG_PRINT_ERROR("service[%d] is no provider", service_id);
        return IPC_HV_SOA_RET_ERR_ARG;
    }

    if (it->service_type != E_IPC_HV_SOA_SERVICE_TYPE_METHOD || it->service_status != LOCAL_SERVICE_STATUS_AVAILABLE)
    {
        LOG_PRINT_ERROR("invalid service[%d]", service_id);
        return IPC_HV_SOA_RET_ERR_ARG;
    }

    it->service_async_handler = (void *)async_cb;
    uint32_t msg_seqid = it->service_provider->send_msg_seqid++;
    if (nullptr == method_req_data || method_req_data_len == 0)
    {
        ret = send_msg_to_process(it->service_provider, g_client->client_id, msg_seqid, E_IPC_HV_SOA_MSG_TYPE_METHOD_REQUEST_ASYNC, service_id, 0, nullptr);
    }
    else
    {
        ret = send_msg_to_process(it->service_provider, g_client->client_id, msg_seqid, E_IPC_HV_SOA_MSG_TYPE_METHOD_REQUEST_ASYNC, service_id, method_req_data_len, method_req_data);
    }

    if (IPC_HV_SOA_RET_SUCCESS != ret)
    {
        LOG_PRINT_ERROR("send_msg_to_process[%d] fail", service_id);
    }

    return ret;
}

int32_t ipc_hv_soa_method_complete(ipc_hv_soa_msg_handle_t handle, uint32_t service_id, void *method_resp_data, uint32_t method_resp_data_len)
{
    if (!g_init_flag)
    {
        return IPC_HV_SOA_RET_ERR_ARG;
    }

    int32_t ret = IPC_HV_SOA_RET_SUCCESS;

    uint32_t msg_type = E_IPC_HV_SOA_MSG_TYPE_INVALID;
    if (E_IPC_HV_SOA_MSG_TYPE_METHOD_REQUEST_SYNC == handle.msg_type)
    {
        msg_type = E_IPC_HV_SOA_MSG_TYPE_METHOD_RESPONSE_SYNC;
    }
    else if (E_IPC_HV_SOA_MSG_TYPE_METHOD_REQUEST_ASYNC == handle.msg_type)
    {
        msg_type = E_IPC_HV_SOA_MSG_TYPE_METHOD_RESPONSE_ASYNC;
    }
    else
    {
        LOG_PRINT_ERROR("invalid msg[%d]", handle.msg_type);
        return IPC_HV_SOA_RET_ERR_ARG;
    }

    auto it = get_process_client(handle.client_id);
    if (nullptr == it)
    {
        LOG_PRINT_ERROR("invalid client[%d]", handle.client_id);
        return IPC_HV_SOA_RET_FAIL;
    }

    auto it1 = get_service(service_id);
    if (nullptr == it1)
    {
        LOG_PRINT_ERROR("service[%d] not find", service_id);
        return IPC_HV_SOA_RET_ERR_ARG;
    }

    if (nullptr == method_resp_data || method_resp_data_len == 0)
    {
        ret = send_msg_to_process(it, g_client->client_id, handle.msg_seqid, msg_type, service_id, 0, nullptr);
    }
    else
    {
        ret = send_msg_to_process(it, g_client->client_id, handle.msg_seqid, msg_type, service_id, method_resp_data_len, method_resp_data);
    }

    if (IPC_HV_SOA_RET_SUCCESS != ret)
    {
        LOG_PRINT_ERROR("ipc_hv_soa_method_complete[%d] fail", service_id);
    }

    return ret;
}

int32_t ipc_hv_soa_event_trigger(uint32_t service_id, void *event_data, uint32_t event_data_len)
{
    if (!g_init_flag)
    {
        return IPC_HV_SOA_RET_ERR_ARG;
    }

    int32_t ret = IPC_HV_SOA_RET_SUCCESS;

    auto it = get_service(service_id);
    if (nullptr == it)
    {
        LOG_PRINT_ERROR("service[%d] not find", service_id);
        return IPC_HV_SOA_RET_ERR_ARG;
    }

    if (nullptr == it->service_provider)
    {
        LOG_PRINT_ERROR("service[%d] is no provider", service_id);
        return IPC_HV_SOA_RET_ERR_ARG;
    }

    if (it->service_type != E_IPC_HV_SOA_SERVICE_TYPE_EVENT || it->service_status != LOCAL_SERVICE_STATUS_AVAILABLE)
    {
        LOG_PRINT_ERROR("invalid service[%d]", service_id);
        return IPC_HV_SOA_RET_ERR_ARG;
    }

    for (auto &listener : it->service_listeners)
    {
        uint32_t msg_seqid = listener.second->send_msg_seqid++;
        if (nullptr == event_data || 0 == event_data_len)
        {
            ret = send_msg_to_process(listener.second, g_client->client_id, msg_seqid, E_IPC_HV_SOA_MSG_TYPE_EVENT_NOTIFY, service_id, 0, nullptr);
        }
        else
        {
            ret = send_msg_to_process(listener.second, g_client->client_id, msg_seqid, E_IPC_HV_SOA_MSG_TYPE_EVENT_NOTIFY, service_id, event_data_len, event_data);
        }

        if (IPC_HV_SOA_RET_SUCCESS != ret)
        {
            LOG_PRINT_ERROR("ipc_hv_soa_event_trigger[%d] fail", service_id);
        }
    }

    return ret;
}
