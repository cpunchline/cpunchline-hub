#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include <stdlib.h>

#define D_IPC_NNG_MODULE_NAME (64)

typedef enum _ipc_nng_module_id_e
{
    E_IPC_NNG_MODULE_ID_INVALID = 0,

    E_IPC_NNG_MODULE_ID_MANAGER = 1,
    E_IPC_NNG_MODULE_ID_TEST1 = 2,
    E_IPC_NNG_MODULE_ID_TEST2 = 3,

    E_IPC_NNG_MODULE_ID_MAX
} ipc_nng_module_id_e;

typedef enum _ipc_nng_workmode_e
{
    E_IPC_NNG_WORKMODE_SINGLE_WORKER = 1,
    E_IPC_NNG_WORKMODE_MULT_WORKER = 2,
} ipc_nng_workmode_e;

typedef enum _ipc_nng_msg_type_e
{
    E_IPC_NNG_MSG_TYPE_INVALID = 0,
    E_IPC_NNG_MSG_TYPE_NOTIFY,    // notify
    E_IPC_NNG_MSG_TYPE_SYNC,      // sync
    E_IPC_NNG_MSG_TYPE_ASYNC,     // async
    E_IPC_NNG_MSG_TYPE_BROADCAST, // broadcast
    E_IPC_NNG_MSG_TYPE_STREAM,    // stream
    E_IPC_NNG_MSG_TYPE_HEARTBEAT, // heartbeat(TODO)
} ipc_nng_msg_type_e;

typedef enum _ipc_nng_msg_id_e
{
    E_IPC_NNG_MSG_ID_INVALID = 0,
    E_IPC_NNG_MSG_ID_TEST_NOTIFY = 1,
    E_IPC_NNG_MSG_ID_TEST_SYNC = 2,
    E_IPC_NNG_MSG_ID_TEST_ASYNC = 3,
    E_IPC_NNG_MSG_ID_TEST_BROADCAST = 4,
} ipc_nng_msg_id_e;

typedef struct _ipc_nng_broadcast_msg_baseinfo_t
{
    uint8_t sub; // "1" mean broadcast
    uint32_t src_id;
    uint32_t msg_type;
    uint32_t msg_id;
} ipc_nng_broadcast_msg_baseinfo_t;

typedef struct _ipc_nng_msg_baseinfo_t
{
    uint32_t src_id;
    uint32_t dest_id;
    uint32_t msg_type;
    uint32_t msg_id;
    size_t send_len;
    size_t recv_len;
} ipc_nng_msg_baseinfo_t;

// clang-format off
typedef void (*PF_IPC_NNG_NOTIFY_HANDLER)(ipc_nng_msg_baseinfo_t *msg_baseinfo, const uint8_t *notify_data);
typedef void (*PF_IPC_NNG_ASYNC_HANDLER)(ipc_nng_msg_baseinfo_t *msg_baseinfo, uint8_t *response_data);
typedef void (*PF_IPC_NNG_BROADCAST_HANDLER)(ipc_nng_broadcast_msg_baseinfo_t *bmsg_baseinfo, const uint8_t *broadcast_data);
typedef void (*PF_IPC_NNG_RESPONSE_HANDLER)(ipc_nng_msg_baseinfo_t *msg_baseinfo, const uint8_t *indata, uint8_t *outdata);
typedef void (*PF_IPC_NNG_STREAM_HANDLER)(ipc_nng_msg_baseinfo_t *msg_baseinfo, const uint8_t *stream_data);
// clang-format on

typedef struct _ipc_nng_register_info_t
{
    uint32_t module_id; // see ipc_nng_module_id_e
    char module_name[D_IPC_NNG_MODULE_NAME];
    // async/sync/notify, it is a RPC impl
    PF_IPC_NNG_NOTIFY_HANDLER notify_handler;
    PF_IPC_NNG_RESPONSE_HANDLER response_handler;
    PF_IPC_NNG_ASYNC_HANDLER async_handler;
    PF_IPC_NNG_BROADCAST_HANDLER broadcast_handler;
    PF_IPC_NNG_STREAM_HANDLER stream_handler;
} ipc_nng_register_info_t;

/*
req          rep          type                          timeout    class
null/data    null         wait                          fixed      notify(fixed sync)
null/data    null         wait                          unfixed    stream(接收方收到消息先回复响应)
null/data    null/data    wait                          unfixed    sync
null/data    null/data    nowait(another thread wait)   fixed      async
*/

int32_t ipc_nng_init(ipc_nng_register_info_t *reg_info);
int32_t ipc_nng_destroy(uint32_t module_id);
int32_t ipc_nng_send_notify(uint32_t src_id, uint32_t dest_id, uint32_t msg_id, const void *notify_data, size_t notify_data_len);
int32_t ipc_nng_send_sync(uint32_t src_id, uint32_t dest_id, uint32_t msg_id, const void *sync_req_data, size_t sync_req_data_len, void *sync_resp_data, size_t *sync_resp_data_len, size_t sync_resp_data_max_len, size_t timeout);
int32_t ipc_nng_send_async(uint32_t src_id, uint32_t dest_id, uint32_t msg_id, const void *async_req_data, size_t async_req_data_len, size_t async_resp_data_len);
int32_t ipc_nng_send_stream(uint32_t src_id, uint32_t dest_id, uint32_t msg_id, const void *stream_data, size_t stream_data_len, size_t timeout);
int32_t ipc_nng_send_broadcast(uint32_t src_id, uint32_t msg_id, const void *broadcast_data, size_t broadcast_data_len);

#ifdef __cplusplus
}
#endif
