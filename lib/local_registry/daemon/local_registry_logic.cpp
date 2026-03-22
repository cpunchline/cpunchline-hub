#include "local_registry.hpp"
#include "generator_autolib.h"

void LocalRegistry::onConnection(const hv::SocketChannelPtr &channel)
{
    auto registry = LocalRegistry::instance();
    if (channel->isConnected())
    {
        LOG_PRINT_DEBUG("%s connected! connfd=%d id=%d tid=%ld\n", channel->peeraddr().c_str(), channel->fd(), channel->id(), currentThreadEventLoop->tid());
        if (strstr(channel->peeraddr().c_str(), LOCAL_REGISTRY_CTRL_SOCKET_FILE))
        {
            LOG_PRINT_INFO("ctrl connected");
        }
        else
        {
            registry->add_client(channel);
        }
    }
    else
    {
        LOG_PRINT_DEBUG("%s disconnected! connfd=%d id=%d tid=%ld\n", channel->peeraddr().c_str(), channel->fd(), channel->id(), currentThreadEventLoop->tid());
        if (strstr(channel->peeraddr().c_str(), LOCAL_REGISTRY_CTRL_SOCKET_FILE))
        {
            LOG_PRINT_INFO("ctrl disconnected");
        }
        else
        {
            registry->remove_client(channel);
        }
    }
}

void LocalRegistry::onMessage(const hv::SocketChannelPtr &channel, hv::Buffer *inbuf)
{
    auto registry = LocalRegistry::instance();

    LOG_PRINT_DEBUG("onMessage channel_id[%u], fd[%d], readbytes[%zu]", channel->id(), channel->fd(), inbuf->size());
    if (inbuf->size() < (int)LOCAL_REGISTRY_MSG_HEADER_SIZE)
    {
        LOG_PRINT_ERROR("not a complete msg, readbytes[%zu] < header_size[%zu]",
                        inbuf->size(), LOCAL_REGISTRY_MSG_HEADER_SIZE);
        return;
    }

    uint32_t real_readbytes = (uint32_t)((uint32_t)inbuf->size() - LOCAL_REGISTRY_MSG_HEADER_SIZE);
    st_local_msg_header *recv_msg_header = (st_local_msg_header *)inbuf->data();
    LOG_PRINT_DEBUG("onMessage service_id[%u], msg_type[%u], msg_seqid[%u], msg_len[%u]",
                    recv_msg_header->service_id,
                    recv_msg_header->msg_type,
                    recv_msg_header->msg_seqid,
                    recv_msg_header->msg_len);
    if (real_readbytes != recv_msg_header->msg_len)
    {
        LOG_PRINT_ERROR("msg_len[%u] != real_readbytes[%u]", recv_msg_header->msg_len, real_readbytes);
        return;
    }

    uint8_t pstruct[LOCAL_REGISTRY_MSG_SIZE_MAX] = {};
    const pb_msgdesc_t *fields = nullptr;
    uint32_t fields_size = 0;
    uint16_t module_id = (uint16_t)(recv_msg_header->service_id >> 16);
    uint16_t msg_id = (uint16_t)(recv_msg_header->service_id & 0xFFFF);
    const st_autolib_servicemap *pmap = gst_autolib_servicemap[module_id];
    if (nullptr == pmap)
    {
        LOG_PRINT_ERROR("module_id[%u] not found", module_id);
        return;
    }
    const st_autolib_servicemap_item *pitem = &pmap->items[msg_id - 1];
    if (nullptr == pitem)
    {
        LOG_PRINT_ERROR("service_id[%u] not found", recv_msg_header->service_id);
        return;
    }

    if (recv_msg_header->msg_type == LOCAL_MSG_TYPE_METHOD_RESPONSE_SYNC || recv_msg_header->msg_type == LOCAL_MSG_TYPE_METHOD_RESPONSE_ASYNC)
    {
        fields = pitem->out_fields;
        fields_size = pitem->out_size;
    }
    else
    {
        fields = pitem->in_fields;
        fields_size = pitem->in_size;
    }

    if (recv_msg_header->msg_len > 0)
    {
        if (fields_size == 0)
        {
            LOG_PRINT_ERROR("msg_len[%u] > 0, but fields_size[%u] = 0", recv_msg_header->msg_len, fields_size);
            return;
        }

        uint8_t *buffer = (uint8_t *)inbuf->data() + LOCAL_REGISTRY_MSG_HEADER_SIZE;
        pb_istream_t stream = pb_istream_from_buffer(buffer, recv_msg_header->msg_len);
        bool status = pb_decode(&stream, fields, pstruct);
        if (!status)
        {
            LOG_PRINT_ERROR("pb_decode service_id[%d] fail, error(%s)", recv_msg_header->service_id, PB_GET_ERROR(&stream));
            return;
        }
        LOG_PRINT_DEBUG("pb_decode service_id[%d] success", recv_msg_header->service_id);
    }
    else
    {
        LOG_PRINT_DEBUG("pb_decode service_id[%d] success(no need)");
    }

    switch (recv_msg_header->service_id)
    {
        case LOCAL_REGISTRY_SERVICE_ID_METHOD_REGISTER_CLIENT:
            {
                st_register_client_req *req = (st_register_client_req *)pstruct;
                registry->register_client(recv_msg_header, req, channel);
            }
            break;
        case LOCAL_REGISTRY_SERVICE_ID_METHOD_GET_CLIENT:
            {
                st_get_client_req *req = (st_get_client_req *)pstruct;
                registry->get_client(recv_msg_header, req, channel);
            }
            break;
        case LOCAL_REGISTRY_SERVICE_ID_METHOD_REGISTER_SERVICE:
            {
                st_register_service *msg = (st_register_service *)pstruct;
                registry->register_service(msg);
            }
            break;
        case LOCAL_REGISTRY_SERVICE_ID_METHOD_LISTEN_SERVICE:
            {
                st_listen_service *msg = (st_listen_service *)pstruct;
                registry->listen_service(msg);
            }
            break;
        case LOCAL_REGISTRY_SERVICE_ID_METHOD_GET_SERVICE:
            {
                st_get_service_req *req = (st_get_service_req *)pstruct;
                if (req->has_listener_client)
                {
                    LOG_PRINT_DEBUG("get service req client_id[%u]-client_name[%s]-service_id[%u]",
                                    req->listener_client.client_id,
                                    req->listener_client.client_name,
                                    req->service_id);
                }
                else
                {
                    LOG_PRINT_DEBUG("get service req service_id[%u]", req->service_id);
                }
                registry->get_service(recv_msg_header, req);
            }
            break;
        case LOCAL_REGISTRY_SERVICE_ID_METHOD_SERVICE_SET_STATUS:
            {
                st_service_set_status *msg = (st_service_set_status *)pstruct;
                registry->service_set_status(msg);
            }
            break;
        case LOCAL_REGISTRY_SERVICE_ID_METHOD_CTRL_GET_CLIENTS:
            {
                LOG_PRINT_DEBUG("ctrl get clients");
                registry->ctr_get_clients(recv_msg_header, channel);
            }
            break;
        case LOCAL_REGISTRY_SERVICE_ID_METHOD_CTRL_GET_SERVICES:
            {
                LOG_PRINT_DEBUG("ctrl get services");
                registry->ctr_get_services(recv_msg_header, channel);
            }
            break;
        default:
            {
                LOG_PRINT_ERROR("invalid service_id[%d]", recv_msg_header->service_id);
            }
            break;
    }
}

