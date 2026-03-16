#include "ipc_hv_soa_manager.hpp"

void ipc_hv_soa_manager::onDaemonConnection(const hv::SocketChannelPtr &channel)
{
    auto manager = ipc_hv_soa_manager::instance();
    if (channel->isConnected())
    {
        LOG_PRINT_DEBUG("connected to daemon %s connfd=%d id=%d tid=%ld\n", channel->peeraddr().c_str(), channel->fd(), channel->id(), currentThreadEventLoop->tid());
        std::shared_ptr<ipc_hv_soa_client_sync_ctx> p_connect_sync_ctx = find_sync_ctx_by_dest(E_IPC_HV_SOA_CLIENT_SYNC_CTX_TYPE_DAEMON_CONNECT, MODULE_ID_AUTOLIB_LOCAL_REGISTRY);
        if (nullptr != p_connect_sync_ctx)
        {
            {
                std::lock_guard<std::mutex> lock(p_connect_sync_ctx->mutex);
                p_connect_sync_ctx->ret = INT32_MIN;
            }
            p_connect_sync_ctx->cond.notify_one();
        }
        manager->m_client.client_status = LOCAL_CLIENT_STATUS_ONLINE;
    }
    else
    {
        LOG_PRINT_DEBUG("disconnected to daemon %s connfd=%d id=%d tid=%ld\n", channel->peeraddr().c_str(), channel->fd(), channel->id(), currentThreadEventLoop->tid());
        std::shared_ptr<ipc_hv_soa_client_sync_ctx> p_connect_sync_ctx = find_sync_ctx_by_dest(E_IPC_HV_SOA_CLIENT_SYNC_CTX_TYPE_DAEMON_DISCONNECT, MODULE_ID_AUTOLIB_LOCAL_REGISTRY);
        if (nullptr != p_connect_sync_ctx)
        {
            {
                std::lock_guard<std::mutex> lock(p_connect_sync_ctx->mutex);
                p_connect_sync_ctx->ret = INT32_MAX;
            }
            p_connect_sync_ctx->cond.notify_one();
        }
        manager->m_client.client_status = LOCAL_CLIENT_STATUS_OFFLINE;
    }
}

