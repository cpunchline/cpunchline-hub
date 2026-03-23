#include "ipc_hv_soa_common.hpp"

extern std::atomic_bool g_init_flag;
extern std::shared_ptr<ipc_hv_soa_client> g_client;

// Internal helper function to encode and send message to daemon
// Optional msg_seqid parameter: if provided, use it; otherwise generate a new one
static int32_t _send_msg_to_daemon_internal(uint32_t service_id, uint32_t msg_seqid, uint32_t msg_type, const void *msgdata, const pb_msgdesc_t *fields, uint32_t field_size)
{
    int32_t ret = IPC_HV_SOA_RET_SUCCESS;

    if (nullptr == g_client->m_daemon_io)
    {
        LOG_PRINT_ERROR("m_daemon_io is nullptr");
        return IPC_HV_SOA_RET_FAIL;
    }

    if (g_client->client_status != LOCAL_CLIENT_STATUS_ONLINE)
    {
        LOG_PRINT_ERROR("client is offline");
        return IPC_HV_SOA_RET_FAIL;
    }

    LOG_PRINT_DEBUG("send service_id[%u] service_seqid[%u] to daemon fd[%d]", service_id, msg_seqid, hio_fd(g_client->m_daemon_io));

    size_t encoded_size = 0;
    if (nullptr != msgdata && field_size > 0)
    {
        if (!pb_get_encoded_size(&encoded_size, fields, msgdata))
        {
            LOG_PRINT_ERROR("pb_get_encoded_size service_id[%u] fail!", service_id);
            return -1;
        }
    }
    else
    {
        encoded_size = 0;
    }

    uint8_t buffer[LOCAL_REGISTRY_MSG_HEADER_SIZE + LOCAL_REGISTRY_MSG_SIZE_MAX] = {};
    if (nullptr != msgdata && encoded_size > 0)
    {
        pb_ostream_t stream = pb_ostream_from_buffer(buffer + LOCAL_REGISTRY_MSG_HEADER_SIZE, field_size);
        bool status = pb_encode(&stream, fields, msgdata);
        if (!status)
        {
            LOG_PRINT_ERROR("pb_encode service_id[%u] fail, error(%s)", service_id, PB_GET_ERROR(&stream));
            return -1;
        }
        LOG_PRINT_DEBUG("pb_encode service_id[%u] success", service_id);
    }
    else
    {
        LOG_PRINT_DEBUG("pb_encode service_id[%u] success(no need)", service_id);
    }

    st_local_msg_header send_msg_header = {};
    send_msg_header.client_id = MODULE_ID_AUTOLIB_LOCAL_REGISTRY;
    send_msg_header.msg_seqid = msg_seqid;
    send_msg_header.msg_type = msg_type;
    send_msg_header.service_id = service_id;
    send_msg_header.msg_len = (uint32_t)encoded_size;
    memcpy(buffer, &send_msg_header, sizeof(send_msg_header));

    // Check connection and send
    if (!hio_is_opened(g_client->m_daemon_io))
    {
        LOG_PRINT_ERROR("hio_is_opened fail, fd[%d] disconnect", hio_fd(g_client->m_daemon_io));
        return IPC_HV_SOA_RET_FAIL;
    }

    ret = hio_write(g_client->m_daemon_io, buffer, LOCAL_REGISTRY_MSG_HEADER_SIZE + encoded_size);
    if (ret < 0)
    {
        LOG_PRINT_ERROR("hio_write fail, ret[%d], error[%d]", ret, hio_error(g_client->m_daemon_io));
        ret = IPC_HV_SOA_RET_FAIL;
    }
    else
    {
        ret = IPC_HV_SOA_RET_SUCCESS;
    }

    return ret;
}

int32_t send_msg_to_daemon(uint32_t service_id, uint32_t msg_type, const void *msgdata, const pb_msgdesc_t *fileds, uint32_t field_size)
{
    uint32_t msg_seqid = g_client->m_msg_seqid.fetch_add(1);
    return _send_msg_to_daemon_internal(service_id, msg_seqid, msg_type, msgdata, fileds, field_size);
}