void LocalRegistry::onWriteComplete(const hv::SocketChannelPtr &channel, hv::Buffer *inbuf)
{
    LOG_PRINT_DEBUG("onWriteComplete len[%zu] to channel_id[%u], fd[%d]", inbuf->size(), channel->id(), channel->fd());
}

std::int32_t LocalRegistry::send_msg_to_client(const hv::SocketChannelPtr &client_channel, uint32_t service_id, uint32_t msg_seqid, uint32_t msg_type, const void *msgdata, const pb_msgdesc_t *fileds, uint32_t field_size)
{
    std::int32_t ret = 0;

    if (nullptr == client_channel)
    {
        LOG_PRINT_ERROR("invalid client_channel");
        return -1;
    }

    LOG_PRINT_DEBUG("send service_id[%d] to channel_id[%u], fd[%d]", service_id, client_channel->id(), client_channel->fd());
    uint8_t buffer[LOCAL_REGISTRY_MSG_HEADER_SIZE + LOCAL_REGISTRY_MSG_SIZE_MAX] = {};

    pb_ostream_t stream = pb_ostream_from_buffer(buffer + LOCAL_REGISTRY_MSG_HEADER_SIZE, field_size);
    bool status = pb_encode(&stream, fileds, msgdata);
    if (!status)
    {
        LOG_PRINT_ERROR("pb_encode service_id[%d] fail, error(%s)", service_id, PB_GET_ERROR(&stream));
        ret = -1;
    }
    else
    {
        st_local_msg_header send_msg_header = {};
        send_msg_header.client_id = MODULE_ID_AUTOLIB_LOCAL_REGISTRY;
        send_msg_header.msg_seqid = msg_seqid;
        send_msg_header.msg_type = msg_type;
        send_msg_header.service_id = service_id;
        send_msg_header.msg_len = (uint32_t)stream.bytes_written;
        memcpy(buffer, &send_msg_header, LOCAL_REGISTRY_MSG_HEADER_SIZE);
        ret = client_channel->write(buffer, (int)(LOCAL_REGISTRY_MSG_HEADER_SIZE + stream.bytes_written));
        if (ret < 0)
        {
            LOG_PRINT_ERROR("channel_id[%u] write fail, ret[%d]", client_channel->id(), ret);
        }
        else
        {
            ret = 0;
        }
    }

    return ret;
}

std::int32_t LocalRegistry::add_client(const hv::SocketChannelPtr &client_channel)
{
    pid_t client_pid;
    std::string client_name;
    std::string client_localaddr;

    client_localaddr = client_channel->peeraddr();
    size_t slash = client_localaddr.rfind('/');
    size_t dash = client_localaddr.find('-', slash + 1);
    size_t dot = client_localaddr.find('.', dash + 1);

    if (slash == std::string::npos || dash == std::string::npos || dot == std::string::npos)
    {
        LOG_PRINT_ERROR("invalid path format");
        return -1;
    }

    std::string pid_str = client_localaddr.substr(slash + 1, dash - (slash + 1));
    client_pid = std::stoi(pid_str);
    client_name = client_localaddr.substr(dash + 1, dot - (dash + 1));

    std::shared_ptr<local_client> client = nullptr;
    auto it = m_clients_by_name.find(client_name);
    if (m_clients_by_name.end() != it)
    {
        client = it->second;
        if (nullptr != m_server.getChannelById(client->client_channel_id))
        {
            LOG_PRINT_ERROR("client[%s]-pid[%" PRIdMAX "]-client_id[%u] running(so not again run)", client_name.c_str(), (intmax_t)client->client_pid, client->client_id);
            return 1;
        }

        client->client_pid = client_pid;
        client->client_channel_id = client_channel->id();
        client->client_name = client_name;
        client->client_localaddr = client_localaddr;
        client->client_status = LOCAL_CLIENT_STATUS_ONLINE;
        LOG_PRINT_INFO("client[%s]-pid[%" PRIdMAX "]-client_id[%u] duplicate online", client_name.c_str(), (intmax_t)client_pid, client->client_id);
    }
    else
    {
        client = std::make_shared<local_client>();
        client->client_id = m_next_client_id++;
        client->client_pid = client_pid;
        client->client_channel_id = client_channel->id();
        client->client_name = client_name;
        client->client_localaddr = client_localaddr;
        client->client_status = LOCAL_CLIENT_STATUS_ONLINE;
        client->client_produce_services.clear();
        // client->consume_services.clear();
        m_clients_by_name.insert({client_name, client});
        LOG_PRINT_INFO("client[%s]-pid[%" PRIdMAX "]-client_id[%u] online", client_name.c_str(), (intmax_t)client_pid, client->client_id);
    }

    return 0;
}