void ipc_hv_soa_manager::onDaemonMessage(const hv::SocketChannelPtr &channel, hv::Buffer *inbuf)
{
    LOG_PRINT_DEBUG("onDaemonMessage channel_id[%u], fd[%d], readbytes[%zu]", channel->id(), channel->fd(), inbuf->size());
    auto manager = ipc_hv_soa_manager::instance();
    if (inbuf->size() < (int)LOCAL_REGISTRY_MSG_HEADER_SIZE)
    {
        LOG_PRINT_ERROR("not a complete msg");
        return;
    }

    bool status = false;
    uint32_t real_readbytes = (uint32_t)((uint32_t)inbuf->size() - LOCAL_REGISTRY_MSG_HEADER_SIZE);
    st_local_msg_header *recv_msg_header = (st_local_msg_header *)inbuf->data();
    if (real_readbytes != recv_msg_header->msg_len)
    {
        LOG_PRINT_ERROR("msg_len[%u] != real_readbytes[%u]", recv_msg_header->msg_len, real_readbytes);
        return;
    }
    LOG_PRINT_DEBUG("onDaemonMessage msg_id[%u], msg_len[%u]", recv_msg_header->msg_id, recv_msg_header->msg_len);
    int32_t ret = IPC_HV_SOA_RET_SUCCESS;
    char resp[LOCAL_REGISTRY_MSG_SIZE_MAX] = {};
    uint32_t resp_size = 0;
    uint8_t *buffer = (uint8_t *)inbuf->data() + LOCAL_REGISTRY_MSG_HEADER_SIZE;
    pb_istream_t stream = pb_istream_from_buffer(buffer, recv_msg_header->msg_len);
    uint16_t module_id = (uint16_t)(recv_msg_header->msg_id >> 16);
    uint16_t msg_id = (uint16_t)(recv_msg_header->msg_id & 0xFFFF);

    if (module_id != MODULE_ID_AUTOLIB_LOCAL_REGISTRY)
    {
        LOG_PRINT_ERROR("module_id[%u] != [%u]", module_id, MODULE_ID_AUTOLIB_LOCAL_REGISTRY);
        return;
    }

    switch (msg_id)
    {
        case LOCAL_REGISTRY_SERVICE_ID_METHOD_REGISTER_CLIENT:
            {
                resp_size = sizeof(st_register_client_resp);
                *(st_register_client_resp *)resp = st_register_client_resp_init_zero;
                status = pb_decode(&stream, st_register_client_resp_fields, resp);
                if (!status)
                {
                    LOG_PRINT_ERROR("pb_decode msg_id[%d] fail, error(%s)", recv_msg_header->msg_id, PB_GET_ERROR(&stream));
                    ret = IPC_HV_SOA_RET_FAIL;
                }
                else
                {
                    st_register_client_resp *p_resp = (st_register_client_resp *)resp;
                    LOG_PRINT_DEBUG("register client id client_id[%u], client_pid[%u]!", p_resp->client_id, p_resp->client_pid);
                    ret = IPC_HV_SOA_RET_SUCCESS;
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
                    ret = IPC_HV_SOA_RET_FAIL;
                }
                else
                {
                    st_get_client_resp *p_resp = (st_get_client_resp *)resp;
                    if (p_resp->has_responser_client)
                    {
                        LOG_PRINT_DEBUG("get_client resp client_id[%u], client_name[%s]!", p_resp->responser_client.client_id, p_resp->responser_client.client_name);
                    }
                    ret = IPC_HV_SOA_RET_SUCCESS;
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
                    ret = IPC_HV_SOA_RET_FAIL;
                }
                else
                {
                    ret = IPC_HV_SOA_RET_SUCCESS;
                }
            }
            break;
        case LOCAL_REGISTRY_SERVICE_ID_METHOD_SERVICE_CHANGE_STATUS:
            {
                // todo
            }
            break;
        case LOCAL_REGISTRY_SERVICE_ID_METHOD_LISTENER_CHANGE_TO_PROVIDER:
            {
                // todo
            }
            break;
        default:
            break;
    }

    std::shared_ptr<ipc_hv_soa_client_sync_ctx> p_msg_sync_ctx = find_sync_ctx_by_service_id(E_IPC_HV_SOA_CLIENT_SYNC_CTX_TYPE_DAEMON_SEND_SYNC, msg_id);
    if (nullptr != p_msg_sync_ctx)
    {
        {
            std::lock_guard<std::mutex> lock(p_msg_sync_ctx->mutex);
            p_msg_sync_ctx->service_id = msg_id;
            memcpy(p_msg_sync_ctx->data, resp, resp_size);
            p_msg_sync_ctx->ret = ret;
        }
        p_msg_sync_ctx->cond.notify_one();
    }
}

void ipc_hv_soa_manager::onDaemonWriteComplete(const hv::SocketChannelPtr &channel, hv::Buffer *inbuf)
{
    LOG_PRINT_DEBUG("onWriteComplete len[%zu] to channel_id[%u], fd[%d]", inbuf->size(), channel->id(), channel->fd());
}

int32_t ipc_hv_soa_manager::connect_to_daemon()
{
    int32_t ret = IPC_HV_SOA_RET_SUCCESS;
    uint32_t conn_sync_ctx_index = m_sync_ctx_index++;
    std::shared_ptr<ipc_hv_soa_client_sync_ctx> p_connect_sync_ctx = start_sync_ctx(conn_sync_ctx_index);
    if (nullptr == p_connect_sync_ctx)
    {
        return IPC_HV_SOA_RET_FAIL;
    }

    p_connect_sync_ctx->sync_ctx_type = E_IPC_HV_SOA_CLIENT_SYNC_CTX_TYPE_DAEMON_CONNECT;
    p_connect_sync_ctx->dest = MODULE_ID_AUTOLIB_LOCAL_REGISTRY;

    std::unique_lock lock(p_connect_sync_ctx->mutex);
    // connect to daemon
    m_daemon_client.start(true);

    auto timeout = std::chrono::milliseconds(LOCAL_REGISTRY_COMMUNICATION_TIMEOUT_MS);
    bool ready = p_connect_sync_ctx->cond.wait_for(lock, timeout, [&p_connect_sync_ctx]
                                                   {
                                                       return (p_connect_sync_ctx->ret != IPC_HV_SOA_RET_FAIL);
                                                   });
    if (!ready)
    {
        ret = IPC_HV_SOA_RET_TIMEOUT;
    }
    else
    {
        if (p_connect_sync_ctx->ret != INT32_MIN)
        {
            LOG_PRINT_ERROR("connect to daemon fail, invalid ret[%d]!", p_connect_sync_ctx->ret);
            ret = IPC_HV_SOA_RET_FAIL;
        }
        else
        {
            ret = IPC_HV_SOA_RET_SUCCESS;
        }
    }
    end_sync_ctx(conn_sync_ctx_index, p_connect_sync_ctx);

    return ret;
}

