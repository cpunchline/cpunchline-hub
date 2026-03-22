#include "ipc_hv_soa_common.hpp"

extern std::atomic_bool g_init_flag;
extern std::shared_ptr<ipc_hv_soa_client> g_client;

// Internal helper function to encode and send message to daemon
static int32_t _send_msg_to_daemon_internal(uint32_t msg_id, const void *msgdata, const pb_msgdesc_t *fields, uint32_t field_size)
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

    LOG_PRINT_DEBUG("send msg_id[%d] to daemon fd[%d]", msg_id, hio_fd(g_client->m_daemon_io));

    // Encode message
    std::vector<uint8_t> buffer(LOCAL_REGISTRY_MSG_HEADER_SIZE + field_size, 0);
    pb_ostream_t stream = pb_ostream_from_buffer(buffer.data() + LOCAL_REGISTRY_MSG_HEADER_SIZE, field_size);
    bool status = pb_encode(&stream, fields, msgdata);
    if (!status)
    {
        LOG_PRINT_ERROR("pb_encode msg_id[%d] fail, error(%s)", msg_id, PB_GET_ERROR(&stream));
        return IPC_HV_SOA_RET_ERR_MSG_ENCODE;
    }

    // Build message header
    st_local_msg_header send_msg_header = {};
    send_msg_header.msg_id = msg_id;
    send_msg_header.msg_len = (uint32_t)stream.bytes_written;
    memcpy(buffer.data(), &send_msg_header, sizeof(send_msg_header));

    // Check connection and send
    if (!hio_is_opened(g_client->m_daemon_io))
    {
        LOG_PRINT_ERROR("hio_write fail, fd[%d] disconnect", hio_fd(g_client->m_daemon_io));
        return IPC_HV_SOA_RET_FAIL;
    }

    ret = hio_write(g_client->m_daemon_io, buffer.data(), LOCAL_REGISTRY_MSG_HEADER_SIZE + stream.bytes_written);
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

int32_t send_msg_to_daemon(uint32_t msg_id, const void *msgdata, const pb_msgdesc_t *fileds, uint32_t field_size)
{
    return _send_msg_to_daemon_internal(msg_id, msgdata, fileds, field_size);
}

int32_t send_msg_to_daemon_sync(uint32_t msg_id, const void *msgdata, const pb_msgdesc_t *fields, uint32_t field_size, void *resp_data, uint32_t *resp_data_len, uint32_t timeout_ms)
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

    // Lock and reset condition state before sending
    std::unique_lock<std::mutex> daemon_lock(g_client->daemo_mutex);
    g_client->daemon_cond_msgid = 0;
    memset(g_client->daemon_cond_msgdata, 0x00, sizeof(g_client->daemon_cond_msgdata));
    g_client->daemon_cond_ret = IPC_HV_SOA_COND_STATE_INIT;

    // Send message using internal helper function
    int32_t ret = _send_msg_to_daemon_internal(msg_id, msgdata, fields, field_size);
    if (IPC_HV_SOA_RET_SUCCESS != ret)
    {
        LOG_PRINT_ERROR("_send_msg_to_daemon_internal fail, ret[%d]", ret);
        // Clean condition state on error
        g_client->daemon_cond_msgid = 0;
        g_client->daemon_cond_ret = IPC_HV_SOA_COND_STATE_INIT;
        return ret;
    }

    auto timeout_duration = std::chrono::milliseconds(timeout_ms);
    bool ready = g_client->daemon_cond.wait_for(daemon_lock, timeout_duration, []
                                                {
                                                    return (g_client->daemon_cond_ret != IPC_HV_SOA_COND_STATE_INIT);
                                                });

    if (!ready)
    {
        LOG_PRINT_ERROR("wait response timeout for msg_id[%d], timeout[%u]ms", msg_id, timeout_ms);
        ret = IPC_HV_SOA_RET_TIMEOUT;
    }
    else if (g_client->daemon_cond_ret != IPC_HV_SOA_COND_STATE_SUCCESS)
    {
        LOG_PRINT_ERROR("daemon processing failed, daemon_cond_ret[%d], msg_id[%d]",
                        g_client->daemon_cond_ret, msg_id);
        ret = IPC_HV_SOA_RET_FAIL;
    }
    else if (g_client->daemon_cond_msgid != msg_id)
    {
        LOG_PRINT_ERROR("response msg_id mismatch, expected[%d], got[%d]",
                        msg_id, g_client->daemon_cond_msgid);
        ret = IPC_HV_SOA_RET_FAIL;
    }
    else
    {
        // Successfully received response
        LOG_PRINT_DEBUG("received response for msg_id[%d], daemon_cond_ret[%d]",
                        msg_id, g_client->daemon_cond_ret);

        // Copy response data if caller provided buffer
        if (nullptr != resp_data && nullptr != resp_data_len)
        {
            // Note: The actual response size depends on the message type.
            // Caller should provide a buffer large enough based on the expected response type.
            // We copy up to the buffer size provided by caller.
            // In practice, for known msg_id, caller knows the expected response structure.

            // For safety, we limit the copy to LOCAL_REGISTRY_MSG_SIZE_MAX
            uint32_t copy_size = std::min(*resp_data_len, (uint32_t)LOCAL_REGISTRY_MSG_SIZE_MAX);
            if (copy_size > 0)
            {
                memcpy(resp_data, g_client->daemon_cond_msgdata, copy_size);
            }
            // Update the actual response length
            *resp_data_len = copy_size;
        }

        ret = IPC_HV_SOA_RET_SUCCESS;
    }

    // Clean condition state
    g_client->daemon_cond_msgid = 0;
    g_client->daemon_cond_ret = IPC_HV_SOA_COND_STATE_INIT;

    return ret;
}