std::int32_t LocalRegistry::remove_client(const hv::SocketChannelPtr &client_channel)
{
    std::string client_name;
    pid_t pid = 0;
    bool find_ret = false;
    std::shared_ptr<local_client> client = nullptr;
    for (const auto &pair : m_clients_by_name)
    {
        if (client_channel->id() == pair.second->client_channel_id)
        {
            find_ret = true;
            client_name = pair.second->client_name;
            pid = pair.second->client_pid;
            client = pair.second;
            break;
        }
    }

    if (find_ret)
    {
        client->client_status = LOCAL_CLIENT_STATUS_OFFLINE;
        for (const auto &pair1 : client->client_produce_services)
        {
            LOG_PRINT_DEBUG("remove_client service_change_status service_id[%d], service_type[%d], service_status[%d]->[%d]",
                            pair1.second->service_id, pair1.second->service_type, pair1.second->service_status, LOCAL_SERVICE_STATUS_DEFAULT);
            for (const auto &pair2 : pair1.second->service_listeners)
            {
                // 5 service_change_status
                st_service_change_status change_msg = st_service_change_status_init_zero;
                change_msg.has_service = true;
                change_msg.service.service_id = pair1.second->service_id;
                change_msg.service.service_type = pair1.second->service_type;
                change_msg.service.service_status = LOCAL_SERVICE_STATUS_DEFAULT;
                change_msg.has_provider_client = false; // client is offline, so no provider_client
                if (pair2.second->client_status == LOCAL_CLIENT_STATUS_ONLINE)
                {
                    send_msg_to_client(m_server.getChannelById(pair2.second->client_channel_id),
                                       LOCAL_REGISTRY_SERVICE_ID_EVENT_SERVICE_CHANGE_STATUS,
                                       m_msg_seqid++,
                                       LOCAL_MSG_TYPE_EVENT_NOTIFY,
                                       &change_msg,
                                       st_service_change_status_fields,
                                       st_service_change_status_size);
                }
            }
            pair1.second->service_status = LOCAL_SERVICE_STATUS_DEFAULT;
            // m_services.erase(pair1.second->service_id); // remove service
        }
        // client->client_produce_services.clear(); // remove produce service
        LOG_PRINT_INFO("client[%s]-pid[%" PRIdMAX "]-client_id[%u] offline", client_name.c_str(), (intmax_t)pid, client->client_id);
    }
    else
    {
        LOG_PRINT_INFO("client[%s] is not existed", client_name.c_str());
    }

    return 0;
}

std::int32_t LocalRegistry::register_client(st_local_msg_header *recv_msg_header, st_register_client_req *req, const hv::SocketChannelPtr &client_channel)
{
    LOG_PRINT_INFO("register client_pid[%d], client_name[%s]", req->client_pid, req->client_name);

    std::shared_ptr<local_client> client = nullptr;
    auto it = m_clients_by_name.find(std::string(req->client_name));
    if (m_clients_by_name.end() == it)
    {
        LOG_PRINT_ERROR("client[%s] register fail, not online", req->client_name);
    }
    else
    {
        client = it->second;
        if (req->client_pid != client->client_pid)
        {
            LOG_PRINT_ERROR("client[%s] register fail, req pid[%d] != client pid[%d]", req->client_name, req->client_pid, client->client_pid);
        }
    }

    st_register_client_resp resp = st_register_client_resp_init_zero;
    resp.client_pid = (client == nullptr) ? 0 : client->client_pid;
    resp.client_id = (client == nullptr) ? 0 : client->client_id;
    LOG_PRINT_DEBUG("register client id resp client_pid[%u], client_id[%u]", client->client_pid, client->client_id);
    send_msg_to_client(client_channel,
                       LOCAL_REGISTRY_SERVICE_ID_METHOD_REGISTER_CLIENT,
                       recv_msg_header->msg_seqid,
                       recv_msg_header->msg_type + 1,
                       &resp,
                       st_register_client_resp_fields,
                       st_register_client_resp_size);

    return 0;
}

std::int32_t LocalRegistry::get_client(st_local_msg_header *recv_msg_header, st_get_client_req *req, const hv::SocketChannelPtr &client_channel)
{
    LOG_PRINT_DEBUG("get client req client_id[%u]", req->client_id);
    st_get_client_resp resp = st_get_client_resp_init_zero;

    std::shared_ptr<local_client> client = nullptr;
    for (auto &c : m_clients_by_name)
    {
        if (c.second->client_id == req->client_id)
        {
            client = c.second;
            break;
        }
    }

    if (nullptr == client)
    {
        resp.has_responser_client = false;
    }
    else
    {
        resp.has_responser_client = true;
        resp.responser_client.client_id = client->client_id;
        resp.responser_client.client_pid = client->client_pid;
        std::strncpy(resp.responser_client.client_name, client->client_name.c_str(), sizeof(resp.responser_client.client_name));
        resp.responser_client.client_status = client->client_status;
    }
    send_msg_to_client(client_channel,
                       LOCAL_REGISTRY_SERVICE_ID_METHOD_GET_CLIENT,
                       recv_msg_header->msg_seqid,
                       recv_msg_header->msg_type + 1,
                       &resp,
                       st_get_client_resp_fields,
                       st_get_client_resp_size);

    return 0;
}

