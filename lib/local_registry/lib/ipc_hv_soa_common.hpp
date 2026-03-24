#pragma once

#include "ipc_hv_soa.h"
#include "local_registry_common.hpp"

#include "generator_autolib.h"
#include "hpp/SyncContextPool.hpp"

struct ipc_hv_soa_sync_context
{
    st_local_msg_header header;
    uint8_t data[LOCAL_REGISTRY_MSG_SIZE_MAX];
};

struct ipc_hv_soa_process_client
{
    uint32_t client_id;                             // 客户端 ID(服务注册中心分配)
    std::string client_name;                        // 客户端进程名
    std::string client_localaddr1;                  // process client listen socket
    std::atomic<LOCAL_CLIENT_STATUS> client_status; // 客户端进程运行状态
    hio_t *client_send_io;                          // send io
    hio_t *client_recv_io;                          // recv io

    std::atomic_uint32_t msg_seqid;
    SyncContextPool<ipc_hv_soa_sync_context> process_sync_pool{1, 8};
    std::shared_ptr<SyncContext<ipc_hv_soa_sync_context>> connect_ctx;
    std::mutex pending_requests_mutex;
    std::unordered_map<uint32_t, std::shared_ptr<SyncContext<ipc_hv_soa_sync_context>>> pending_requests;
};

struct ipc_hv_soa_service
{
    uint32_t service_id;                                                                        // 服务 ID
    uint32_t service_type;                                                                      // 服务类型 see local_service_type
    std::atomic<uint32_t> service_status;                                                       // 服务状态 see local_service_status
    void *service_handler;                                                                      // 服务处理函数
    void *service_async_handler;                                                                // 服务异步处理函数 (only used by method)
    std::shared_ptr<ipc_hv_soa_process_client> service_provider;                                // 服务提供者
    std::unordered_map<uint32_t, std::shared_ptr<ipc_hv_soa_process_client>> service_listeners; // 监听该服务的消费者列表 (client id)
};

struct ipc_hv_soa_timer
{
    uint32_t id;
    uint32_t repeat;
    uint32_t interval_ms;
    PF_IPC_HV_SOA_TIMER_CB cb;
    htimer_t *timer;
    void *data;
};

struct ipc_hv_soa_client
{
    // unpack setting
    unpack_setting_t unpack_setting;

    // temp map
    std::mutex process_clients_map_mutex;
    std::unordered_map<uint32_t, std::shared_ptr<ipc_hv_soa_process_client>> process_clients_map;
    std::mutex services_map_mutex;
    std::unordered_map<uint32_t, std::shared_ptr<ipc_hv_soa_service>> services_map;

    // main loop
    hloop_t *m_main_loop;
    std::thread m_main_loop_thread;

    // timer loop
    std::mutex m_timers_map_mutex;
    std::unordered_map<uint32_t, ipc_hv_soa_timer> m_timers_map;
    hloop_t *m_timer_loop;
    std::thread m_timer_loop_thread;

    // mult worker loop
    std::vector<hloop_t *> m_worker_loops;
    std::vector<std::thread> m_worker_threads;
    std::mutex m_worker_mutex;

    // one connecter(daemon)
    hio_t *m_daemon_io; // LOCAL_REGISTEY_SOCKET_FMT

    std::atomic_uint32_t m_msg_seqid;
    SyncContextPool<ipc_hv_soa_sync_context> daemon_sync_pool{1, 8};
    std::shared_ptr<SyncContext<ipc_hv_soa_sync_context>> connect_ctx;
    std::mutex pending_requests_mutex;
    std::unordered_map<uint32_t, std::shared_ptr<SyncContext<ipc_hv_soa_sync_context>>> pending_requests;

    // one accepter
    hio_t *m_listen_io; // LOCAL_REGISTEY_SOCKET_FMT1

    // msg handler
    std::thread msg_handler_thread;
    UnBoundedQueue<ipc_hv_soa_sync_context> msg_handler_queue;

    uint32_t client_id;                             // 客户端 ID(服务注册中心分配)
    pid_t client_pid;                               // 客户端进程 ID
    std::string client_name;                        // 客户端进程名
    std::string client_localaddr;                   // daemon connect socket
    std::string client_localaddr1;                  // listen socket
    std::atomic<LOCAL_CLIENT_STATUS> client_status; // 客户端进程运行状态
};

// map
// process_clients_map operations
std::shared_ptr<ipc_hv_soa_process_client> find_process_client(uint32_t client_id);                                              // get from local cache
std::shared_ptr<ipc_hv_soa_process_client> save_process_client(uint32_t client_id, std::string client_name);                     // save new process client to local cache
std::shared_ptr<ipc_hv_soa_process_client> find_and_save_process_client(uint32_t client_id, std::string client_name, hio_t *io); // if not get from local cache, then save new process client to local cache
std::shared_ptr<ipc_hv_soa_process_client> get_process_client(uint32_t client_id);                                               // get from local cache, if not find, get clientinfo from daemon

// services_map operations
std::shared_ptr<ipc_hv_soa_service> find_service(uint32_t service_id);                                                                                                                                                        // get from local cache
std::shared_ptr<ipc_hv_soa_service> save_service(uint32_t service_id, uint32_t service_type, uint32_t service_status, void *service_handler, void *service_async_handler, std::shared_ptr<ipc_hv_soa_process_client> client); // save new service to local cache
std::shared_ptr<ipc_hv_soa_service> get_service(uint32_t service_id);                                                                                                                                                         // get from local cache, if not find, get service from daemon

// io
hloop_t *get_idle_loop();
void on_close(hio_t *io);
void on_recv_daemon(hio_t *io, void *buf, int readbytes);
void on_recv_process(hio_t *io, void *buf, int readbytes);
void on_write(hio_t *io, const void *buf, int writebytes);
void on_connect(hio_t *io);
void on_post_event_cb(hevent_t *ev);
void on_accept(hio_t *io);

// inn
void process_msg_handler(void);
int32_t send_msg_to_daemon(uint32_t service_id, uint32_t msg_type, const void *msgdata, const pb_msgdesc_t *fileds, uint32_t field_size);
int32_t send_msg_to_daemon_sync(uint32_t service_id, uint32_t msg_type, const void *msgdata, const pb_msgdesc_t *fields, uint32_t field_size, void *resp_data, uint32_t *resp_data_len, uint32_t timeout_ms);
int32_t connect_with_daemon(void);
int32_t connect_with_process_client(std::shared_ptr<ipc_hv_soa_process_client> dest);
int32_t send_msg_to_process(std::shared_ptr<ipc_hv_soa_process_client> dest, uint32_t client_id, uint32_t msg_seqid, uint32_t msg_type, uint32_t service_id, uint32_t msg_len, const void *msgdata);
int32_t send_msg_to_process_sync(std::shared_ptr<ipc_hv_soa_process_client> dest, uint32_t client_id, uint32_t msg_seqid, uint32_t service_id, uint32_t msg_len, const void *msgdata, void *method_resp_data, uint32_t *method_resp_data_len, uint32_t timeout_ms);

int32_t ipc_hv_soa_inn_sync_complete(uint32_t service_id, uint32_t msg_type, uint32_t msg_seqid, void *method_resp_data, uint32_t method_resp_data_len);
int32_t ipc_hv_soa_inn_trigger_to_client(std::shared_ptr<ipc_hv_soa_service> service, void *event_data, uint32_t event_data_len, std::shared_ptr<ipc_hv_soa_process_client> client);
int32_t register_client_req(void);