int32_t send_msg_to_daemon_sync(uint32_t service_id, uint32_t msg_type, const void *msgdata, const pb_msgdesc_t *fields, uint32_t field_size, void *resp_data, uint32_t *resp_data_len, uint32_t timeout_ms)
{
    if (nullptr == g_client)
    {
        LOG_PRINT_ERROR("g_client is nullptr");
        return IPC_HV_SOA_RET_FAIL;
    }

    // Validate parameters
    if (nullptr == fields || (field_size > 0 && nullptr == msgdata))
    {
        LOG_PRINT_ERROR("invalid arguments");
        return IPC_HV_SOA_RET_ERR_ARG;
    }

    uint32_t msg_seqid = g_client->m_msg_seqid.fetch_add(1);

    // Borrow a sync context from the pool using RAII
    auto guard = g_client->daemon_sync_pool.Borrow();
    if (!guard.IsValid())
    {
        LOG_PRINT_ERROR("failed to borrow sync context from pool");
        return IPC_HV_SOA_RET_FAIL;
    }
    auto ctx = guard.get();

    // Set context data - access through SyncContext's data member
    ctx->data.header.client_id = g_client->client_id;
    ctx->data.header.msg_seqid = msg_seqid;
    ctx->data.header.msg_type = msg_type;
    ctx->data.header.service_id = service_id;
    ctx->data.header.msg_len = field_size;
    memset(ctx->data.data, 0x00, sizeof(ctx->data.data));

    // Add context to pending requests map using msg_seqid as key
    // This ensures each request has a unique identifier in the map
    {
        std::lock_guard<std::mutex> pending_lock(g_client->pending_requests_mutex);
        g_client->pending_requests.insert({msg_seqid, guard.get_shared_ptr()});
    }

    // Send message using internal helper function with the same msg_seqid
    int32_t ret = _send_msg_to_daemon_internal(service_id, msg_seqid, msg_type, msgdata, fields, field_size);
    if (IPC_HV_SOA_RET_SUCCESS != ret)
    {
        LOG_PRINT_ERROR("_send_msg_to_daemon_internal fail, ret[%d], msg_seqid[%u]", ret, msg_seqid);
        // Remove from pending requests map
        {
            std::lock_guard<std::mutex> pending_lock(g_client->pending_requests_mutex);
            g_client->pending_requests.erase(msg_seqid);
        }
        // Set context result to indicate error
        ctx->SetResult(IPC_HV_SOA_COND_STATE_INIT);
        return ret;
    }

    // Wait for response with timeout
    int wait_result = ctx->Wait(timeout_ms);
    if (wait_result < 0)
    {
        LOG_PRINT_ERROR("wait response timeout for service_id[%u], msg_seqid[%u], timeout[%u]ms", service_id, msg_seqid, timeout_ms);
        ret = IPC_HV_SOA_RET_TIMEOUT;
    }
    else if (wait_result != IPC_HV_SOA_COND_STATE_SUCCESS)
    {
        LOG_PRINT_ERROR("daemon processing failed, wait_result[%d], service_id[%u], msg_seqid[%u]",
                        wait_result, service_id, msg_seqid);
        ret = IPC_HV_SOA_RET_FAIL;
    }
    else if (ctx->data.header.service_id != service_id)
    {
        LOG_PRINT_ERROR("response service_id mismatch, expected[%u], got[%u], msg_seqid[%u]",
                        service_id, ctx->data.header.service_id, msg_seqid);
        ret = IPC_HV_SOA_RET_FAIL;
    }
    else
    {
        // Successfully received response
        LOG_PRINT_DEBUG("received response for service_id[%u], msg_seqid[%u], wait_result[%d]",
                        service_id, msg_seqid, wait_result);

        // Copy response data if caller provided buffer
        if (nullptr != resp_data && nullptr != resp_data_len)
        {
            // For safety, we limit the copy to LOCAL_REGISTRY_MSG_SIZE_MAX
            uint32_t copy_size = std::min(*resp_data_len, (uint32_t)LOCAL_REGISTRY_MSG_SIZE_MAX);
            if (copy_size > 0)
            {
                memcpy(resp_data, ctx->data.data, copy_size);
            }
            // Update the actual response length
            *resp_data_len = copy_size;
        }

        ret = IPC_HV_SOA_RET_SUCCESS;
    }

    // Remove from pending requests map
    {
        std::lock_guard<std::mutex> pending_lock(g_client->pending_requests_mutex);
        g_client->pending_requests.erase(msg_seqid);
    }

    return ret;
}

