#include "utility/utils.h"
#include "ipc_hv_soa.h"
#include "generator_autolib.h"

static void method_provider_cb(ipc_hv_soa_msg_handle_t handle, uint32_t service_id, void *method_req_data, uint32_t method_req_data_len)
{
    (void)method_req_data;
    LOG_PRINT_INFO("service_id[%d], method_req_data_len[%d]", service_id, method_req_data_len);
    LOG_PRINT_INFO("msg_type[%d], msg_seqid[%d], client_id[%d]", handle.msg_type, handle.msg_seqid, handle.client_id);
    switch (handle.msg_type)
    {
        case E_IPC_HV_SOA_MSG_TYPE_METHOD_NOTIFY:
            LOG_PRINT_INFO("recv notify");
            break;
        case E_IPC_HV_SOA_MSG_TYPE_METHOD_REQUEST_SYNC:
            LOG_PRINT_INFO("recv sync");
            static int32_t sync_value = 0;
            sync_value++;
            ipc_hv_soa_method_complete(handle, service_id, &sync_value, sizeof(sync_value));
            break;
        case E_IPC_HV_SOA_MSG_TYPE_METHOD_REQUEST_ASYNC:
            LOG_PRINT_INFO("recv async");
            static int32_t async_value = 0;
            async_value++;
            ipc_hv_soa_method_complete(handle, service_id, &async_value, sizeof(async_value));
        default:
            break;
    }
}

static int32_t event_provider_cb(uint32_t service_id, void **event_data, uint32_t *event_data_len)
{
    *event_data = NULL;
    *event_data_len = 0;
    LOG_PRINT_INFO("provider service_id[%d], ready to send event data_len[%d]", service_id, *event_data_len);
    return 0;
}

ipc_hv_soa_provider_service_t providers[] = {
    {
     .service_id = IPC_HV_SOA_CLIENT1_SERVICE_ID_METHOD_TEST_METHOD_1,
     .service_type = E_IPC_HV_SOA_SERVICE_TYPE_METHOD,
     .u.method_provider_cb = method_provider_cb,
     },
    {
     .service_id = IPC_HV_SOA_CLIENT1_SERVICE_ID_EVENT_TEST_EVENT_1,
     .service_type = E_IPC_HV_SOA_SERVICE_TYPE_EVENT,
     .u.event_provider_cb = event_provider_cb,
     },
};

static void method_status_listener_cb(uint32_t service_id, uint32_t service_status)
{
    LOG_PRINT_INFO("listener service_id[%d] service_status[%d]", service_id, service_status);
}

static void method_async_cb(uint32_t service_id, void *method_resp_data, uint32_t method_resp_data_len)
{
    (void)method_resp_data;
    LOG_PRINT_INFO("async resp service_id[%d] method_resp_data_len[%d]", service_id, method_resp_data_len);
}

static void event_listener_cb(uint32_t service_id, void *event_data, uint32_t event_data_len)
{
    (void)event_data;
    LOG_PRINT_INFO("listener service_id[%d] event_data_len[%d]", service_id, event_data_len);
}

ipc_hv_soa_listener_service_t listeners[] = {
    {
     .service_id = IPC_HV_SOA_CLIENT2_SERVICE_ID_METHOD_TEST_METHOD_2,
     .service_type = E_IPC_HV_SOA_SERVICE_TYPE_METHOD,
     .u.method_listener_cb = method_status_listener_cb,
     },
    {
     .service_id = IPC_HV_SOA_CLIENT2_SERVICE_ID_EVENT_TEST_EVENT_2,
     .service_type = E_IPC_HV_SOA_SERVICE_TYPE_EVENT,
     .u.event_listener_cb = event_listener_cb,
     },
};

uint32_t unused_providers[] = {IPC_HV_SOA_CLIENT1_SERVICE_ID_METHOD_TEST_METHOD_1, IPC_HV_SOA_CLIENT1_SERVICE_ID_EVENT_TEST_EVENT_1};
uint32_t unused_listeners[] = {IPC_HV_SOA_CLIENT2_SERVICE_ID_METHOD_TEST_METHOD_2, IPC_HV_SOA_CLIENT2_SERVICE_ID_EVENT_TEST_EVENT_2};
uint32_t test_timer = 1;