std::int32_t LocalRegistry::register_service(st_register_service *msg)
{
    if (!msg->has_provider_client)
    {
        LOG_PRINT_ERROR("not have provider client info!");
        return -1;
    }

    LOG_PRINT_DEBUG("register service client_id[%u]-client_name[%s]-produce_services_count[%u]-reg[%s]",
                    msg->provider_client.client_id, msg->provider_client.client_name, msg->produce_services_count, msg->reg ? "true" : "false");
    if (0 == msg->produce_services_count)
    {
        LOG_PRINT_WARN("client[%s] not have any produce item!", msg->provider_client.client_name);
        return 0;
    }
    for (uint32_t i = 0; i < msg->produce_services_count; i++)
    {
        LOG_PRINT_DEBUG("register service service_id[%u], service_type[%u], service_status[%u]",
                        msg->produce_services[i].service_id, msg->produce_services[i].service_type, msg->produce_services[i].service_status);
    }

    std::shared_ptr<local_client> client = nullptr;
    auto it1 = m_clients_by_name.find(msg->provider_client.client_name);
    if (m_clients_by_name.end() == it1)
    {
        LOG_PRINT_ERROR("client[%s] register service fail, not connected!", msg->provider_client.client_name);
        return -1;
    }

    if (msg->reg)
    {
        // register service
        std::shared_ptr<local_service_item> service_item = nullptr;
        for (size_t i = 0; i < msg->produce_services_count; ++i)
        {
            auto it2 = m_services.find(msg->produce_services[i].service_id);
            if (m_services.end() == it2)
            {
                // new service
                service_item = std::make_shared<local_service_item>();
                service_item->service_id = msg->produce_services[i].service_id;
                service_item->service_type = msg->produce_services[i].service_type;
                service_item->service_status = LOCAL_SERVICE_STATUS_AVAILABLE;
                service_item->service_provider = it1->second; // it will fill in at connected
                service_item->service_listeners.clear();
                m_services.insert({service_item->service_id, service_item});
            }
            else
            {
                service_item = it2->second;
                service_item->service_id = msg->produce_services[i].service_id;
                service_item->service_type = msg->produce_services[i].service_type;
                service_item->service_status = LOCAL_SERVICE_STATUS_AVAILABLE;
                service_item->service_provider = it1->second;
            }
            LOG_PRINT_DEBUG("%s service service_id[%u] service_type[%u], service_status[%u], service_provider[%s]",
                            (nullptr == it2) ? "new" : "update",
                            service_item->service_id, service_item->service_type, service_item->service_status,
                            (nullptr == service_item->service_provider) ? "nullptr" : service_item->service_provider->client_name.c_str());

            auto it3 = it1->second->client_produce_services.find(msg->produce_services[i].service_id);
            if (it1->second->client_produce_services.end() == it3)
            {
                it1->second->client_produce_services.insert({service_item->service_id, service_item});
            }
            if (it1->second->client_status == LOCAL_CLIENT_STATUS_ONLINE)
            {
                // service_item is updated in m_services, so here no need to update it
                // 9 listener_change_to_provider
                size_t count = 0;
                st_listener_change_to_provider e2c_msg = st_listener_change_to_provider_init_zero;
                e2c_msg.service_id = service_item->service_id;
                for (auto &listener : service_item->service_listeners)
                {
                    e2c_msg.listener_clients[count].client_id = listener.second->client_id;
                    e2c_msg.listener_clients[count].client_pid = listener.second->client_pid;
                    std::strncpy(e2c_msg.listener_clients[count].client_name, listener.second->client_name.c_str(), sizeof(e2c_msg.listener_clients[count].client_name));
                    LOG_PRINT_INFO("service_id[%d] add listener[%s]", service_item->service_id, listener.second->client_name.c_str());
                    e2c_msg.listener_clients[count].client_status = listener.second->client_status;
                    count++;
                }

                e2c_msg.listener_clients_count = (pb_size_t)count;
                e2c_msg.reg = true;
                send_msg_to_client(m_server.getChannelById(it1->second->client_channel_id),
                                   LOCAL_REGISTRY_SERVICE_ID_EVENT_LISTENER_CHANGE_TO_PROVIDER,
                                   m_msg_seqid++,
                                   LOCAL_MSG_TYPE_EVENT_NOTIFY,
                                   &e2c_msg,
                                   st_listener_change_to_provider_fields,
                                   st_listener_change_to_provider_size);
            }

            for (auto &listener : service_item->service_listeners)
            {
                if (service_item->service_type == LOCAL_SERVICE_TYPE_METHOD)
                {
                    // 5 service_change_status
                    st_service_change_status change_msg = st_service_change_status_init_zero;
                    change_msg.has_service = true;
                    change_msg.service.service_id = service_item->service_id;
                    change_msg.service.service_type = service_item->service_type;
                    change_msg.service.service_status = LOCAL_SERVICE_STATUS_AVAILABLE;
                    change_msg.has_provider_client = true;
                    change_msg.provider_client.client_id = it1->second->client_id;
                    change_msg.provider_client.client_pid = it1->second->client_pid;
                    std::strncpy(change_msg.provider_client.client_name, listener.second->client_name.c_str(), sizeof(change_msg.provider_client.client_name));
                    change_msg.provider_client.client_status = it1->second->client_status;
                    if (listener.second->client_status == LOCAL_CLIENT_STATUS_ONLINE)
                    {
                        send_msg_to_client(m_server.getChannelById(listener.second->client_channel_id),
                                           LOCAL_REGISTRY_SERVICE_ID_EVENT_SERVICE_CHANGE_STATUS,
                                           m_msg_seqid++,
                                           LOCAL_MSG_TYPE_EVENT_NOTIFY,
                                           &change_msg,
                                           st_service_change_status_fields,
                                           st_service_change_status_size);
                    }
                }
                else
                {
                    // event service is no need to notify listener
                }
            }
        }
    }
    else
    {
        // unregister service
        for (size_t i = 0; i < msg->produce_services_count; ++i)
        {
            // delete service from m_services
            auto it2 = m_services.find(msg->produce_services[i].service_id);
            if (m_services.end() == it2)
            {
                LOG_PRINT_WARN("no service item[%d]!", msg->produce_services[i].service_id);
                continue;
            }
            m_services.erase(msg->produce_services[i].service_id);

            // delete service from client_produce_services
            auto it3 = it1->second->client_produce_services.find(msg->produce_services[i].service_id);
            if (it1->second->client_produce_services.end() == it3)
            {
                LOG_PRINT_WARN("client[%s] not have service item[%d]!", msg->provider_client.client_name, msg->produce_services[i].service_id);
                continue;
            }
            it1->second->client_produce_services.erase(msg->produce_services[i].service_id);

            // 9 listener_change_to_provider
            std::shared_ptr<local_service_item> service_item = it3->second;
            LOG_PRINT_DEBUG("delete service service_change_status service_id[%d], service_type[%d], service_status[%d]->[%d]",
                            service_item->service_id, service_item->service_type, service_item->service_status, LOCAL_SERVICE_STATUS_UNAVAILABLE);
            for (auto &listener : service_item->service_listeners)
            {
                if (service_item->service_type == LOCAL_SERVICE_TYPE_METHOD)
                {
                    // 5 service_change_status
                    st_service_change_status change_msg = st_service_change_status_init_zero;
                    change_msg.has_service = true;
                    change_msg.service.service_id = service_item->service_id;
                    change_msg.service.service_type = service_item->service_type;
                    change_msg.service.service_status = LOCAL_SERVICE_STATUS_UNAVAILABLE;
                    change_msg.has_provider_client = false;
                    if (listener.second->client_status == LOCAL_CLIENT_STATUS_ONLINE)
                    {
                        send_msg_to_client(m_server.getChannelById(listener.second->client_channel_id),
                                           LOCAL_REGISTRY_SERVICE_ID_EVENT_SERVICE_CHANGE_STATUS,
                                           m_msg_seqid++,
                                           LOCAL_MSG_TYPE_EVENT_NOTIFY,
                                           &change_msg,
                                           st_service_change_status_fields,
                                           st_service_change_status_size);
                    }
                }
                else
                {
                    // event service is no need to notify listener
                }
            }
#if 0
            // because client info is add/del by connect/disconnect, other logic can't operate client info;
            if (it1->second->client_produce_services.size() == 0)
            {
                LOG_PRINT_WARN("client[%s] no service, so delete client info", it1->second->client_name.c_str());
                m_clients_by_name.erase(it1->second->client_name);
            }
#endif
        }
    }

    return 0;
}