int32_t connect_with_daemon()
{
    int32_t ret = IPC_HV_SOA_RET_SUCCESS;

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

    std::unique_lock daemon_lock(g_client->daemo_mutex);

    // init
    g_client->daemon_cond_msgid = 0;
    memset(g_client->daemon_cond_msgdata, 0x00, sizeof(g_client->daemon_cond_msgdata));
    g_client->daemon_cond_ret = IPC_HV_SOA_COND_STATE_INIT;

    hio_setcb_close(g_client->m_daemon_io, on_close);
    hio_setcb_connect(g_client->m_daemon_io, on_connect);
    hio_set_connect_timeout(g_client->m_daemon_io, LOCAL_REGISTRY_COMMUNICATION_TIMEOUT_MS);
    if (0 != hio_connect(g_client->m_daemon_io))
    {
        LOG_PRINT_ERROR("hio_connect fail, errno[%d]!", hio_error(g_client->m_daemon_io));
        return IPC_HV_SOA_RET_FAIL;
    }

    auto timeout = std::chrono::milliseconds(LOCAL_REGISTRY_COMMUNICATION_TIMEOUT_MS);
    bool ready = g_client->daemon_cond.wait_for(daemon_lock, timeout, []
                                                {
                                                    return (g_client->daemon_cond_ret != IPC_HV_SOA_COND_STATE_INIT);
                                                });
    if (!ready)
    {
        ret = IPC_HV_SOA_RET_TIMEOUT;
    }
    else
    {
        if (g_client->daemon_cond_ret != IPC_HV_SOA_COND_STATE_CONNECTED)
        {
            LOG_PRINT_ERROR("connect with daemon fail, invalid daemon_cond_ret[%d]!", g_client->daemon_cond_ret);
            ret = IPC_HV_SOA_RET_FAIL;
        }
        else
        {
            g_client->client_status = LOCAL_CLIENT_STATUS_ONLINE;
        }
    }

    // clean
    g_client->daemon_cond_msgid = 0;
    g_client->daemon_cond_ret = IPC_HV_SOA_COND_STATE_INIT;

    return ret;
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

    std::unique_lock send_msg_lock(client->send_msg_mutex);

    // init
    client->send_msg_seqid = 0;
    client->send_msg_map.clear();
    client->send_msg_cond_ret = IPC_HV_SOA_COND_STATE_INIT;

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

    auto timeout = std::chrono::milliseconds(LOCAL_REGISTRY_COMMUNICATION_TIMEOUT_MS);
    bool ready = client->send_msg_cond.wait_for(send_msg_lock, timeout, [&client]
                                                {
                                                    return (client->send_msg_cond_ret != IPC_HV_SOA_COND_STATE_INIT);
                                                });
    if (!ready)
    {
        ret = IPC_HV_SOA_RET_TIMEOUT;
    }
    else
    {
        if (client->send_msg_cond_ret != IPC_HV_SOA_COND_STATE_CONNECTED)
        {
            LOG_PRINT_ERROR("connect with daemon fail, invalid send_msg_cond_ret[%d]!", client->send_msg_cond_ret);
            ret = IPC_HV_SOA_RET_FAIL;
        }
        else
        {
            LOG_PRINT_INFO("connected with process client[%s], it be online", client->client_name.c_str());
            client->client_status = LOCAL_CLIENT_STATUS_ONLINE;
        }
    }

    // clean
    client->send_msg_cond_ret = IPC_HV_SOA_COND_STATE_INIT;

    return ret;
}

