#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include <stdlib.h>

#define D_IPC_NNG_LONG_MODULE_NAME (64)

typedef enum _ipc_nng_long_module_id_e
{
    E_IPC_NNG_LONG_MODULE_ID_DAEMON = 0,
    E_IPC_NNG_LONG_MODULE_ID_TEST1 = 1,
    E_IPC_NNG_LONG_MODULE_ID_TEST2 = 2,

    E_IPC_NNG_LONG_MODULE_ID_MAX
} ipc_nng_long_module_id_e;

typedef enum _ipc_nng_long_msg_type_e
{
    E_IPC_NNG_LONG_MSG_TYPE_INVALID = 0,
    E_IPC_NNG_LONG_MSG_TYPE_CONN,     // conn only used for daemon
    E_IPC_NNG_LONG_MSG_TYPE_REGISTER, // register
    E_IPC_NNG_LONG_MSG_TYPE_BROADCAST,
    E_IPC_NNG_LONG_MSG_TYPE_REQ,
    E_IPC_NNG_LONG_MSG_TYPE_REP,
    E_IPC_NNG_LONG_MSG_TYPE_MULT_REQ,
    E_IPC_NNG_LONG_MSG_TYPE_MULT_REP, // only daemon rep
    E_IPC_NNG_LONG_MSG_TYPE_STREAM_REQ,
    E_IPC_NNG_LONG_MSG_TYPE_STREAM_REP,
    E_IPC_NNG_LONG_MSG_TYPE_ASYNC_REQ,
    E_IPC_NNG_LONG_MSG_TYPE_ASYNC_REP,
    E_IPC_NNG_LONG_MSG_TYPE_ERR,
} ipc_nng_long_msg_type_e;

typedef enum _ipc_nng_long_msg_id_e
{
    E_IPC_NNG_LONG_MSG_ID_INVALID = 0,
    E_IPC_NNG_LONG_MSG_ID_CONN,
    E_IPC_NNG_LONG_MSG_ID_REGISTER,
    E_IPC_NNG_LONG_MSG_ID_TEST_NOTIFY,
    E_IPC_NNG_LONG_MSG_ID_TEST_SYNC,
    E_IPC_NNG_LONG_MSG_ID_TEST_ASYNC,
    E_IPC_NNG_LONG_MSG_ID_TEST_BROADCAST,
} ipc_nng_long_msg_id_e;

typedef struct _ipc_nng_long_msg_baseinfo_t
{
    uint32_t tag;
    uint32_t src_id;
    uint32_t dest_id;
    uint32_t msg_id;
    uint64_t event_id;
    uint32_t msg_type;
    uint64_t expire;
    int32_t result;
    size_t send_len;
    size_t recv_len; // it can replace by some protocol, such as nanopb
} ipc_nng_long_msg_baseinfo_t;

typedef struct _ipc_nng_long_mcast_dest_t
{
    uint32_t num;
    uint32_t module_array[E_IPC_NNG_LONG_MODULE_ID_MAX];
} ipc_nng_long_mcast_dest_t;

// clang-format off
typedef void (*PF_IPC_NNG_LONG_ASYNC_HANDLER)(ipc_nng_long_msg_baseinfo_t *msg_baseinfo, uint8_t *response_data);
typedef void (*PF_IPC_NNG_LONG_BROADCAST_HANDLER)(ipc_nng_long_msg_baseinfo_t *msg_baseinfo, const uint8_t *broadcast_data);
typedef void (*PF_IPC_NNG_LONG_RESPONSE_HANDLER)(ipc_nng_long_msg_baseinfo_t *msg_baseinfo, const uint8_t *indata, uint8_t *outdata);
typedef void (*PF_IPC_NNG_LONG_STREAM_HANDLER)(ipc_nng_long_msg_baseinfo_t *msg_baseinfo, const uint8_t *stream_data);
// clang-format on

typedef struct _ipc_nng_long_register_info_t
{
    uint32_t module_id; // see ipc_nng_long_module_id_e
    char module_name[D_IPC_NNG_LONG_MODULE_NAME];
    // async/sync/notify, it is a RPC impl
    PF_IPC_NNG_LONG_RESPONSE_HANDLER response_handler;
    PF_IPC_NNG_LONG_ASYNC_HANDLER async_handler;
    PF_IPC_NNG_LONG_BROADCAST_HANDLER broadcast_handler;
    PF_IPC_NNG_LONG_STREAM_HANDLER stream_handler;
} ipc_nng_long_register_info_t;

/*
req          rep          type                          timeout    class
null/data    null         wait                          fixed      notify(fixed sync)
null/data    null         wait                          unfixed    stream(接收方收到消息先回复响应)
null/data    null/data    wait                          unfixed    sync
null/data    null/data    nowait(another thread wait)   unfixed    async
*/

int32_t ipc_nng_long_init(ipc_nng_long_register_info_t *reg_info);
int32_t ipc_nng_long_destroy(uint32_t module_id);
int32_t ipc_nng_long_send_notify(uint32_t src_id, uint32_t dest_id, uint32_t msg_id, const void *notify_data, size_t notify_data_len);
int32_t ipc_nng_long_send_sync(uint32_t src_id, uint32_t dest_id, uint32_t msg_id, const void *sync_req_data, size_t sync_req_data_len, void *sync_resp_data, size_t *sync_resp_data_len, size_t sync_resp_data_max_len, size_t timeout);
int32_t ipc_nng_long_send_async(uint32_t src_id, uint32_t dest_id, uint32_t msg_id, const void *async_req_data, size_t async_req_data_len, size_t async_resp_data_len, size_t timeout);
int32_t ipc_nng_long_send_stream(uint32_t src_id, uint32_t dest_id, uint32_t msg_id, const void *stream_data, size_t stream_data_len, size_t timeout);
int32_t ipc_nng_long_send_broadcast(uint32_t src_id, uint32_t msg_id, const void *broadcast_data, size_t broadcast_data_len);
int32_t ipc_nng_long_send_multicast(uint32_t src_id, const ipc_nng_long_mcast_dest_t *dest, uint32_t msg_id, const void *mcast_data, uint32_t mcast_data_len, size_t timeout);

#ifdef __cplusplus
}
#endif