int32_t ipc_hv_soa_manager::disconnect_to_daemon()
{
    int32_t ret = IPC_HV_SOA_RET_SUCCESS;
    uint32_t conn_sync_ctx_index = m_sync_ctx_index++;
    std::shared_ptr<ipc_hv_soa_client_sync_ctx> p_connect_sync_ctx = start_sync_ctx(conn_sync_ctx_index);
    if (nullptr == p_connect_sync_ctx)
    {
        return IPC_HV_SOA_RET_FAIL;
    }

    p_connect_sync_ctx->sync_ctx_type = E_IPC_HV_SOA_CLIENT_SYNC_CTX_TYPE_DAEMON_DISCONNECT;
    p_connect_sync_ctx->dest = MODULE_ID_AUTOLIB_LOCAL_REGISTRY;

    std::unique_lock lock(p_connect_sync_ctx->mutex);
    // disconnect to daemon
    m_daemon_client.stop(true);

    auto timeout = std::chrono::milliseconds(LOCAL_REGISTRY_COMMUNICATION_TIMEOUT_MS);
    bool ready = p_connect_sync_ctx->cond.wait_for(lock, timeout, [&p_connect_sync_ctx]
                                                   {
                                                       return (p_connect_sync_ctx->ret != IPC_HV_SOA_RET_FAIL);
                                                   });
    if (!ready)
    {
        ret = IPC_HV_SOA_RET_TIMEOUT;
    }
    else
    {
        if (p_connect_sync_ctx->ret != INT32_MAX)
        {
            LOG_PRINT_ERROR("disconnect to daemon fail, invalid ret[%d]!", p_connect_sync_ctx->ret);
            ret = IPC_HV_SOA_RET_FAIL;
        }
        else
        {
            ret = IPC_HV_SOA_RET_SUCCESS;
        }
    }
    end_sync_ctx(conn_sync_ctx_index, p_connect_sync_ctx);

    return ret;
}