int32_t connect_with_daemon()
{
    g_client->m_daemon_io = hio_create_socket(g_client->m_main_loop, LOCAL_REGISTRY_SOCKET_PATH, -1, HIO_TYPE_SOCK_STREAM, HIO_CLIENT_SIDE);
    if (nullptr == g_client->m_daemon_io)
    {
        LOG_PRINT_ERROR("hio_create_socket fail!");
        return IPC_HV_SOA_RET_FAIL;
    }

    unlink(g_client->client_localaddr.c_str());
    struct sockaddr_un client_addr = {};
    client_addr.sun_family = AF_UNIX;
    strncpy(client_addr.sun_path, g_client->client_localaddr.c_str(), sizeof(client_addr.sun_path));
    if (bind(hio_fd(g_client->m_daemon_io), (struct sockaddr *)&client_addr, (socklen_t)SUN_LEN(&client_addr)) < 0)
    {
        LOG_PRINT_ERROR("bind %s fail, errno[%d](%s)!", client_addr.sun_path, errno, strerror(errno));
        return IPC_HV_SOA_RET_FAIL;
    }

    g_client->m_msg_seqid = 1;
    g_client->connect_ctx = std::make_shared<SyncContext<ipc_hv_soa_sync_context>>();
    g_client->connect_ctx->Reset();

    hio_setcb_close(g_client->m_daemon_io, on_close);
    hio_setcb_connect(g_client->m_daemon_io, on_connect);
    hio_set_connect_timeout(g_client->m_daemon_io, LOCAL_REGISTRY_COMMUNICATION_TIMEOUT_MS);
    if (0 != hio_connect(g_client->m_daemon_io))
    {
        LOG_PRINT_ERROR("hio_connect fail, errno[%d]!", hio_error(g_client->m_daemon_io));
        return IPC_HV_SOA_RET_FAIL;
    }

    int wait_result = g_client->connect_ctx->Wait(LOCAL_REGISTRY_COMMUNICATION_TIMEOUT_MS);
    if (wait_result < 0)
    {
        LOG_PRINT_ERROR("wait for daemon connection timeout, timeout[%u]ms", LOCAL_REGISTRY_COMMUNICATION_TIMEOUT_MS);
        return IPC_HV_SOA_RET_TIMEOUT;
    }
    if (wait_result != IPC_HV_SOA_COND_STATE_CONNECTED)
    {
        LOG_PRINT_ERROR("daemon connection failed, wait_result[%d]", wait_result);
        return IPC_HV_SOA_RET_FAIL;
    }

    LOG_PRINT_DEBUG("connected with daemon successfully");
    return IPC_HV_SOA_RET_SUCCESS;
}

int32_t connect_with_process_client(std::shared_ptr<ipc_hv_soa_process_client> client)
{
    int32_t ret = IPC_HV_SOA_RET_SUCCESS;
    hloop_t *idle_loop = get_idle_loop();

    if (nullptr != client->client_send_io)
    {
        hio_close(client->client_send_io);
        return IPC_HV_SOA_RET_FAIL;
    }

    client->client_send_io = hio_create_socket(idle_loop, client->client_localaddr1.c_str(), -1, HIO_TYPE_SOCK_STREAM, HIO_CLIENT_SIDE);
    if (nullptr == client->client_send_io)
    {
        LOG_PRINT_ERROR("hio_create_socket fail!");
        return IPC_HV_SOA_RET_FAIL;
    }

    struct sockaddr_un client_addr = {};
    client_addr.sun_family = AF_UNIX;
    snprintf(client_addr.sun_path, sizeof(client_addr.sun_path), LOCAL_REGISTEY_SOCKET_FMT2, g_client->client_id, g_client->client_name.c_str(), client->client_id);
    unlink(client_addr.sun_path);
    if (bind(hio_fd(client->client_send_io), (struct sockaddr *)&client_addr, (socklen_t)SUN_LEN(&client_addr)))
    {
        LOG_PRINT_ERROR("bind %s fail, errno[%d](%s)", client_addr.sun_path, errno, strerror(errno));
        return IPC_HV_SOA_RET_FAIL;
    }

    // init
    client->msg_seqid = 1;
    client->connect_ctx = std::make_shared<SyncContext<ipc_hv_soa_sync_context>>();
    client->connect_ctx->Reset();

    hio_setcb_close(client->client_send_io, on_close);
    hio_setcb_connect(client->client_send_io, on_connect);
    hio_set_connect_timeout(client->client_send_io, LOCAL_REGISTRY_COMMUNICATION_TIMEOUT_MS);
    void *userdata = (void *)(uintptr_t)client->client_id;
    hevent_set_userdata(client->client_send_io, userdata);
    if (0 != hio_connect(client->client_send_io))
    {
        LOG_PRINT_ERROR("hio_connect fail, errno[%d]!", hio_error(client->client_send_io));
        return IPC_HV_SOA_RET_FAIL;
    }

    // Wait for connection to complete
    int wait_result = client->connect_ctx->Wait(LOCAL_REGISTRY_COMMUNICATION_TIMEOUT_MS);
    if (wait_result < 0)
    {
        LOG_PRINT_ERROR("wait for process client connection timeout, timeout[%u]ms", LOCAL_REGISTRY_COMMUNICATION_TIMEOUT_MS);
        return IPC_HV_SOA_RET_TIMEOUT;
    }

    if (wait_result != IPC_HV_SOA_COND_STATE_CONNECTED)
    {
        LOG_PRINT_ERROR("connect with daemon fail, invalid result[%d]!", wait_result);
        return IPC_HV_SOA_RET_FAIL;
    }

    LOG_PRINT_INFO("connected with process client[%s], it be online", client->client_name.c_str());
    client->client_status = LOCAL_CLIENT_STATUS_ONLINE;

    return ret;
}