std::int32_t LocalRegistry::listen_service(st_listen_service *msg)
{
    if (!msg->has_listener_client)
    {
        LOG_PRINT_ERROR("not have client info!");
        return -1;
    }

    LOG_PRINT_DEBUG("listen service client_id[%u]-client_name[%s]-listen_services_count[%u]",
                    msg->listener_client.client_id, msg->listener_client.client_name, msg->listen_services_count);
    if (msg->listen_services_count == 0)
    {
        LOG_PRINT_WARN("client[%s] not have any listen item!", msg->listener_client.client_name);
        return 0;
    }
    for (uint32_t i = 0; i < msg->listen_services_count; i++)
    {
        LOG_PRINT_DEBUG("listen service service_id[%u]", msg->listen_services[i].service_id);
    }

    std::shared_ptr<local_client> client = nullptr;
    auto it1 = m_clients_by_name.find(std::string(msg->listener_client.client_name));
    if (m_clients_by_name.end() == it1)
    {
        LOG_PRINT_ERROR("client[%s] listen service fail, not connected!", msg->listener_client.client_name);
        return -1;
    }

    size_t i = 0;
    std::shared_ptr<local_service_item> service_item = nullptr;
    for (i = 0; i < msg->listen_services_count; ++i)
    {
        auto it2 = m_services.find(msg->listen_services[i].service_id);
        if (m_services.end() == it2)
        {
            // new service(but status is default)
            service_item = std::make_shared<local_service_item>();
            service_item->service_id = msg->listen_services[i].service_id;
            service_item->service_type = msg->listen_services[i].service_type;
            service_item->service_status = LOCAL_SERVICE_STATUS_DEFAULT;
            service_item->service_provider = nullptr;
            service_item->service_listeners.clear();
            m_services.insert({service_item->service_id, service_item});
        }
        else
        {
            service_item = it2->second;
        }

        if (msg->reg)
        {
            // listen

            // add listener to service
            auto it3 = service_item->service_listeners.find(it1->second->client_id);
            if (service_item->service_listeners.end() == it3)
            {
                LOG_PRINT_INFO("client[%s] listen service[%d]", it1->second->client_name.c_str(), service_item->service_id);
                service_item->service_listeners.insert({it1->second->client_id, it1->second});
            }

            if (service_item->service_type == LOCAL_SERVICE_TYPE_METHOD && service_item->service_status == LOCAL_SERVICE_STATUS_AVAILABLE)
            {
                // 5 service_change_status
                st_service_change_status change_msg = st_service_change_status_init_zero;
                change_msg.has_service = true;
                change_msg.service.service_id = service_item->service_id;
                change_msg.service.service_type = service_item->service_type;
                change_msg.service.service_status = service_item->service_status;
                if (nullptr != service_item->service_provider)
                {
                    change_msg.has_provider_client = true;
                    change_msg.provider_client.client_id = service_item->service_provider->client_id;
                    change_msg.provider_client.client_pid = service_item->service_provider->client_pid;
                    std::strncpy(change_msg.provider_client.client_name, service_item->service_provider->client_name.c_str(), sizeof(change_msg.provider_client.client_name));
                    change_msg.provider_client.client_status = service_item->service_provider->client_status;
                }
                else
                {
                    change_msg.has_provider_client = false;
                }
                if (it1->second->client_status == LOCAL_CLIENT_STATUS_ONLINE)
                {
                    send_msg_to_client(m_server.getChannelById(it1->second->client_channel_id),
                                       LOCAL_REGISTRY_SERVICE_ID_EVENT_SERVICE_CHANGE_STATUS,
                                       m_msg_seqid++,
                                       LOCAL_MSG_TYPE_EVENT_NOTIFY,
                                       &change_msg,
                                       st_service_change_status_fields,
                                       st_service_change_status_size);
                }
            }
            else
            {
                // event service is no need to notify listener
                // service status not available, it is useless for new listener
            }
        }
        else
        {
            // cancel listen
            if (service_item->service_type == LOCAL_SERVICE_TYPE_METHOD)
            {
                service_item->service_listeners.erase(it1->second->client_id);
            }
        }

        if (nullptr != service_item->service_provider)
        {
            // 9 listener_change_to_provider
            st_listener_change_to_provider e2c_msg = st_listener_change_to_provider_init_zero;
            e2c_msg.service_id = service_item->service_id;
            e2c_msg.listener_clients_count = 1;
            e2c_msg.listener_clients[0].client_id = it1->second->client_id;
            e2c_msg.listener_clients[0].client_pid = it1->second->client_pid;
            std::strncpy(e2c_msg.listener_clients[0].client_name, it1->second->client_name.c_str(), sizeof(e2c_msg.listener_clients[0].client_name));
            e2c_msg.listener_clients[0].client_status = it1->second->client_status;
            e2c_msg.reg = msg->reg;
            LOG_PRINT_INFO("service_id[%d] add listener[%s]", service_item->service_id, it1->second->client_name.c_str());
            if (service_item->service_provider->client_status == LOCAL_CLIENT_STATUS_ONLINE)
            {
                send_msg_to_client(m_server.getChannelById(service_item->service_provider->client_channel_id),
                                   LOCAL_REGISTRY_SERVICE_ID_EVENT_LISTENER_CHANGE_TO_PROVIDER,
                                   m_msg_seqid++,
                                   LOCAL_MSG_TYPE_EVENT_NOTIFY,
                                   &e2c_msg,
                                   st_listener_change_to_provider_fields,
                                   st_listener_change_to_provider_size);
            }
        }
    }

    return 0;
}