int32_t send_msg_to_process(std::shared_ptr<ipc_hv_soa_process_client> dest, uint32_t client_id, uint32_t msg_seqid, uint32_t msg_type, uint32_t service_id, uint32_t msgdata_len, const void *msgdata)
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
            LOG_PRINT_ERROR("process client[%d][%p]is online but disconnect!", dest->client_id, dest->client_send_io);
            ret = connect_with_process_client(dest);
        }
    }

    if (IPC_HV_SOA_RET_SUCCESS == ret)
    {
        LOG_PRINT_DEBUG("client[%d] send msg_id[%d] msg_type[%d] msg_seqid[%d] to client[%d]", client_id, service_id, msg_type, msg_seqid, dest->client_id);

#if NANOPB_SUPPORT_OPTION
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
        if (nullptr != msgdata && msgdata_len > 0)
        {
            if (!pb_get_encoded_size(&encoded_size, fields, msgdata))
            {
                LOG_PRINT_ERROR("pb_get_encoded_size msg_id[%d] fail!", service_id);
                return IPC_HV_SOA_RET_FAIL;
            }
        }
        else
        {
            encoded_size = 0;
        }

        std::vector<uint8_t> buffer(LOCAL_REGISTRY_MSG_PROCESS_HEADER_SIZE + encoded_size);
        memcpy(buffer.data() + 0 * sizeof(uint32_t), &client_id, sizeof(uint32_t));
        memcpy(buffer.data() + 1 * sizeof(uint32_t), &msg_seqid, sizeof(uint32_t));
        memcpy(buffer.data() + 2 * sizeof(uint32_t), &msg_type, sizeof(uint32_t));
        memcpy(buffer.data() + 3 * sizeof(uint32_t), &service_id, sizeof(uint32_t));
        memcpy(buffer.data() + 4 * sizeof(uint32_t), &encoded_size, sizeof(uint32_t));

        if (nullptr != msgdata && encoded_size > 0)
        {
            pb_ostream_t stream = pb_ostream_from_buffer(buffer.data() + LOCAL_REGISTRY_MSG_PROCESS_HEADER_SIZE, encoded_size);
            bool status = pb_encode(&stream, fields, msgdata);
            if (!status)
            {
                LOG_PRINT_ERROR("pb_encode msg_id[%d] fail, error(%s)", service_id, PB_GET_ERROR(&stream));
                return IPC_HV_SOA_RET_FAIL;
            }
            LOG_PRINT_DEBUG("pb_encode service_id[%d] success", service_id);
        }
#else
        std::vector<uint8_t> buffer(LOCAL_REGISTRY_MSG_PROCESS_HEADER_SIZE + msgdata_len);
        memcpy(buffer.data() + 0 * sizeof(uint32_t), &client_id, sizeof(uint32_t));
        memcpy(buffer.data() + 1 * sizeof(uint32_t), &msg_seqid, sizeof(uint32_t));
        memcpy(buffer.data() + 2 * sizeof(uint32_t), &msg_type, sizeof(uint32_t));
        memcpy(buffer.data() + 3 * sizeof(uint32_t), &service_id, sizeof(uint32_t));
        memcpy(buffer.data() + 4 * sizeof(uint32_t), &msgdata_len, sizeof(uint32_t));
        if (nullptr != msgdata && msgdata_len > 0)
        {
            memcpy(buffer.data() + LOCAL_REGISTRY_MSG_PROCESS_HEADER_SIZE, msgdata, msgdata_len);
        }
#endif

        std::lock_guard<std::mutex> send_msg_lock(dest->send_msg_mutex);
        ret = hio_write(dest->client_send_io, buffer.data(), buffer.size());
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
        LOG_PRINT_ERROR("connect with client[%d] fail!", dest->client_id);
    }

    return ret;
}