int32_t send_msg_to_process(std::shared_ptr<ipc_hv_soa_process_client> dest, uint32_t client_id, uint32_t msg_seqid, uint32_t msg_type, uint32_t service_id, uint32_t msg_len, const void *msgdata)
{
    if (nullptr == dest)
    {
        LOG_PRINT_ERROR("invalid params!");
        return IPC_HV_SOA_RET_ERR_ARG;
    }

    int32_t ret = IPC_HV_SOA_RET_SUCCESS;

    if (dest->client_status != LOCAL_CLIENT_STATUS_ONLINE)
    {
        ret = connect_with_process_client(dest);
    }
    else
    {
        if (nullptr == dest->client_send_io || !hio_is_opened(dest->client_send_io))
        {
            LOG_PRINT_ERROR("process client[%u][%p]is online but disconnect!", dest->client_id, dest->client_send_io);
            ret = connect_with_process_client(dest);
        }
    }

    if (IPC_HV_SOA_RET_SUCCESS == ret)
    {
        LOG_PRINT_DEBUG("client[%u] send service_id[%u] msg_type[%u] msg_seqid[%u] to client[%u]", client_id, service_id, msg_type, msg_seqid, dest->client_id);

        const pb_msgdesc_t *fields = nullptr;
        uint16_t module_id = (uint16_t)(service_id >> 16);
        uint16_t msg_id = (uint16_t)(service_id & 0xFFFF);
        const st_autolib_servicemap *pmap = gst_autolib_servicemap[module_id];
        if (nullptr == pmap)
        {
            LOG_PRINT_ERROR("module_id[%u] not found", module_id);
            return IPC_HV_SOA_RET_FAIL;
        }

        const st_autolib_servicemap_item *pitem = &pmap->items[msg_id - 1];
        if (nullptr == pitem)
        {
            LOG_PRINT_ERROR("service_id[%u] not found", service_id);
            return IPC_HV_SOA_RET_FAIL;
        }

        if (msg_type == E_IPC_HV_SOA_MSG_TYPE_METHOD_RESPONSE_SYNC || msg_type == E_IPC_HV_SOA_MSG_TYPE_METHOD_RESPONSE_ASYNC)
        {
            fields = pitem->out_fields;
        }
        else
        {
            fields = pitem->in_fields;
        }

        size_t encoded_size = 0;
        if (nullptr != msgdata && msg_len > 0)
        {
            if (!pb_get_encoded_size(&encoded_size, fields, msgdata))
            {
                LOG_PRINT_ERROR("pb_get_encoded_size service_id[%u] fail!", service_id);
                return IPC_HV_SOA_RET_FAIL;
            }
        }
        else
        {
            encoded_size = 0;
        }

        uint8_t buffer[LOCAL_REGISTRY_MSG_HEADER_SIZE + LOCAL_REGISTRY_MSG_SIZE_MAX] = {};
        if (nullptr != msgdata && encoded_size > 0)
        {
            pb_ostream_t stream = pb_ostream_from_buffer(buffer + LOCAL_REGISTRY_MSG_HEADER_SIZE, encoded_size);
            bool status = pb_encode(&stream, fields, msgdata);
            if (!status)
            {
                LOG_PRINT_ERROR("pb_encode service_id[%u] fail, error(%s)", service_id, PB_GET_ERROR(&stream));
                return IPC_HV_SOA_RET_FAIL;
            }
            LOG_PRINT_DEBUG("pb_encode service_id[%u] success", service_id);
        }

        st_local_msg_header send_msg_header = {};
        send_msg_header.client_id = client_id;
        send_msg_header.msg_seqid = msg_seqid;
        send_msg_header.msg_type = msg_type;
        send_msg_header.service_id = service_id;
        send_msg_header.msg_len = (uint32_t)encoded_size;
        memcpy(buffer, &send_msg_header, LOCAL_REGISTRY_MSG_HEADER_SIZE);

        ret = hio_write(dest->client_send_io, buffer, LOCAL_REGISTRY_MSG_HEADER_SIZE + encoded_size);
        if (ret < 0)
        {
            LOG_PRINT_ERROR("hio_write fail, ret[%d], error[%d]", ret, hio_error(dest->client_send_io));
            ret = IPC_HV_SOA_RET_FAIL;
        }
        else
        {
            ret = IPC_HV_SOA_RET_SUCCESS;
        }
    }
    else
    {
        LOG_PRINT_ERROR("connect with client[%u] fail!", dest->client_id);
    }

    return ret;
}