std::int32_t LocalRegistry::get_service(st_local_msg_header *recv_msg_header, st_get_service_req *req)
{
    st_get_service_resp resp = st_get_service_resp_init_zero;

    if (!req->has_listener_client)
    {
        LOG_PRINT_ERROR("not have client info!");
        return -1;
    }

    std::shared_ptr<local_client> client = nullptr;
    auto it1 = m_clients_by_name.find(std::string(req->listener_client.client_name));
    if (m_clients_by_name.end() == it1)
    {
        LOG_PRINT_ERROR("client[%s] get service fail, not connected!", req->listener_client.client_name);
        return -1;
    }

    auto it2 = m_services.find(req->service_id);
    resp.has_service = true;
    if (m_services.end() == it2)
    {
        // it also means service is exists, only status is not available
        resp.service.service_id = req->service_id;
        resp.service.service_type = LOCAL_SERVICE_TYPE_UNKNOWN;
        resp.service.service_status = LOCAL_SERVICE_STATUS_UNAVAILABLE;
        resp.has_provider_client = false;
    }
    else
    {
        resp.service.service_id = it2->second->service_id;
        resp.service.service_type = it2->second->service_type;
        resp.service.service_status = it2->second->service_status;
        if (nullptr != it2->second->service_provider)
        {
            resp.has_provider_client = true;
            resp.provider_client.client_id = it2->second->service_provider->client_id;
            resp.provider_client.client_pid = it2->second->service_provider->client_pid;
            std::strncpy(resp.provider_client.client_name, it2->second->service_provider->client_name.c_str(), sizeof(resp.provider_client.client_name));
            resp.provider_client.client_status = it2->second->service_provider->client_status;
        }
        else
        {
            resp.has_provider_client = false;
        }
    }

    LOG_PRINT_DEBUG("get service resp service_id[%u] service_type[%u], service_status[%u]",
                    resp.service.service_id, resp.service.service_type, resp.service.service_status);
    if (it1->second->client_status == LOCAL_CLIENT_STATUS_ONLINE)
    {
        send_msg_to_client(m_server.getChannelById(it1->second->client_channel_id),
                           LOCAL_REGISTRY_SERVICE_ID_METHOD_GET_SERVICE,
                           recv_msg_header->msg_seqid,
                           recv_msg_header->msg_type + 1,
                           &resp,
                           st_get_service_resp_fields,
                           st_get_service_resp_size);
    }

    return 0;
}

