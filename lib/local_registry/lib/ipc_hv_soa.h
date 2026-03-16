#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include <stdbool.h>

#define IPC_HV_SOA_FOREACH_ERR(X)              \
    X(0, SUCCESS, "Success")                   \
    X(-1, FAIL, "Fail")                        \
    X(-2, ERR_ARG, "Invalid param")            \
    X(-3, ERR_MEM, "Memory allocation failed") \
    X(-4, ERR_AGAIN, "again operation")        \
    X(-5, ERR_LIMIT, "Limit exceeded")         \
    X(-6, ERR_MSG_ENCODE, "Encode error")      \
    X(-7, ERR_MSG_DECODE, "Decode error")      \
    X(-8, TIMEOUT, "Timeout")                  \
    X(-9, ERR_EXISTS, "EXISTS")                \
    X(-10, ERR_NOT_EXISTS, "NOT EXISTS")       \
    X(-16, ERR_OTHER, "Not found")

enum
{
#define X(code, name, msg) IPC_HV_SOA_RET_##name = (code),
    IPC_HV_SOA_FOREACH_ERR(X)
#undef X
};

typedef enum _ipc_hv_soa_service_type_e
{
    E_IPC_HV_SOA_SERVICE_TYPE_INVALID = 0,
    E_IPC_HV_SOA_SERVICE_TYPE_METHOD = 1,
    E_IPC_HV_SOA_SERVICE_TYPE_EVENT = 2,
    E_IPC_HV_SOA_SERVICE_TYPE_MAX,
} ipc_hv_soa_service_type_e; // same as local_service_type

typedef enum _ipc_hv_soa_msg_type_e
{
    E_IPC_HV_SOA_MSG_TYPE_INVALID,
    E_IPC_HV_SOA_MSG_TYPE_METHOD_NOTIFY = 1,
    E_IPC_HV_SOA_MSG_TYPE_EVENT = 2,
    E_IPC_HV_SOA_MSG_TYPE_METHOD_REQUEST_SYNC = 3,
    E_IPC_HV_SOA_MSG_TYPE_METHOD_RESPONSE_SYNC = 4,
    E_IPC_HV_SOA_MSG_TYPE_METHOD_REQUEST_ASYNC = 5,
    E_IPC_HV_SOA_MSG_TYPE_METHOD_RESPONSE_ASYNC = 6,
    E_IPC_HV_SOA_MSG_TYPE_MAX,
} ipc_hv_soa_msg_type_e;

typedef struct _ipc_hv_soa_msg_handle_t
{
    // sync/async request info
    uint32_t msg_type;
    uint32_t msg_seqid;
    uint32_t client_id;
} ipc_hv_soa_msg_handle_t;

typedef void (*PF_IPC_HV_SOA_METHOD_PROVIDER_CB)(ipc_hv_soa_msg_handle_t handle, uint32_t service_id, void *method_req_data, uint32_t method_req_data_len);
typedef void (*PF_IPC_HV_SOA_METHOD_ASYNC_CB)(uint32_t service_id, void *method_resp_data, uint32_t method_resp_data_len);
typedef void (*PF_IPC_HV_SOA_SERVICE_STATUS_CB)(uint32_t service_id, uint32_t service_status);
typedef int32_t (*PF_IPC_HV_SOA_EVENT_LISTEN_CB)(uint32_t service_id, void **event_data, uint32_t *event_data_len);
typedef void (*PF_IPC_HV_SOA_EVENT_LISTENER_CB)(uint32_t service_id, void *event_data, uint32_t event_data_len);
typedef void (*PF_IPC_HV_SOA_TIMER_CB)(uint32_t timer_id, void *timer_data);

typedef struct
{
    uint32_t service_id;
    uint32_t service_type;
    union
    {
        PF_IPC_HV_SOA_METHOD_PROVIDER_CB method_provider_cb; // method
        PF_IPC_HV_SOA_EVENT_LISTEN_CB event_provider_cb;     // event(listener change)
    } u;
} ipc_hv_soa_provider_service_t;

typedef struct
{
    uint32_t service_id;
    uint32_t service_type;
    union
    {
        PF_IPC_HV_SOA_SERVICE_STATUS_CB method_listener_cb; // method (status change)
        PF_IPC_HV_SOA_EVENT_LISTENER_CB event_listener_cb;  // event
    } u;
} ipc_hv_soa_listener_service_t;

int32_t ipc_hv_soa_init(uint32_t *client_id);
int32_t ipc_hv_soa_destroy(uint32_t client_id);

// Provider APIs
int32_t ipc_hv_soa_provider_service_offer(ipc_hv_soa_provider_service_t *provider_services, uint32_t provider_services_size);
int32_t ipc_hv_soa_provider_service_revoke(uint32_t *provider_services, uint32_t provider_services_size);
int32_t ipc_hv_soa_provider_service_set_status(uint32_t *provider_services, uint32_t provider_services_size, uint32_t provider_services_status);

// Listener APIs
int32_t ipc_hv_soa_listener_service_subscribe(ipc_hv_soa_listener_service_t *listener_services, uint32_t listener_services_size);
int32_t ipc_hv_soa_listener_service_unsubscribe(uint32_t *listener_services, uint32_t listener_services_size);

// Method Service APIs
/*
Request_NoReturn notify
Request_Return   sync
Request_Return   async
Response         complete
*/
int32_t ipc_hv_soa_method_notify(uint32_t service_id, void *method_req_data, uint32_t method_req_data_len);
int32_t ipc_hv_soa_method_sync(uint32_t service_id, void *method_req_data, uint32_t method_req_data_len, void *method_resp_data, uint32_t *method_resp_data_len, uint32_t timeout_ms);
int32_t ipc_hv_soa_method_async(uint32_t service_id, void *method_req_data, uint32_t method_req_data_len, PF_IPC_HV_SOA_METHOD_ASYNC_CB async_cb);
int32_t ipc_hv_soa_method_complete(ipc_hv_soa_msg_handle_t handle, uint32_t service_id, void *method_resp_data, uint32_t method_resp_data_len);

// Event Service APIs
int32_t ipc_hv_soa_event_trigger(uint32_t service_id, void *event_data, uint32_t event_data_len);

// Timer APIs
// repeat 0 invalid; 1 once; UINT32_MAX cycle; other mult;
int32_t ipc_hv_soa_timer_create(uint32_t timer_id, uint32_t repeat, uint32_t interval_ms, PF_IPC_HV_SOA_TIMER_CB timer_cb, void *timer_data);
int32_t ipc_hv_soa_timer_reset(uint32_t timer_id, uint32_t interval_ms);
int32_t ipc_hv_soa_timer_delete(uint32_t timer_id);
bool ipc_hv_soa_timer_exist(uint32_t timer_id);

#ifdef __cplusplus
}
#endif