int32_t send_msg_to_process_sync(std::shared_ptr<ipc_hv_soa_process_client> dest, uint32_t client_id, uint32_t msg_seqid, uint32_t service_id, uint32_t msg_len, const void *msgdata, void *method_resp_data, uint32_t *method_resp_data_len, uint32_t timeout_ms)
{
    if (nullptr == dest)
    {
        LOG_PRINT_ERROR("invalid params!");
        return IPC_HV_SOA_RET_ERR_ARG;
    }

    int32_t ret = IPC_HV_SOA_RET_SUCCESS;

    if (dest->client_status != LOCAL_CLIENT_STATUS_ONLINE)
    {
        ret = connect_with_process_client(dest);
    }
    else
    {
        if (nullptr == dest->client_send_io || !hio_is_opened(dest->client_send_io))
        {
            LOG_PRINT_ERROR("process client[%u][%p]is online but disconnect!", dest->client_id, dest->client_send_io);
            ret = connect_with_process_client(dest);
        }
    }

    if (IPC_HV_SOA_RET_SUCCESS == ret)
    {
        LOG_PRINT_DEBUG("client[%u] send service_id[%u] msg_type[%u] msg_seqid[%u] to client[%u]", client_id, service_id, E_IPC_HV_SOA_MSG_TYPE_METHOD_REQUEST_SYNC, msg_seqid, dest->client_id);

        uint32_t msg_type = E_IPC_HV_SOA_MSG_TYPE_METHOD_REQUEST_SYNC;
        const pb_msgdesc_t *fields = nullptr;
        uint16_t module_id = (uint16_t)(service_id >> 16);
        uint16_t msg_id = (uint16_t)(service_id & 0xFFFF);
        const st_autolib_servicemap *pmap = gst_autolib_servicemap[module_id];
        if (nullptr == pmap)
        {
            LOG_PRINT_ERROR("module_id[%u] not found", module_id);
            return IPC_HV_SOA_RET_FAIL;
        }

        const st_autolib_servicemap_item *pitem = &pmap->items[msg_id - 1];
        if (nullptr == pitem)
        {
            LOG_PRINT_ERROR("service_id[%u] not found", service_id);
            return IPC_HV_SOA_RET_FAIL;
        }

        fields = pitem->in_fields;

        size_t encoded_size = 0;
        if (nullptr != msgdata && msg_len > 0)
        {
            if (!pb_get_encoded_size(&encoded_size, fields, msgdata))
            {
                LOG_PRINT_ERROR("pb_get_encoded_size service_id[%u] fail!", service_id);
                return IPC_HV_SOA_RET_FAIL;
            }
        }
        else
        {
            encoded_size = 0;
        }

        uint8_t buffer[LOCAL_REGISTRY_MSG_HEADER_SIZE + LOCAL_REGISTRY_MSG_SIZE_MAX] = {};
        if (nullptr != msgdata && encoded_size > 0)
        {
            pb_ostream_t stream = pb_ostream_from_buffer(buffer + LOCAL_REGISTRY_MSG_HEADER_SIZE, encoded_size);
            bool status = pb_encode(&stream, fields, msgdata);
            if (!status)
            {
                LOG_PRINT_ERROR("pb_encode service_id[%u] fail, error(%s)", service_id, PB_GET_ERROR(&stream));
                return IPC_HV_SOA_RET_FAIL;
            }
            LOG_PRINT_DEBUG("pb_encode service_id[%u] success", service_id);
        }

        st_local_msg_header send_msg_header = {};
        send_msg_header.client_id = client_id;
        send_msg_header.msg_seqid = msg_seqid;
        send_msg_header.msg_type = msg_type;
        send_msg_header.service_id = service_id;
        send_msg_header.msg_len = (uint32_t)encoded_size;
        memcpy(buffer, &send_msg_header, LOCAL_REGISTRY_MSG_HEADER_SIZE);

        // Borrow a sync context from the pool using RAII
        auto guard = dest->process_sync_pool.Borrow();
        if (!guard.IsValid())
        {
            LOG_PRINT_ERROR("failed to borrow sync context from pool");
            return IPC_HV_SOA_RET_FAIL;
        }
        auto ctx = guard.get();

        // Initialize context data
        ctx->data.header.client_id = 0;
        ctx->data.header.msg_seqid = msg_seqid;
        ctx->data.header.msg_type = E_IPC_HV_SOA_MSG_TYPE_METHOD_RESPONSE_SYNC;
        ctx->data.header.service_id = service_id;
        ctx->data.header.msg_len = 0;
        memset(ctx->data.data, 0x00, sizeof(ctx->data.data));
        ctx->result = -1;

        // Store context in pending_requests map for later completion
        {
            std::lock_guard<std::mutex> lock(dest->pending_requests_mutex);
            dest->pending_requests[msg_seqid] = guard.get_shared_ptr();
        }

        // Send message
        ret = hio_write(dest->client_send_io, buffer, LOCAL_REGISTRY_MSG_HEADER_SIZE + encoded_size);
        if (ret < 0)
        {
            LOG_PRINT_ERROR("hio_write fail, ret[%d], error[%d]", ret, hio_error(dest->client_send_io));
            ret = IPC_HV_SOA_RET_FAIL;

            // Remove from pending_requests on error
            {
                std::lock_guard<std::mutex> lock(dest->pending_requests_mutex);
                dest->pending_requests.erase(msg_seqid);
            }
        }
        else
        {
            ret = IPC_HV_SOA_RET_SUCCESS;
            // Wait for response with timeout
            int wait_result = ctx->Wait(timeout_ms);

            if (wait_result < 0)
            {
                LOG_PRINT_ERROR("wait response timeout for service_id[%u], msg_seqid[%u], timeout[%u]ms", service_id, msg_seqid, timeout_ms);
                ret = IPC_HV_SOA_RET_TIMEOUT;
            }
            else if (wait_result != IPC_HV_SOA_COND_STATE_SUCCESS)
            {
                LOG_PRINT_ERROR("response failed, ctx->result[%d], service_id[%u], msg_seqid[%u]", ctx->result, service_id, msg_seqid);
                ret = IPC_HV_SOA_RET_FAIL;
            }
            else if (ctx->data.header.service_id != service_id)
            {
                LOG_PRINT_ERROR("response service_id mismatch, expected[%u], got[%u], msg_seqid[%u]",
                                service_id, ctx->data.header.service_id, msg_seqid);
                ret = IPC_HV_SOA_RET_FAIL;
            }
            else
            {
                // Successfully received response
                LOG_PRINT_DEBUG("received response for service_id[%u], msg_seqid[%u], ctx->result[%d]",
                                service_id, msg_seqid, ctx->result);

                // Copy response data if caller provided buffer
                if (nullptr != method_resp_data && nullptr != method_resp_data_len)
                {
                    if (*method_resp_data_len < ctx->data.header.msg_len)
                    {
                        ret = IPC_HV_SOA_RET_ERR_ARG;
                    }
                    else
                    {
                        if (ctx->data.header.msg_len > 0)
                        {
                            memcpy(method_resp_data, ctx->data.data, ctx->data.header.msg_len);
                        }
                    }
                }

                if (nullptr != method_resp_data_len)
                {
                    *method_resp_data_len = ctx->data.header.msg_len;
                }

                ret = IPC_HV_SOA_RET_SUCCESS;
            }

            // Remove from pending_requests after completion
            {
                std::lock_guard<std::mutex> lock(dest->pending_requests_mutex);
                dest->pending_requests.erase(msg_seqid);
            }
        }
    }
    else
    {
        LOG_PRINT_ERROR("connect with client[%u] fail!", dest->client_id);
    }

    return ret;
}