int32_t send_msg_to_process_sync(std::shared_ptr<ipc_hv_soa_process_client> dest, uint32_t client_id, uint32_t msg_seqid, uint32_t service_id, uint32_t msgdata_len, const void *msgdata, void *method_resp_data, uint32_t *method_resp_data_len, uint32_t timeout_ms)
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
            LOG_PRINT_ERROR("process client[%d][%p]is online but disconnect!", dest->client_id, dest->client_send_io);
            ret = connect_with_process_client(dest);
        }
    }

    if (IPC_HV_SOA_RET_SUCCESS == ret)
    {
        ipc_hv_soa_process_client_data expected_resp_data = {};
        ipc_hv_soa_process_client_data real_resp_data = {};

        LOG_PRINT_DEBUG("client[%d] send msg_id[%d] msg_type[%d] msg_seqid[%d] to client[%d]", client_id, service_id, E_IPC_HV_SOA_MSG_TYPE_METHOD_REQUEST_SYNC, msg_seqid, dest->client_id);
        expected_resp_data.service_id = service_id;
        expected_resp_data.msg_type = E_IPC_HV_SOA_MSG_TYPE_METHOD_RESPONSE_SYNC;
        expected_resp_data.msg_seqid = msg_seqid;
        expected_resp_data.client_id = 0;
        expected_resp_data.is_complete = false;
        expected_resp_data.data_len = 0;
        memset(expected_resp_data.data, 0x00, sizeof(expected_resp_data.data));
        uint64_t key = ((uint64_t)service_id << 32) | (uint64_t)msg_seqid;

        uint32_t msg_type = E_IPC_HV_SOA_MSG_TYPE_METHOD_REQUEST_SYNC;
#if NANOPB_SUPPORT_OPTION
        (void)msgdata_len;
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
        if (nullptr != msgdata && msgdata_len > 0)
        {
            if (!pb_get_encoded_size(&encoded_size, fields, msgdata))
            {
                LOG_PRINT_ERROR("pb_get_encoded_size msg_id[%d] fail!", service_id);
                return IPC_HV_SOA_RET_FAIL;
            }
        }
        else
        {
            encoded_size = 0;
        }

        std::vector<uint8_t> buffer(LOCAL_REGISTRY_MSG_PROCESS_HEADER_SIZE + encoded_size);
        memcpy(buffer.data() + 0 * sizeof(uint32_t), &client_id, sizeof(uint32_t));
        memcpy(buffer.data() + 1 * sizeof(uint32_t), &msg_seqid, sizeof(uint32_t));
        memcpy(buffer.data() + 2 * sizeof(uint32_t), &msg_type, sizeof(uint32_t));
        memcpy(buffer.data() + 3 * sizeof(uint32_t), &service_id, sizeof(uint32_t));
        memcpy(buffer.data() + 4 * sizeof(uint32_t), &encoded_size, sizeof(uint32_t));

        if (nullptr != msgdata && encoded_size > 0)
        {
            pb_ostream_t stream = pb_ostream_from_buffer(buffer.data() + LOCAL_REGISTRY_MSG_PROCESS_HEADER_SIZE, encoded_size);
            bool status = pb_encode(&stream, fields, msgdata);
            if (!status)
            {
                LOG_PRINT_ERROR("pb_encode msg_id[%d] fail, error(%s)", service_id, PB_GET_ERROR(&stream));
                return IPC_HV_SOA_RET_FAIL;
            }
            LOG_PRINT_DEBUG("pb_encode service_id[%d] success", service_id);
        }
#else
        std::vector<uint8_t> buffer(LOCAL_REGISTRY_MSG_PROCESS_HEADER_SIZE + msgdata_len);
        memcpy(buffer.data() + 0 * sizeof(uint32_t), &client_id, sizeof(uint32_t));
        memcpy(buffer.data() + 1 * sizeof(uint32_t), &msg_seqid, sizeof(uint32_t));
        memcpy(buffer.data() + 2 * sizeof(uint32_t), &msg_type, sizeof(uint32_t));
        memcpy(buffer.data() + 3 * sizeof(uint32_t), &service_id, sizeof(uint32_t));
        memcpy(buffer.data() + 4 * sizeof(uint32_t), &msgdata_len, sizeof(uint32_t));
        if (nullptr != msgdata && msgdata_len > 0)
        {
            memcpy(buffer.data() + LOCAL_REGISTRY_MSG_PROCESS_HEADER_SIZE, msgdata, msgdata_len);
        }
#endif

        std::unique_lock<std::mutex> send_msg_lock(dest->send_msg_mutex);
        dest->send_msg_map.insert({key, expected_resp_data});
        ret = hio_write(dest->client_send_io, buffer.data(), buffer.size());
        if (ret < 0)
        {
            LOG_PRINT_ERROR("hio_write fail, ret[%d], error[%d]", ret, hio_error(dest->client_send_io));
            ret = IPC_HV_SOA_RET_FAIL;
        }
        else
        {
            ret = IPC_HV_SOA_RET_SUCCESS;
            // init
            auto timeout = std::chrono::milliseconds(timeout_ms);
            dest->send_msg_cond_ret = -1;
            do
            {
                bool ready = dest->send_msg_cond.wait_for(send_msg_lock, timeout, [&dest]
                                                          {
                                                              return (dest->send_msg_cond_ret != -1);
                                                          });
                if (!ready)
                {
                    ret = IPC_HV_SOA_RET_TIMEOUT;
                    break;
                }

                auto it = dest->send_msg_map.find(key);
                if (it != dest->send_msg_map.end())
                {
                    real_resp_data = it->second;
                    if (real_resp_data.is_complete)
                    {
                        ret = IPC_HV_SOA_RET_SUCCESS;
                    }
                    else
                    {
                        ret = IPC_HV_SOA_RET_ERR_OTHER;
                    }
                }
                else
                {
                    ret = IPC_HV_SOA_RET_FAIL;
                }
                break;
            } while (1);

            if (IPC_HV_SOA_RET_SUCCESS == ret)
            {
                if (nullptr != method_resp_data && nullptr != method_resp_data_len)
                {
                    if (*method_resp_data_len < real_resp_data.data_len)
                    {
                        ret = IPC_HV_SOA_RET_ERR_ARG;
                    }
                    else
                    {
                        if (real_resp_data.data_len > 0)
                        {
                            memcpy(method_resp_data, real_resp_data.data, real_resp_data.data_len);
                        }
                    }
                }

                if (nullptr != method_resp_data_len)
                {
                    *method_resp_data_len = real_resp_data.data_len;
                }
            }
            else
            {
                LOG_PRINT_ERROR("ipc_hv_soa_method_sync[%d] fail, ret[%d]", service_id, ret);
            }
        }
        dest->send_msg_map.erase(key);
    }
    else
    {
        LOG_PRINT_ERROR("connect with client[%d] fail!", dest->client_id);
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
        LOG_PRINT_ERROR("register client_id fail, client_pid[%d] != resp.client_pid[%d]",
                        g_client->client_pid, resp.client_pid);
        return IPC_HV_SOA_RET_FAIL;
    }

    g_client->client_id = resp.client_id;
    LOG_PRINT_INFO("register client success, client_id[%u], client_pid[%u]",
                   g_client->client_id, g_client->client_pid);

    return IPC_HV_SOA_RET_SUCCESS;
}

