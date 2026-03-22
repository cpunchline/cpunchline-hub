#include <semaphore.h>
#include "local_registry_common.hpp"

std::vector<st_local_client_info> g_clients_info = {};
std::vector<st_local_service_info> g_services_info = {};
sem_t get_clients_sem;
sem_t get_services_sem;

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

int main()
{
    hlog_set_handler(_ipc_hv_logger);
    hlog_set_level(LOG_LEVEL_INFO);
    hlog_set_format("HV-%L %s");

    hv::UdsClient ctrl_client;

    sem_init(&get_clients_sem, 0, 0);
    sem_init(&get_services_sem, 0, 0);

    unpack_setting_t unpack_setting = {};
    unpack_setting.mode = UNPACK_BY_LENGTH_FIELD;
    unpack_setting.package_max_length = DEFAULT_PACKAGE_MAX_LENGTH;
    unpack_setting.body_offset = sizeof(st_local_msg_header);
    unpack_setting.length_field_offset = sizeof(uint32_t);
    unpack_setting.length_field_bytes = sizeof(uint32_t);
    unpack_setting.length_adjustment = 0;
    unpack_setting.length_field_coding = ENCODE_BY_LITTEL_ENDIAN;

    int connfd = ctrl_client.createsocket(LOCAL_REGISTRY_SOCKET_PATH);
    if (connfd < 0)
    {
        return -1;
    }
    ctrl_client.bind(LOCAL_REGISTRY_CTRL_SOCKET_PATH);

    ctrl_client.onConnection = [&ctrl_client](const hv::SocketChannelPtr &channel)
    {
        if (channel->isConnected())
        {
            LOG_PRINT_DEBUG("connected to %s! connfd=%d\n", channel->peeraddr().c_str(), channel->fd());

            LOG_PRINT_INFO("get clients req to daemon");
            // get clients
            uint8_t buffer[LOCAL_REGISTRY_MSG_HEADER_SIZE] = {};
            uint32_t msg_id = LOCAL_REGISTRY_SERVICE_ID_METHOD_CTRL_GET_CLIENTS;
            uint32_t msg_len = 0;
            memcpy(buffer, &msg_id, sizeof(msg_id));
            memcpy(buffer + 4, &msg_len, sizeof(msg_len));
            channel->write(buffer, sizeof(buffer));

            // get services
            LOG_PRINT_INFO("get services req to daemon");
            memset(buffer, 0, sizeof(buffer));
            msg_id = LOCAL_REGISTRY_SERVICE_ID_METHOD_CTRL_GET_SERVICES;
            memcpy(buffer, &msg_id, sizeof(msg_id));
            memcpy(buffer + 4, &msg_len, sizeof(msg_len));
            channel->write(buffer, sizeof(buffer));
        }
        else
        {
            LOG_PRINT_DEBUG("disconnected to %s! connfd=%d\n", channel->peeraddr().c_str(), channel->fd());
        }
        if (ctrl_client.isReconnect())
        {
            LOG_PRINT_DEBUG("reconnect cnt=%d, delay=%d\n", ctrl_client.reconn_setting->cur_retry_cnt, ctrl_client.reconn_setting->cur_delay);
        }
    };

    ctrl_client.onMessage = [](const hv::SocketChannelPtr &channel, hv::Buffer *inbuf)
    {
        LOG_PRINT_INFO("onMessage channel_id[%d], fd[%d], readbytes[%zu]", channel->id(), channel->fd(), inbuf->size());
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

        uint8_t *buffer = (uint8_t *)inbuf;
        pb_istream_t stream = pb_istream_from_buffer(&(buffer[LOCAL_REGISTRY_MSG_HEADER_SIZE]), recv_msg_header->msg_len);
        switch (recv_msg_header->service_id)
        {
            case LOCAL_REGISTRY_SERVICE_ID_METHOD_CTRL_GET_CLIENTS:
                {
                    st_ctrl_get_clients get_clients_info = st_ctrl_get_clients_init_zero;
                    status = pb_decode(&stream, st_ctrl_get_clients_fields, &get_clients_info);
                    if (!status)
                    {
                        LOG_PRINT_ERROR("pb_decode service_id[%d] fail, error(%s)", recv_msg_header->service_id, PB_GET_ERROR(&stream));
                        break;
                    }

                    if (0 == get_clients_info.more_flag)
                    {
                        if (g_clients_info.size() == 0)
                        {
                            LOG_PRINT_INFO("no clients!");
                        }
                        else
                        {
                            for (size_t i = 0; i < g_clients_info.size(); i++)
                            {
                                LOG_PRINT_INFO("client_id[%u], client_pid[%u], client_name[%s], client_status[%u] produce_services_count[%u]",
                                               g_clients_info[i].client_id,
                                               g_clients_info[i].client_pid,
                                               g_clients_info[i].client_name,
                                               g_clients_info[i].client_status,
                                               g_clients_info[i].produce_services_count);
                                for (size_t j = 0; j < g_clients_info[i].produce_services_count; j++)
                                {
                                    LOG_PRINT_INFO("service_id[%u], service_type[%u], service_status[%u]",
                                                   g_clients_info[i].produce_services[j].service_id,
                                                   g_clients_info[i].produce_services[j].service_type,
                                                   g_clients_info[i].produce_services[j].service_status);
                                }
                            }
                        }

                        sem_post(&get_clients_sem);
                    }
#if 0
            // each recv will print
            LOG_PRINT_INFO("get clients info, client_count[%u], more_flag[%u]", get_clients_info.clients_count, get_clients_info.more_flag);
            for (size_t i = 0; i < get_clients_info.clients_count; i++)
            {
                LOG_PRINT_INFO("client_id[%u], client_pid[%u], client_name[%s], client_status[%u] produce_services_count[%u]",
                               get_clients_info.clients[i].client_id,
                               get_clients_info.clients[i].client_pid,
                               get_clients_info.clients[i].client_name,
                               get_clients_info.clients[i].client_status,
                               get_clients_info.clients[i].produce_services_count);
                for (size_t j = 0; j < get_clients_info.clients[i].produce_services_count; j++)
                {
                    LOG_PRINT_INFO("service_id[%u], service_type[%u], service_status[%u]",
                                   get_clients_info.clients[i].produce_services[j].service_id,
                                   get_clients_info.clients[i].produce_services[j].service_type,
                                   get_clients_info.clients[i].produce_services[j].service_status);
                }
            }
#endif
                    for (size_t l = 0; l < get_clients_info.clients_count; l++)
                    {
                        g_clients_info.push_back(get_clients_info.clients[l]);
                    }
                }
                break;
            case LOCAL_REGISTRY_SERVICE_ID_METHOD_CTRL_GET_SERVICES:
                {
                    st_ctrl_get_services get_services_info = st_ctrl_get_services_init_zero;
                    status = pb_decode(&stream, st_ctrl_get_services_fields, &get_services_info);
                    if (!status)
                    {
                        LOG_PRINT_ERROR("pb_decode service_id[%d] fail, error(%s)", recv_msg_header->service_id, PB_GET_ERROR(&stream));
                        break;
                    }

                    if (get_services_info.more_flag == 0)
                    {
                        if (g_services_info.size() == 0)
                        {
                            LOG_PRINT_INFO("no services!");
                        }
                        else
                        {
                            for (size_t i = 0; i < g_services_info.size(); i++)
                            {
                                if (g_services_info[i].has_service)
                                {
                                    LOG_PRINT_INFO("service_id[%u], service_type[%u], service_status[%u], listener_clients_count[%u]",
                                                   g_services_info[i].service.service_id,
                                                   g_services_info[i].service.service_type,
                                                   g_services_info[i].service.service_status,
                                                   g_services_info[i].listener_clients_count);
                                    if (g_services_info[i].has_provider_client)
                                    {
                                        LOG_PRINT_INFO("provide_client_id[%u], provide_client_pid[%u], provide_client_name[%s], provide_client_status[%u]",
                                                       g_services_info[i].provider_client.client_id,
                                                       g_services_info[i].provider_client.client_pid,
                                                       g_services_info[i].provider_client.client_name,
                                                       g_services_info[i].provider_client.client_status);
                                    }
                                    else
                                    {
                                        LOG_PRINT_INFO("no provide_client");
                                    }

                                    if (g_services_info[i].listener_clients_count > 0)
                                    {
                                        for (size_t j = 0; j < g_services_info[i].listener_clients_count; j++)
                                        {
                                            LOG_PRINT_INFO("listener_client_id[%u], listener_client_pid[%u], listener_client_name[%s], listener_client_status[%u]",
                                                           g_services_info[i].listener_clients[j].client_id,
                                                           g_services_info[i].listener_clients[j].client_pid,
                                                           g_services_info[i].listener_clients[j].client_name,
                                                           g_services_info[i].listener_clients[j].client_status);
                                        }
                                    }
                                    else
                                    {
                                        LOG_PRINT_INFO("no listener_clients");
                                    }
                                }
                            }
                        }

                        sem_post(&get_services_sem);
                    }

#if 0
                // each recv will print
                LOG_PRINT_INFO("get services info, services_count[%u], more_flag[%u]", get_services_info.services_count, get_services_info.more_flag);
                for (size_t k = 0; k < get_services_info.services_count; k++)
                {
                    if (get_services_info.services[k].has_service)
                    {
                        LOG_PRINT_INFO("service_id[%u], service_type[%u], service_status[%u], listener_clients_count[%u]",
                                       get_services_info.services[k].service.service_id,
                                       get_services_info.services[k].service.service_type,
                                       get_services_info.services[k].service.service_status,
                                       get_services_info.services[k].listener_clients_count);
                        if (get_services_info.services[k].has_provide_client)
                        {
                            LOG_PRINT_INFO("provide_client_id[%u], provide_client_pid[%u], provide_client_name[%s], provide_client_status[%u]",
                                           get_services_info.services[k].provide_client.client_id,
                                           get_services_info.services[k].provide_client.client_pid,
                                           get_services_info.services[k].provide_client.client_name,
                                           get_services_info.services[k].provide_client.client_status);
                        }
                        else
                        {
                            LOG_PRINT_INFO("no provide_client");
                        }
                        if (get_services_info.services[k].listener_clients_count > 0)
                        {
                            for (size_t j = 0; j < get_services_info.services[k].listener_clients_count; j++)
                            {
                                LOG_PRINT_INFO("listener_client_id[%u], listener_client_pid[%u], listener_client_name[%s], listener_client_status[%u]",
                                               get_services_info.services[k].listener_clients[j].client_id,
                                               get_services_info.services[k].listener_clients[j].client_pid,
                                               get_services_info.services[k].listener_clients[j].client_name,
                                               get_services_info.services[k].listener_clients[j].client_status);
                            }
                        }
                    }
                    else
                    {
                        LOG_PRINT_INFO("no service");
                    }
                }
#endif

                    for (size_t l = 0; l < get_services_info.services_count; l++)
                    {
                        g_services_info.push_back(get_services_info.services[l]);
                    }
                }
                break;
            default:
                {
                    LOG_PRINT_ERROR("unknown service_id[%d]", recv_msg_header->service_id);
                    break;
                }
        }
    };

    ctrl_client.onWriteComplete = [](const hv::SocketChannelPtr &channel, hv::Buffer *buf)
    {
        (void)buf;
        if (!channel->isWriteComplete())
        {
            return;
        }
        LOG_PRINT_DEBUG("onWriteComplete channel_id[%d], fd[%d], writebytes[%zu]", channel->id(), channel->fd(), buf->size());
    };

    ctrl_client.setUnpack(&unpack_setting);
    ctrl_client.start(true);

    sem_wait(&get_clients_sem);
    LOG_PRINT_INFO("get clients done.");
    sem_destroy(&get_clients_sem);

    sem_wait(&get_services_sem);
    LOG_PRINT_INFO("get services done.");
    sem_destroy(&get_services_sem);

    ctrl_client.stop(true);

    return EXIT_SUCCESS;
}