std::int32_t LocalRegistry::service_set_status(st_service_set_status *msg)
{
    if (!msg->has_provider_client)
    {
        LOG_PRINT_ERROR("not have client info!");
        return -1;
    }

    LOG_PRINT_DEBUG("service set status client_id[%u]-client_name[%s]-services_count[%u]",
                    msg->provider_client.client_id, msg->provider_client.client_name, msg->services_count);
    if (msg->services_count == 0)
    {
        LOG_PRINT_ERROR("no service");
        return -1;
    }
    for (uint32_t i = 0; i < msg->services_count; i++)
    {
        LOG_PRINT_DEBUG("service set status service_id[%u], service_type[%u], service_status[%u]",
                        msg->services[i].service_id, msg->services[i].service_type, msg->services[i].service_status);
    }

    std::shared_ptr<local_client> client = nullptr;
    auto it1 = m_clients_by_name.find(std::string(msg->provider_client.client_name));
    if (m_clients_by_name.end() == it1)
    {
        LOG_PRINT_ERROR("client[%s] set status service fail", msg->provider_client.client_name);
        return -1;
    }

    size_t i = 0;
    for (i = 0; i < msg->services_count; ++i)
    {
        auto it2 = m_services.find(msg->services[i].service_id);
        if (m_services.end() != it2)
        {
            bool status_changed = false;
            if (it2->second->service_status != msg->services[i].service_status)
            {
                LOG_PRINT_DEBUG("service_set_status service_change_status service_id[%d], service_type[%d], service_status[%d]->[%d]",
                                it2->second->service_id, it2->second->service_type, it2->second->service_status, msg->services[i].service_status);
                status_changed = true;
                it2->second->service_status = msg->services[i].service_status;
            }

            if (status_changed)
            {
                if (it2->second->service_type == LOCAL_SERVICE_TYPE_METHOD)
                {
                    // 5 service_change_status
                    st_service_change_status change_msg = st_service_change_status_init_zero;
                    change_msg.has_service = true;
                    change_msg.service.service_id = it2->second->service_id;
                    change_msg.service.service_type = it2->second->service_type;
                    change_msg.service.service_status = it2->second->service_status;

                    if (nullptr != it2->second->service_provider)
                    {
                        change_msg.has_provider_client = true;
                        change_msg.provider_client.client_id = it2->second->service_provider->client_id;
                        change_msg.provider_client.client_pid = it2->second->service_provider->client_pid;
                        std::strncpy(change_msg.provider_client.client_name, it2->second->service_provider->client_name.c_str(), sizeof(change_msg.provider_client.client_name));
                        change_msg.provider_client.client_status = it2->second->service_provider->client_status;
                    }
                    else
                    {
                        change_msg.has_provider_client = false;
                    }

                    for (auto &listener : it2->second->service_listeners)
                    {
                        if (listener.second->client_status == LOCAL_CLIENT_STATUS_ONLINE)
                        {
                            send_msg_to_client(m_server.getChannelById(listener.second->client_channel_id),
                                               LOCAL_REGISTRY_SERVICE_ID_EVENT_SERVICE_CHANGE_STATUS,
                                               m_msg_seqid++,
                                               LOCAL_MSG_TYPE_EVENT_NOTIFY,
                                               &change_msg,
                                               st_service_change_status_fields,
                                               st_service_change_status_size);
                        }
                    }
                }
            }
        }
    }

    return 0;
}

std::int32_t LocalRegistry::ctr_get_clients(st_local_msg_header *recv_msg_header, const hv::SocketChannelPtr &client_channel)
{
    st_ctrl_get_clients resp = st_ctrl_get_clients_init_zero;
    constexpr uint32_t each_clients_count = ARRAY_SIZE(resp.clients);
    size_t i = 0, j = 0;
    uint32_t more_flag = 0;

    for (auto &c : m_clients_by_name)
    {
        resp.clients[i].client_id = c.second->client_id;
        resp.clients[i].client_pid = c.second->client_pid;
        std::strncpy(resp.clients[i].client_name, c.second->client_name.c_str(), sizeof(resp.clients[i].client_name));
        resp.clients[i].client_status = c.second->client_status;

        j = 0;
        for (const auto &service : c.second->client_produce_services)
        {
            resp.clients[i].produce_services[j].service_id = service.first;
            resp.clients[i].produce_services[j].service_type = service.second->service_type;
            resp.clients[i].produce_services[j].service_status = service.second->service_status;
            j++;
            if (j >= LOCAL_REGISTRY_CLIENT_PROVIDE_SERVICES_ONCE_COUNT_MAX)
            {
                break;
            }
        }
        resp.clients[i].produce_services_count = (pb_size_t)j;
        i++;
        if (j >= LOCAL_REGISTRY_CLIENT_PROVIDE_SERVICES_ONCE_COUNT_MAX)
        {
            resp.clients_count = (pb_size_t)i;
            resp.more_flag = ++more_flag;
            send_msg_to_client(client_channel,
                               LOCAL_REGISTRY_SERVICE_ID_METHOD_CTRL_GET_CLIENTS,
                               recv_msg_header->msg_seqid,
                               recv_msg_header->msg_type + 1,
                               &resp,
                               st_ctrl_get_clients_fields,
                               st_ctrl_get_clients_size);
            resp = st_ctrl_get_clients_init_zero;
            i = 0;
            continue;
        }

        if (i >= each_clients_count)
        {
            resp.clients_count = each_clients_count;
            resp.more_flag = ++more_flag;
            send_msg_to_client(client_channel,
                               LOCAL_REGISTRY_SERVICE_ID_METHOD_CTRL_GET_CLIENTS,
                               recv_msg_header->msg_seqid,
                               recv_msg_header->msg_type + 1,
                               &resp,
                               st_ctrl_get_clients_fields,
                               st_ctrl_get_clients_size);
            resp = st_ctrl_get_clients_init_zero;
            i = 0;
            continue;
        }
    }

    if (i != 0)
    {
        resp.clients_count = (pb_size_t)i;
        resp.more_flag = ++more_flag;
        send_msg_to_client(client_channel,
                           LOCAL_REGISTRY_SERVICE_ID_METHOD_CTRL_GET_CLIENTS,
                           recv_msg_header->msg_seqid,
                           recv_msg_header->msg_type + 1,
                           &resp,
                           st_ctrl_get_clients_fields,
                           st_ctrl_get_clients_size);
    }

    // finish
    resp = st_ctrl_get_clients_init_zero;
    resp.more_flag = 0;
    send_msg_to_client(client_channel,
                       LOCAL_REGISTRY_SERVICE_ID_METHOD_CTRL_GET_CLIENTS,
                       recv_msg_header->msg_seqid,
                       recv_msg_header->msg_type + 1,
                       &resp,
                       st_ctrl_get_clients_fields,
                       st_ctrl_get_clients_size);

    return 0;
}

