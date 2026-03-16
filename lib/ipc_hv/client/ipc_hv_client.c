#include <pthread.h>

#include "utility/utils.h"
#include "ipc_hv.h"

static void ipc_msg_handle(const struct _ipc_hv_msg_t *msg, uint8_t *response_data, size_t *response_data_len)
{
    if (NULL == msg)
    {
        return;
    }

    if (E_IPC_HV_MSG_TYPE_NOTIFY == msg->msg_type)
    {
        if (E_IPC_HV_MSG_ID_TEST_NOTIFY == msg->msg_id)
        {
            if (msg->send_len > 0)
            {
                LOG_PRINT_INFO("recv notify data[%s]", msg->msg_data);
            }
            else
            {
                LOG_PRINT_INFO("recv notify data");
            }
        }
        else
        {
            LOG_PRINT_ERROR("invalid msg_id[%u]! ", msg->msg_id);
        }
    }
    else if (E_IPC_HV_MSG_TYPE_ASYNC_REQ == msg->msg_type)
    {
        if (E_IPC_HV_MSG_ID_TEST_ASYNC == msg->msg_id)
        {
            if (msg->send_len > 0)
            {
                LOG_PRINT_INFO("recv async req data[%s]", msg->msg_data);
            }
            else
            {
                LOG_PRINT_INFO("recv async req data");
            }

            if (NULL != response_data && NULL != response_data_len && *response_data_len >= (uint32_t)strlen("async resp"))
            {
                memcpy(response_data, (const uint8_t *)"async resp", strlen("async resp"));
                *response_data_len = strlen("async resp");
            }
        }
        else
        {
            LOG_PRINT_ERROR("invalid msg_id[%u]! ", msg->msg_id);
        }
    }
    else if (E_IPC_HV_MSG_TYPE_SYNC_REQ == msg->msg_type)
    {
        if (E_IPC_HV_MSG_ID_TEST_SYNC == msg->msg_id)
        {
            if (msg->send_len > 0)
            {
                LOG_PRINT_INFO("recv sync req data[%s]", msg->msg_data);
            }
            else
            {
                LOG_PRINT_INFO("recv sync req data");
            }

            if (NULL != response_data && NULL != response_data_len && *response_data_len >= (uint32_t)strlen("sync resp"))
            {
                memcpy(response_data, (const uint8_t *)"sync resp", strlen("sync resp"));
                *response_data_len = strlen("sync resp");
            }
        }
        else
        {
            LOG_PRINT_ERROR("invalid msg_id[%u]! ", msg->msg_id);
        }
    }
    else if (E_IPC_HV_MSG_TYPE_ASYNC_REP == msg->msg_type)
    {
        if (E_IPC_HV_MSG_ID_TEST_ASYNC == msg->msg_id)
        {
            if (msg->send_len > 0)
            {
                LOG_PRINT_INFO("recv async resp data[%s]", msg->msg_data);
            }
            else
            {
                LOG_PRINT_INFO("recv async resp data");
            }
        }
        else
        {
            LOG_PRINT_ERROR("invalid msg_id[%u]! ", msg->msg_id);
        }
    }
    else
    {
        LOG_PRINT_ERROR("invalid msg_type[%u]! ", msg->msg_type);
    }
}

int main(void)
{
    LOG_PRINT_INFO("ipc_hv_client start!");

    int ret = -1;

    ipc_hv_register_info_t reg_info = {};
    reg_info.module_id = E_IPC_HV_ID_CLIENT;
    strncpy(reg_info.module_name, "ipc_hv_client", sizeof(reg_info.module_name));
    reg_info.response_cb = ipc_msg_handle;
    ret = ipc_hv_init(&reg_info);
    if (0 != ret)
    {
        LOG_PRINT_ERROR("ipc_hv_init fail, ret[%d] ", ret);
        return -1;
    }

    // notify
    ret = ipc_hv_send_notify(E_IPC_HV_ID_CLIENT, E_IPC_HV_ID_SERVER, E_IPC_HV_MSG_ID_TEST_NOTIFY, NULL, 0);
    if (0 != ret)
    {
        LOG_PRINT_ERROR("ipc_hv_send_notify fail, ret[%d]", ret);
        return -1;
    }

    // sync
    char sync_resp[16] = {};
    size_t sync_resp_len = sizeof(sync_resp);
    ret = ipc_hv_send_sync(E_IPC_HV_ID_CLIENT, E_IPC_HV_ID_SERVER, E_IPC_HV_MSG_ID_TEST_SYNC, NULL, 0, (uint8_t *)sync_resp, &sync_resp_len, 1000);
    if (0 != ret)
    {
        LOG_PRINT_ERROR("ipc_hv_send_sync fail, ret[%d]", ret);
        return -1;
    }
    LOG_PRINT_INFO("recv sync_resp[%s][%zu]", sync_resp, sync_resp_len);

    // async
    ret = ipc_hv_send_async(E_IPC_HV_ID_CLIENT, E_IPC_HV_ID_SERVER, E_IPC_HV_MSG_ID_TEST_ASYNC, NULL, 0, 16, ipc_msg_handle, 1000);
    if (0 != ret)
    {
        LOG_PRINT_ERROR("ipc_hv_send_async fail, ret[%d]", ret);
        return -1;
    }

    ipc_hv_destroy(E_IPC_HV_ID_CLIENT);

    return 0;
}