int32_t register_client_req()
{
    int32_t ret = IPC_HV_SOA_RET_SUCCESS;

    st_register_client_req req = st_register_client_req_init_zero;
    req.client_pid = g_client->client_pid;
    std::strncpy(req.client_name, g_client->client_name.c_str(), sizeof(req.client_name));
    LOG_PRINT_DEBUG("register client id req client_pid[%d], client_name[%s]", req.client_pid, req.client_name);

    if (g_client->client_status != LOCAL_CLIENT_STATUS_ONLINE)
    {
        LOG_PRINT_ERROR("g_client is offline");
        return IPC_HV_SOA_RET_FAIL;
    }

    // Use the new sync function to send request and wait for response
    st_register_client_resp resp = st_register_client_resp_init_zero;
    uint32_t resp_size = sizeof(st_register_client_resp);

    ret = send_msg_to_daemon_sync(
        LOCAL_REGISTRY_SERVICE_ID_METHOD_REGISTER_CLIENT,
        E_IPC_HV_SOA_MSG_TYPE_METHOD_REQUEST_SYNC,
        &req,
        st_register_client_req_fields,
        st_register_client_req_size,
        &resp,
        &resp_size,
        LOCAL_REGISTRY_COMMUNICATION_TIMEOUT_MS);

    if (IPC_HV_SOA_RET_SUCCESS != ret)
    {
        LOG_PRINT_ERROR("send_msg_to_daemon_sync fail, ret[%d]!", ret);
        return ret;
    }

    // Validate response
    if (resp.client_pid != g_client->client_pid)
    {
        LOG_PRINT_ERROR("register client_id fail, client_pid[%" PRIdMAX "] != resp.client_pid[%d]",
                        (intmax_t)g_client->client_pid, resp.client_pid);
        return IPC_HV_SOA_RET_FAIL;
    }

    g_client->client_id = resp.client_id;
    LOG_PRINT_INFO("register client success, client_id[%u], client_pid[%" PRIdMAX "]",
                   g_client->client_id, (intmax_t)g_client->client_pid);

    return IPC_HV_SOA_RET_SUCCESS;
}