std::int32_t LocalRegistry::ctr_get_services(st_local_msg_header *recv_msg_header, const hv::SocketChannelPtr &client_channel)
{
    st_ctrl_get_services resp = st_ctrl_get_services_init_zero;
    constexpr uint32_t each_services_count = ARRAY_SIZE(resp.services);
    size_t i = 0, j = 0;
    uint32_t more_flag = 0;

    for (auto &s : m_services)
    {
        resp.services[i].has_service = true;
        resp.services[i].service.service_id = s.second->service_id;
        resp.services[i].service.service_type = s.second->service_type;
        resp.services[i].service.service_status = s.second->service_status;
        if (nullptr != s.second->service_provider)
        {
            resp.services[i].has_provider_client = true;
            resp.services[i].provider_client.client_id = s.second->service_provider->client_id;
            resp.services[i].provider_client.client_pid = s.second->service_provider->client_pid;
            std::strncpy(resp.services[i].provider_client.client_name, s.second->service_provider->client_name.c_str(), sizeof(resp.services[i].provider_client.client_name));
            resp.services[i].provider_client.client_status = s.second->service_provider->client_status;
        }
        else
        {
            resp.services[i].has_provider_client = false;
        }

        j = 0;
        for (const auto &listener : s.second->service_listeners)
        {
            resp.services[i].listener_clients[j].client_id = listener.second->client_id;
            resp.services[i].listener_clients[j].client_pid = listener.second->client_pid;
            std::strncpy(resp.services[i].listener_clients[j].client_name, listener.second->client_name.c_str(), sizeof(resp.services[i].listener_clients[j].client_name));
            resp.services[i].listener_clients[j].client_status = listener.second->client_status;
            j++;
            if (j >= LOCAL_REGISTRY_CLIENT_ONCE_COUNT_MAX)
            {
                break;
            }
        }
        resp.services[i].listener_clients_count = (pb_size_t)j;
        i++;
        if (j >= LOCAL_REGISTRY_CLIENT_ONCE_COUNT_MAX)
        {
            resp.services_count = (pb_size_t)i;
            resp.more_flag = ++more_flag;
            send_msg_to_client(client_channel,
                               LOCAL_REGISTRY_SERVICE_ID_METHOD_CTRL_GET_SERVICES,
                               recv_msg_header->msg_seqid,
                               recv_msg_header->msg_type + 1,
                               &resp,
                               st_ctrl_get_services_fields,
                               st_ctrl_get_services_size);
            resp = st_ctrl_get_services_init_zero;
            i = 0;
            continue;
        }

        if (i >= each_services_count)
        {
            resp.services_count = each_services_count;
            resp.more_flag = ++more_flag;
            send_msg_to_client(client_channel,
                               LOCAL_REGISTRY_SERVICE_ID_METHOD_CTRL_GET_SERVICES,
                               recv_msg_header->msg_seqid,
                               recv_msg_header->msg_type + 1,
                               &resp,
                               st_ctrl_get_services_fields,
                               st_ctrl_get_services_size);
            resp = st_ctrl_get_services_init_zero;
            i = 0;
            continue;
        }
    }

    if (i != 0)
    {
        resp.services_count = (pb_size_t)i;
        resp.more_flag = ++more_flag;
        send_msg_to_client(client_channel,
                           LOCAL_REGISTRY_SERVICE_ID_METHOD_CTRL_GET_SERVICES,
                           recv_msg_header->msg_seqid,
                           recv_msg_header->msg_type + 1,
                           &resp,
                           st_ctrl_get_services_fields,
                           st_ctrl_get_services_size);
    }

    // finish
    resp = st_ctrl_get_services_init_zero;
    resp.more_flag = 0;
    send_msg_to_client(client_channel,
                       LOCAL_REGISTRY_SERVICE_ID_METHOD_CTRL_GET_SERVICES,
                       recv_msg_header->msg_seqid,
                       recv_msg_header->msg_type + 1,
                       &resp,
                       st_ctrl_get_services_fields,
                       st_ctrl_get_services_size);
    return 0;
}
