#pragma once

#include "local_registry_common.hpp"

struct local_client;

// 服务项
struct local_service_item
{
    uint32_t service_id;                                                           // 服务ID
    uint32_t service_type;                                                         // 服务类型 see local_service_type
    uint32_t service_status;                                                       // 服务状态 see local_service_status
    std::shared_ptr<local_client> service_provider;                                // 服务提供者
    std::unordered_map<uint32_t, std::shared_ptr<local_client>> service_listeners; // 监听该服务的消费者列表(client id)
};

// 客户端
struct local_client
{
    uint32_t client_id;                                                                        // 客户端进程ID(服务注册中心分配)
    pid_t client_pid;                                                                          // 客户端进程PID
    uint32_t client_channel_id;                                                                // 客户端进程channel id
    std::string client_name;                                                                   // 客户端进程名
    std::string client_localaddr;                                                              // 客户端进程本地unix socket地址
    LOCAL_CLIENT_STATUS client_status;                                                         // 客户端进程运行状态
    std::unordered_map<uint32_t, std::shared_ptr<local_service_item>> client_produce_services; // 提供服务列表
    // std::unordered_map<uint32_t, std::shared_ptr<local_service_item>> client_consume_services; // 监听服务列表
};

// 服务注册中心
class LocalRegistry
{
    SINGLETON_DECL(LocalRegistry);

protected:
    LocalRegistry();
    ~LocalRegistry();

private:
    static void onConnection(const hv::SocketChannelPtr &channel);
    static void onMessage(const hv::SocketChannelPtr &channel, hv::Buffer *inbuf);
    static void onWriteComplete(const hv::SocketChannelPtr &channel, hv::Buffer *inbuf);
    std::int32_t send_msg_to_client(const hv::SocketChannelPtr &client_channel, uint32_t msg_id, const void *msgdata, const pb_msgdesc_t *fileds, uint32_t field_size);

    std::int32_t add_client(const hv::SocketChannelPtr &client_channel);
    std::int32_t remove_client(const hv::SocketChannelPtr &client_channel);
    std::int32_t register_client(st_register_client_req *req, const hv::SocketChannelPtr &client_channel);
    std::int32_t get_client(st_get_client_req *req, const hv::SocketChannelPtr &client_channel);
    std::int32_t register_service(st_register_service *msg);
    std::int32_t listen_service(st_listen_service *msg);
    std::int32_t get_service(st_get_service_req *req);
    std::int32_t service_set_status(st_service_set_status *msg);
    std::int32_t ctr_get_clients(const hv::SocketChannelPtr &client_channel);
    std::int32_t ctr_get_services(const hv::SocketChannelPtr &client_channel);

    // global map
    // one loop so no need lock;
    std::unordered_map<std::string, std::shared_ptr<local_client>> m_clients_by_name; // 客户端列表(process name)
    std::unordered_map<uint32_t, std::shared_ptr<local_service_item>> m_services;     // 服务列表
    std::uint32_t m_next_client_id{1};                                                // auto-inc; no need atomic;
    uint32_t m_ctrl_channel_id{0};
    hv::UdsServer m_server;
};