void process_msg_handler(void)
{
    while (1)
    {
        ipc_hv_soa_sync_context recv_data = {};
        if (!g_client->msg_handler_queue.Pop(recv_data))
        {
            break;
        }

        ipc_hv_soa_msg_handle_t handle = {};
        handle.client_id = recv_data.header.client_id;
        handle.msg_type = recv_data.header.msg_type;
        handle.msg_seqid = recv_data.header.msg_seqid;

        switch (recv_data.header.msg_type)
        {
            case E_IPC_HV_SOA_MSG_TYPE_METHOD_NOTIFY:
            case E_IPC_HV_SOA_MSG_TYPE_METHOD_REQUEST_SYNC:
            case E_IPC_HV_SOA_MSG_TYPE_METHOD_REQUEST_ASYNC:
                {
                    auto it = find_service(recv_data.header.service_id);
                    if (it == nullptr)
                    {
                        LOG_PRINT_ERROR("not find service_id[%u]", recv_data.header.service_id);
                        break;
                    }

                    PF_IPC_HV_SOA_METHOD_PROVIDER_CB cb = (PF_IPC_HV_SOA_METHOD_PROVIDER_CB)it->service_handler;
                    if (nullptr != cb)
                    {
                        cb(handle, recv_data.header.service_id, recv_data.data, recv_data.header.msg_len);
                    }
                }
                break;
            case E_IPC_HV_SOA_MSG_TYPE_METHOD_RESPONSE_SYNC:
                break;
            case E_IPC_HV_SOA_MSG_TYPE_METHOD_RESPONSE_ASYNC:
                {
                    auto it = find_service(recv_data.header.service_id);
                    if (it == nullptr)
                    {
                        LOG_PRINT_ERROR("not find service_id[%u]", recv_data.header.service_id);
                        break;
                    }
                    PF_IPC_HV_SOA_METHOD_ASYNC_CB cb = (PF_IPC_HV_SOA_METHOD_ASYNC_CB)it->service_async_handler;
                    if (nullptr != cb)
                    {
                        cb(recv_data.header.service_id, recv_data.data, recv_data.header.msg_len);
                    }
                }
                break;
            case E_IPC_HV_SOA_MSG_TYPE_EVENT_NOTIFY:
                {
                    auto it = find_service(recv_data.header.service_id);
                    if (it == nullptr)
                    {
                        LOG_PRINT_ERROR("not find service_id[%u]", recv_data.header.service_id);
                        break;
                    }

                    PF_IPC_HV_SOA_EVENT_LISTENER_CB cb = (PF_IPC_HV_SOA_EVENT_LISTENER_CB)it->service_handler;
                    if (nullptr != cb)
                    {
                        cb(recv_data.header.service_id, recv_data.data, recv_data.header.msg_len);
                    }
                }
                break;
            default:
                LOG_PRINT_ERROR("invalid msg_type[%u]", recv_data.header.msg_type);
                break;
        }
    }
}