void process_msg_handler(void)
{
    while (1)
    {
        ipc_hv_soa_process_client_data recv_data = {};
        if (!g_client->msg_handler_queue.Pop(recv_data))
        {
            break;
        }

        ipc_hv_soa_msg_handle_t handle = {};
        handle.client_id = recv_data.client_id;
        handle.msg_type = recv_data.msg_type;
        handle.msg_seqid = recv_data.msg_seqid;

        switch (recv_data.msg_type)
        {
            case E_IPC_HV_SOA_MSG_TYPE_METHOD_NOTIFY:
            case E_IPC_HV_SOA_MSG_TYPE_METHOD_REQUEST_SYNC:
            case E_IPC_HV_SOA_MSG_TYPE_METHOD_REQUEST_ASYNC:
                {
                    auto it = find_service(recv_data.service_id);
                    if (it == nullptr)
                    {
                        LOG_PRINT_ERROR("not find service_id[%u]", recv_data.service_id);
                        break;
                    }

                    PF_IPC_HV_SOA_METHOD_PROVIDER_CB cb = (PF_IPC_HV_SOA_METHOD_PROVIDER_CB)it->service_handler;
                    if (nullptr != cb)
                    {
                        cb(handle, recv_data.service_id, recv_data.data, recv_data.data_len);
                    }
                }
                break;
            case E_IPC_HV_SOA_MSG_TYPE_METHOD_RESPONSE_SYNC:
                break;
            case E_IPC_HV_SOA_MSG_TYPE_METHOD_RESPONSE_ASYNC:
                {
                    auto it = find_service(recv_data.service_id);
                    if (it == nullptr)
                    {
                        LOG_PRINT_ERROR("not find service_id[%u]", recv_data.service_id);
                        break;
                    }
                    PF_IPC_HV_SOA_METHOD_ASYNC_CB cb = (PF_IPC_HV_SOA_METHOD_ASYNC_CB)it->service_async_handler;
                    if (nullptr != cb)
                    {
                        cb(recv_data.service_id, recv_data.data, recv_data.data_len);
                    }
                }
                break;
            case E_IPC_HV_SOA_MSG_TYPE_EVENT:
                {
                    auto it = find_service(recv_data.service_id);
                    if (it == nullptr)
                    {
                        LOG_PRINT_ERROR("not find service_id[%u]", recv_data.service_id);
                        break;
                    }

                    PF_IPC_HV_SOA_EVENT_LISTENER_CB cb = (PF_IPC_HV_SOA_EVENT_LISTENER_CB)it->service_handler;
                    if (nullptr != cb)
                    {
                        cb(recv_data.service_id, recv_data.data, recv_data.data_len);
                    }
                }
                break;
            default:
                LOG_PRINT_ERROR("invalid msg_type[%u]", recv_data.msg_type);
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
        LOG_PRINT_ERROR("service[%d] is no provider", service->service_id);
        return IPC_HV_SOA_RET_ERR_ARG;
    }

    uint32_t msg_seqid = service->service_provider->send_msg_seqid++;
    if (nullptr == event_data || 0 == event_data_len)
    {
        ret = send_msg_to_process(client, g_client->client_id, msg_seqid, E_IPC_HV_SOA_MSG_TYPE_EVENT, service->service_id, 0, nullptr);
    }
    else
    {
        ret = send_msg_to_process(client, g_client->client_id, msg_seqid, E_IPC_HV_SOA_MSG_TYPE_EVENT, service->service_id, event_data_len, event_data);
    }

    if (IPC_HV_SOA_RET_SUCCESS != ret)
    {
        LOG_PRINT_ERROR("ipc_hv_soa_inn_trigger_to_client[%d] fail", service->service_id);
    }

    return ret;
}

int32_t ipc_hv_soa_inn_sync_complete(uint32_t service_id, uint32_t msg_seqid, void *method_resp_data, uint32_t method_resp_data_len)
{
    if (!g_init_flag)
    {
        return IPC_HV_SOA_RET_ERR_ARG;
    }

    int32_t ret = IPC_HV_SOA_RET_SUCCESS;

    auto it = find_service(service_id);
    if (nullptr == it)
    {
        LOG_PRINT_ERROR("service[%d] not find", service_id);
        return IPC_HV_SOA_RET_ERR_ARG;
    }

    std::unique_lock<std::mutex> send_msg_lock(it->service_provider->send_msg_mutex);
    bool getsync = false;

    uint64_t key = ((uint64_t)service_id << 32) | (uint64_t)msg_seqid;
    auto it1 = it->service_provider->send_msg_map.find(key);
    if (it1 != it->service_provider->send_msg_map.end())
    {
        getsync = true;
    }
    else
    {
        getsync = false;
    }

    if (getsync)
    {
        it1->second.is_complete = true;
        it1->second.data_len = method_resp_data_len;
        if (method_resp_data_len > 0)
        {
            memcpy(it1->second.data, method_resp_data, method_resp_data_len);
        }
        else
        {
            memset(it1->second.data, 0x00, sizeof(it1->second.data));
        }
        it->service_provider->send_msg_cond_ret = 0;
        it->service_provider->send_msg_cond.notify_one();
    }
    else
    {
        LOG_PRINT_ERROR("invalid service[%d]", service_id);
        ret = IPC_HV_SOA_RET_ERR_ARG;
    }

    return ret;
}
