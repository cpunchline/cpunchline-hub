#pragma once

#include "ipc_hv_soa.h"
#include "local_registry_common.hpp"
#include "generator_autolib.h"

struct ipc_hv_soa_timer
{
    uint32_t id;
    uint32_t repeat;
    uint32_t interval_ms;
    PF_IPC_HV_SOA_TIMER_CB cb;
    void *data;
};

enum ipc_hv_soa_client_sync_ctx_type
{
    E_IPC_HV_SOA_CLIENT_SYNC_CTX_TYPE_INVALID = 0,
    E_IPC_HV_SOA_CLIENT_SYNC_CTX_TYPE_DAEMON_CONNECT = 1,     // sync_ctx_type + dest
    E_IPC_HV_SOA_CLIENT_SYNC_CTX_TYPE_DAEMON_DISCONNECT = 2,  // sync_ctx_type + dest
    E_IPC_HV_SOA_CLIENT_SYNC_CTX_TYPE_DAEMON_SEND_SYNC = 3,   // sync_ctx_type + service_id
    E_IPC_HV_SOA_CLIENT_SYNC_CTX_TYPE_PROCESS_CONNECT = 4,    // sync_ctx_type + dest
    E_IPC_HV_SOA_CLIENT_SYNC_CTX_TYPE_PROCESS_DISCONNECT = 5, // sync_ctx_type + dest
    E_IPC_HV_SOA_CLIENT_SYNC_CTX_TYPE_PROCESS_SEND_SYNC = 6,  // sync_ctx_type + service_id
};

struct ipc_hv_soa_client_sync_ctx
{
    uint32_t sync_ctx_index;
    uint32_t sync_ctx_type; // see ipc_hv_soa_client_sync_ctx_type
    uint32_t service_id;
    uint32_t service_type;
    uint32_t service_seqid;
    uint32_t dest;
    int32_t ret;
    uint32_t data_len;
    uint8_t data[LOCAL_REGISTRY_MSG_SIZE_MAX];
    std::mutex mutex;
    std::condition_variable cond;
};

struct ipc_hv_soa_client
{
    uint32_t client_id;                             // 客户端ID(服务注册中心分配)
    std::string client_name;                        // 客户端进程名
    std::string client_localaddr1;                  // process client listen socket
    std::atomic<LOCAL_CLIENT_STATUS> client_status; // 客户端进程运行状态
    uint32_t client_send_channel_id;
    uint32_t client_recv_channel_id;
    std::atomic_uint32_t send_msg_seqid;
    std::unordered_map<uint64_t, ipc_hv_soa_client_sync_ctx> send_msg_map; // key: uint64_t = service_id(uint32_t) + send_msg_seqid(uint32_t);
};

struct ipc_hv_soa_service
{
    uint32_t service_id;                                                                // 服务ID
    uint32_t service_type;                                                              // 服务类型 see local_service_type
    std::atomic<uint32_t> service_status;                                               // 服务状态 see local_service_status
    void *service_handler;                                                              // 服务处理函数
    void *service_async_handler;                                                        // 服务异步处理函数(only used by method)
    std::shared_ptr<ipc_hv_soa_client> service_provider;                                // 服务提供者
    std::unordered_map<uint32_t, std::shared_ptr<ipc_hv_soa_client>> service_listeners; // 监听该服务的消费者列表(client id)
};

struct ipc_hv_soa_manager
{
    SINGLETON_DECL(ipc_hv_soa_manager);

protected:
    ipc_hv_soa_manager();
    ~ipc_hv_soa_manager();

private:
    // common
    static std::shared_ptr<ipc_hv_soa_client_sync_ctx> start_sync_ctx(uint32_t index);
    static void end_sync_ctx(uint32_t index, std::shared_ptr<ipc_hv_soa_client_sync_ctx> p_sync_ctx);
    static std::shared_ptr<ipc_hv_soa_client_sync_ctx> find_sync_ctx_by_dest(uint32_t sync_ctx_type, uint32_t dest);
    static std::shared_ptr<ipc_hv_soa_client_sync_ctx> find_sync_ctx_by_service_id(uint32_t sync_ctx_type, uint32_t service_id);

    // daemon
    static void onDaemonConnection(const hv::SocketChannelPtr &channel);
    static void onDaemonMessage(const hv::SocketChannelPtr &channel, hv::Buffer *inbuf);
    static void onDaemonWriteComplete(const hv::SocketChannelPtr &channel, hv::Buffer *inbuf);
    int32_t connect_to_daemon();
    int32_t disconnect_to_daemon();
    int32_t send_sync_to_daemon(uint32_t service_id, const void *service_data, const pb_msgdesc_t *req_fileds, uint32_t req_field_size, void *resp_data, uint32_t *resp_data_size);
    int32_t send_async_to_daemon(uint32_t service_id, const void *service_data, const pb_msgdesc_t *fileds, uint32_t field_size);

    // local cache

    std::mutex m_clients_map_mutex;
    std::unordered_map<uint32_t, std::shared_ptr<ipc_hv_soa_client>> m_clients_map;
    std::mutex m_services_map_mutex;
    std::unordered_map<uint32_t, std::shared_ptr<ipc_hv_soa_service>> m_services_map;
    std::mutex m_timers_map_mutex;
    std::unordered_map<uint32_t, ipc_hv_soa_timer> m_timers_map;
    std::mutex m_sync_ctx_map_mutex;
    std::unordered_map<uint32_t, std::shared_ptr<ipc_hv_soa_client_sync_ctx>> m_sync_ctx_map;

    hv::EventLoopThreadPool m_worker_loop_pool;              // worker loop pool
    HObjectPool<ipc_hv_soa_client_sync_ctx> m_sync_ctx_pool; // sync ctx pool
    std::atomic_uint32_t m_sync_ctx_index{1};

    ipc_hv_soa_client m_client;

    hv::UdsClient m_daemon_client; // one connector on path LOCAL_REGISTRY_SOCKET_PATH, bin on LOCAL_REGISTEY_SOCKET_FMT
    hv::UdsServer m_listen_server; // one accepter on path LOCAL_REGISTEY_SOCKET_FMT1
};