int32_t ipc_hv_soa_manager::send_sync_to_daemon(uint32_t service_id, const void *service_data, const pb_msgdesc_t *req_fileds, uint32_t req_field_size, void *resp_data, uint32_t *resp_data_size)
{
    auto manager = ipc_hv_soa_manager::instance();
    int32_t ret = IPC_HV_SOA_RET_SUCCESS;

    LOG_PRINT_DEBUG("send sync service_id[%d] to daemon fd[%d]", service_id, hio_fd(manager->m_daemon_client.channel->io()));
    std::vector<uint8_t> buffer(LOCAL_REGISTRY_MSG_HEADER_SIZE + req_field_size, 0);
    pb_ostream_t stream = pb_ostream_from_buffer(buffer.data() + LOCAL_REGISTRY_MSG_HEADER_SIZE, req_field_size);
    bool status = pb_encode(&stream, req_fileds, service_data);
    if (!status)
    {
        LOG_PRINT_ERROR("pb_encode service_id[%d] fail, error(%s)", service_id, PB_GET_ERROR(&stream));
        ret = IPC_HV_SOA_RET_FAIL;
    }
    else
    {
        st_local_msg_header send_msg_header = {};
        send_msg_header.msg_id = service_id;
        send_msg_header.msg_len = (uint32_t)stream.bytes_written;
        memcpy(buffer.data(), &send_msg_header, sizeof(send_msg_header));

        uint32_t send_sync_ctx_index = m_sync_ctx_index++;
        std::shared_ptr<ipc_hv_soa_client_sync_ctx> p_send_sync_ctx = start_sync_ctx(send_sync_ctx_index);
        if (nullptr == p_send_sync_ctx)
        {
            return IPC_HV_SOA_RET_FAIL;
        }

        std::unique_lock lock(p_send_sync_ctx->mutex);
        ret = manager->m_daemon_client.send(buffer.data(), (int)(LOCAL_REGISTRY_MSG_HEADER_SIZE + stream.bytes_written));
        if (ret < 0)
        {
            LOG_PRINT_ERROR("send to daemon fail, ret[%d]", ret);
        }
        else
        {
            ret = IPC_HV_SOA_RET_SUCCESS;

            auto timeout = std::chrono::milliseconds(LOCAL_REGISTRY_COMMUNICATION_TIMEOUT_MS);
            bool ready = p_send_sync_ctx->cond.wait_for(lock, timeout, [&p_send_sync_ctx]
                                                        {
                                                            return (p_send_sync_ctx->ret != IPC_HV_SOA_RET_FAIL);
                                                        });
            if (!ready)
            {
                ret = IPC_HV_SOA_RET_TIMEOUT;
            }
            else
            {
                if (p_send_sync_ctx->ret != 0)
                {
                    LOG_PRINT_ERROR("send to daemon wait resp fail, invalid ret[%d]!", p_send_sync_ctx->ret);
                    ret = IPC_HV_SOA_RET_FAIL;
                }
                else
                {
                    if (nullptr != resp_data_size && p_send_sync_ctx->data_len <= *resp_data_size)
                    {
                        *resp_data_size = p_send_sync_ctx->data_len;
                        if (nullptr != resp_data)
                        {
                            memcpy(resp_data, p_send_sync_ctx->data, p_send_sync_ctx->data_len);
                            ret = IPC_HV_SOA_RET_SUCCESS;
                        }
                        else
                        {
                            LOG_PRINT_ERROR("send to daemon wait resp fail, invalid resp_data!");
                            ret = IPC_HV_SOA_RET_FAIL;
                        }
                    }
                    else
                    {
                        LOG_PRINT_ERROR("send to daemon wait resp fail, invalid resp_data_size[%u], data_len[%u]!", *resp_data_size, p_send_sync_ctx->data_len);
                        ret = IPC_HV_SOA_RET_FAIL;
                    }
                }
            }
        }
        end_sync_ctx(send_sync_ctx_index, p_send_sync_ctx);
    }

    return ret;
}

int32_t ipc_hv_soa_manager::send_async_to_daemon(uint32_t service_id, const void *service_data, const pb_msgdesc_t *fileds, uint32_t field_size)
{
    auto manager = ipc_hv_soa_manager::instance();
    int32_t ret = IPC_HV_SOA_RET_SUCCESS;

    LOG_PRINT_DEBUG("send async service_id[%d] to daemon fd[%d]", service_id, hio_fd(manager->m_daemon_client.channel->io()));
    std::vector<uint8_t> buffer(LOCAL_REGISTRY_MSG_HEADER_SIZE + field_size, 0);
    pb_ostream_t stream = pb_ostream_from_buffer(buffer.data() + LOCAL_REGISTRY_MSG_HEADER_SIZE, field_size);
    bool status = pb_encode(&stream, fileds, service_data);
    if (!status)
    {
        LOG_PRINT_ERROR("pb_encode service_id[%d] fail, error(%s)", service_id, PB_GET_ERROR(&stream));
        ret = IPC_HV_SOA_RET_FAIL;
    }
    else
    {
        st_local_msg_header send_msg_header = {};
        send_msg_header.msg_id = service_id;
        send_msg_header.msg_len = (uint32_t)stream.bytes_written;
        memcpy(buffer.data(), &send_msg_header, sizeof(send_msg_header));
        ret = manager->m_daemon_client.send(buffer.data(), (int)(LOCAL_REGISTRY_MSG_HEADER_SIZE + stream.bytes_written));
        if (ret < 0)
        {
            LOG_PRINT_ERROR("send to daemon fail, ret[%d]", ret);
            ret = IPC_HV_SOA_RET_FAIL;
        }
        else
        {
            ret = IPC_HV_SOA_RET_SUCCESS;
        }
    }

    return ret;
}
