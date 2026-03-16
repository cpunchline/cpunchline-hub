#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include <unistd.h>

#define UNIX_SOCKET_NAME_LEN_MAX (256)
#define IPC_HV_MODULE_NAME       (64)
#define IPC_HV_CONNECT_TIMEOUT   (1000)
#define IPC_HV_WRITE_TIMEOUT     (1000)
#define IPC_HV_READ_TIMEOUT      (1000)
#define IPC_HV_CLOSE_TIMEOUT     (1000)

typedef enum _ipc_hv_id_e
{
    E_IPC_HV_ID_INVALID,
    E_IPC_HV_ID_SERVER = 1,
    E_IPC_HV_ID_CLIENT = 2,

    E_IPC_HV_ID_MAX
} ipc_hv_id_e;

typedef enum _ipc_hv_msg_id_e
{
    E_IPC_HV_MSG_ID_INVALID = 0,
    E_IPC_HV_MSG_ID_TEST_NOTIFY,
    E_IPC_HV_MSG_ID_TEST_ASYNC,
    E_IPC_HV_MSG_ID_TEST_SYNC,
} ipc_hv_msg_id_e;

typedef enum _ipc_hv_msg_type_e
{
    E_IPC_HV_MSG_TYPE_INVALUD = 0,
    E_IPC_HV_MSG_TYPE_NOTIFY,
    E_IPC_HV_MSG_TYPE_SYNC_REQ,
    E_IPC_HV_MSG_TYPE_ASYNC_REQ,
    E_IPC_HV_MSG_TYPE_SYNC_REP,
    E_IPC_HV_MSG_TYPE_ASYNC_REP,
} ipc_hv_msg_type_e;

struct _ipc_hv_msg_t;
typedef void (*ipc_hv_handle_cb_t)(const struct _ipc_hv_msg_t *msg, uint8_t *response_data, size_t *response_data_len);

typedef struct _ipc_hv_register_info_t
{
    uint32_t module_id; // see ipc_hv_id_e
    char module_name[IPC_HV_MODULE_NAME];
    ipc_hv_handle_cb_t response_cb;
} ipc_hv_register_info_t;

typedef struct _ipc_hv_msg_t
{
    uint32_t src;
    uint32_t dest;
    uint32_t msg_type;
    uint32_t msg_id;
    size_t timeout;
    void *ctx;
    size_t send_len;
    size_t recv_max_len;
    uint8_t msg_data[];
} ipc_hv_msg_t;

int ipc_hv_init(ipc_hv_register_info_t *reg_info);
void ipc_hv_destroy(ipc_hv_id_e id);

int ipc_hv_send_notify(uint32_t src, uint32_t dest, uint32_t msg_id, const uint8_t *notify_data, size_t notify_len);
int ipc_hv_send_async(uint32_t src, uint32_t dest, uint32_t msg_id, const uint8_t *req_data, size_t req_len, size_t resp_max_len, ipc_hv_handle_cb_t async_cb, size_t timeout);
int ipc_hv_send_sync(uint32_t src, uint32_t dest, uint32_t msg_id, const uint8_t *req_data, size_t req_len, uint8_t *resp_data, size_t *resp_len, size_t timeout);

#ifdef __cplusplus
}
#endif