int32_t ipc_hv_soa_inn_trigger_to_client(std::shared_ptr<ipc_hv_soa_service> service, void *event_data, uint32_t event_data_len, std::shared_ptr<ipc_hv_soa_process_client> client)
{
    if (!g_init_flag)
    {
        return IPC_HV_SOA_RET_ERR_ARG;
    }

    if (nullptr == g_client || nullptr == service || nullptr == client)
    {
        return IPC_HV_SOA_RET_ERR_ARG;
    }

    int32_t ret = IPC_HV_SOA_RET_SUCCESS;

    if (nullptr == service->service_provider)
    {
        LOG_PRINT_ERROR("service[%u] is no provider", service->service_id);
        return IPC_HV_SOA_RET_ERR_ARG;
    }

    uint32_t msg_seqid = service->service_provider->msg_seqid++;
    if (nullptr == event_data || 0 == event_data_len)
    {
        ret = send_msg_to_process(client, g_client->client_id, msg_seqid, E_IPC_HV_SOA_MSG_TYPE_EVENT_NOTIFY, service->service_id, 0, nullptr);
    }
    else
    {
        ret = send_msg_to_process(client, g_client->client_id, msg_seqid, E_IPC_HV_SOA_MSG_TYPE_EVENT_NOTIFY, service->service_id, event_data_len, event_data);
    }

    if (IPC_HV_SOA_RET_SUCCESS != ret)
    {
        LOG_PRINT_ERROR("ipc_hv_soa_inn_trigger_to_client[%u] fail", service->service_id);
    }

    return ret;
}

int32_t ipc_hv_soa_inn_sync_complete(uint32_t service_id, uint32_t msg_type, uint32_t msg_seqid, void *method_resp_data, uint32_t method_resp_data_len)
{
    if (!g_init_flag)
    {
        return IPC_HV_SOA_RET_ERR_ARG;
    }

    int32_t ret = IPC_HV_SOA_RET_SUCCESS;

    auto it = find_service(service_id);
    if (nullptr == it)
    {
        LOG_PRINT_ERROR("service[%u] not find", service_id);
        return IPC_HV_SOA_RET_ERR_ARG;
    }

    // Find the service provider (the client that sent the request)
    std::shared_ptr<ipc_hv_soa_process_client> provider = it->service_provider;
    if (nullptr == provider)
    {
        LOG_PRINT_ERROR("service[%u] has no provider", service_id);
        return IPC_HV_SOA_RET_ERR_ARG;
    }

    // Search for the waiting context in pending_requests by msg_seqid
    std::shared_ptr<SyncContext<ipc_hv_soa_sync_context>> ctx_ptr = nullptr;
    {
        std::lock_guard<std::mutex> lock(provider->pending_requests_mutex);
        auto ctx_it = provider->pending_requests.find(msg_seqid);
        if (ctx_it == provider->pending_requests.end())
        {
            LOG_PRINT_ERROR("no waiting context found for msg_seqid[%u] on client[%u]", msg_seqid, provider->client_id);
            return IPC_HV_SOA_RET_FAIL;
        }
        ctx_ptr = ctx_it->second;
        // Remove from pending_requests after finding
        provider->pending_requests.erase(ctx_it);
    }

    if (nullptr == ctx_ptr)
    {
        LOG_PRINT_ERROR("context pointer is null for msg_seqid[%u]", msg_seqid);
        return IPC_HV_SOA_RET_FAIL;
    }

    // Lock the context and set the response data
    {
        std::lock_guard<std::mutex> lock(ctx_ptr->mutex);
        ctx_ptr->data.header.client_id = g_client->client_id;
        ctx_ptr->data.header.msg_seqid = msg_seqid;
        ctx_ptr->data.header.msg_type = msg_type;
        ctx_ptr->data.header.service_id = service_id;
        ctx_ptr->data.header.msg_len = method_resp_data_len;
        if (nullptr != method_resp_data && method_resp_data_len > 0)
        {
            memcpy(ctx_ptr->data.data, method_resp_data, method_resp_data_len);
        }
    }

    // Set result to wake up the waiting thread
    ctx_ptr->SetResult(IPC_HV_SOA_COND_STATE_SUCCESS);

    LOG_PRINT_DEBUG("completed sync request for service_id[%u], msg_seqid[%u]", service_id, msg_seqid);

    return ret;
}