static void on_test_timer_cb(uint32_t timer_id, void *timer_data)
{
    (void)timer_id;
    (void)timer_data;
    int32_t ret = IPC_HV_SOA_RET_SUCCESS;
    ret = ipc_hv_soa_event_trigger(IPC_HV_SOA_CLIENT1_SERVICE_ID_EVENT_TEST_EVENT_1, NULL, 0);
    if (IPC_HV_SOA_RET_SUCCESS != ret)
    {
        LOG_PRINT_ERROR("ipc_hv_soa_event_trigger fail, ret[%d]", ret);
    }

    ret = ipc_hv_soa_method_notify(IPC_HV_SOA_CLIENT2_SERVICE_ID_METHOD_TEST_METHOD_2, NULL, 0);
    if (IPC_HV_SOA_RET_SUCCESS != ret)
    {
        LOG_PRINT_ERROR("ipc_hv_soa_method_notify fail, ret[%d]", ret);
    }

    int32_t sync_resp_data = 0;

    uint32_t sync_resp_data_len = sizeof(sync_resp_data);
    ret = ipc_hv_soa_method_sync(IPC_HV_SOA_CLIENT2_SERVICE_ID_METHOD_TEST_METHOD_2, NULL, 0, &sync_resp_data, &sync_resp_data_len, 1000);
    if (IPC_HV_SOA_RET_SUCCESS != ret)
    {
        LOG_PRINT_ERROR("ipc_hv_soa_method_sync fail, ret[%d]", ret);
    }
    else
    {
        LOG_PRINT_INFO("sync resp data_len[%d] data[%d]", sync_resp_data_len, sync_resp_data);
    }

    ret = ipc_hv_soa_method_async(IPC_HV_SOA_CLIENT2_SERVICE_ID_METHOD_TEST_METHOD_2, NULL, 0, method_async_cb);
    if (IPC_HV_SOA_RET_SUCCESS != ret)
    {
        LOG_PRINT_ERROR("ipc_hv_soa_method_sync fail, ret[%d]", ret);
    }
}

int main(void)
{
    int32_t ret = -1;
    uint32_t client_id = 0;

    do
    {
        ret = ipc_hv_soa_init(&client_id);
        if (IPC_HV_SOA_RET_SUCCESS != ret)
        {
            LOG_PRINT_ERROR("ipc_hv_soa_init fail, ret[%d]", ret);
            break;
        }
        LOG_PRINT_INFO("ipc_hv_soa_init client_id[%d]", client_id);

        ret = ipc_hv_soa_provider_service_offer(providers, UTIL_ARRAY_SIZE(providers));
        if (IPC_HV_SOA_RET_SUCCESS != ret)
        {
            LOG_PRINT_ERROR("ipc_hv_soa_provider_service_offer fail, ret[%d]", ret);
            break;
        }

#if 0
        ret = ipc_hv_soa_provider_service_revoke(unused_providers, UTIL_ARRAY_SIZE(unused_providers));
        if (IPC_HV_SOA_RET_SUCCESS != ret)
        {
            LOG_PRINT_ERROR("ipc_hv_soa_provider_service_revoke fail, ret[%d]", ret);
            break;
        }
#endif

        ret = ipc_hv_soa_listener_service_subscribe(listeners, UTIL_ARRAY_SIZE(listeners));
        if (IPC_HV_SOA_RET_SUCCESS != ret)
        {
            LOG_PRINT_ERROR("ipc_hv_soa_listener_service_subscribe fail, ret[%d]", ret);
            break;
        }

#if 0
        ret = ipc_hv_soa_listener_service_unsubscribe(unused_listeners, UTIL_ARRAY_SIZE(unused_listeners));
        if (IPC_HV_SOA_RET_SUCCESS != ret)
        {
            LOG_PRINT_ERROR("ipc_hv_soa_listener_service_unsubscribe fail, ret[%d]", ret);
            break;
        }
#endif

        ret = IPC_HV_SOA_RET_SUCCESS;
    } while (0);

    if (IPC_HV_SOA_RET_SUCCESS != ret)
    {
        ipc_hv_soa_destroy(client_id);
        return ret;
    }

    ret = ipc_hv_soa_timer_create(test_timer, 3, 5 * 1000, on_test_timer_cb, NULL);
    if (IPC_HV_SOA_RET_SUCCESS != ret)
    {
        LOG_PRINT_ERROR("ipc_hv_soa_timer_create fail, ret[%d]", ret);
    }

    while (1)
    {
        sleep(1);
        if (!ipc_hv_soa_timer_exist(test_timer))
        {
            break;
        }
    }

    ipc_hv_soa_destroy(client_id);

    return ret;
}
