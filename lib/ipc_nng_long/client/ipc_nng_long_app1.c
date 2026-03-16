#include <time.h>
#include <sys/time.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "utility/utils.h"
#include "ipc_nng_long.h"

static void ipc_nng_long_async_handler_cb(ipc_nng_long_msg_baseinfo_t *msg_baseinfo, uint8_t *response_data)
{
    LOG_PRINT_DEBUG("async src_id[%u]-dest_id[%u]-msg_id[%u]", msg_baseinfo->src_id, msg_baseinfo->dest_id,
                    msg_baseinfo->msg_id);
    if (E_IPC_NNG_LONG_MSG_ID_TEST_ASYNC == msg_baseinfo->msg_id)
    {
        LOG_PRINT_BUF("async_data_resp", response_data, (uint32_t)msg_baseinfo->recv_len);
    }
}

static void ipc_nng_long_broadcast_handler_cb(ipc_nng_long_msg_baseinfo_t *msg_baseinfo, const uint8_t *broadcast_data)
{
    LOG_PRINT_DEBUG("broadcast src_id[%u]-msg_id[%u]", msg_baseinfo->src_id, msg_baseinfo->msg_id);
    if (E_IPC_NNG_LONG_MSG_ID_TEST_BROADCAST == msg_baseinfo->msg_id)
    {
        LOG_PRINT_BUF("broadcast_data", broadcast_data, (uint32_t)sizeof(int));
    }
}

static void ipc_nng_long_response_handler_cb(ipc_nng_long_msg_baseinfo_t *msg_baseinfo, const uint8_t *indata, uint8_t *outdata)
{
    LOG_PRINT_DEBUG("response src_id[%u]-dest_id[%u]-msg_id[%u]", msg_baseinfo->src_id, msg_baseinfo->dest_id,
                    msg_baseinfo->msg_id);
    LOG_PRINT_BUF("indata", indata, (uint32_t)msg_baseinfo->send_len);

    if (E_IPC_NNG_LONG_MSG_ID_TEST_SYNC == msg_baseinfo->msg_id)
    {
        int sync_resp = 2;
        memcpy(outdata, &sync_resp, sizeof(sync_resp));
        LOG_PRINT_DEBUG("send sync response");
    }
    else if (E_IPC_NNG_LONG_MSG_ID_TEST_ASYNC == msg_baseinfo->msg_id)
    {
        int async_resp = 3;
        memcpy(outdata, &async_resp, sizeof(async_resp));
        LOG_PRINT_DEBUG("send async response");
    }
    else if (E_IPC_NNG_LONG_MSG_ID_TEST_NOTIFY == msg_baseinfo->msg_id)
    {
        LOG_PRINT_DEBUG("send notify response");
    }
}

int main(void)
{
    int ret = -1;
    ipc_nng_long_register_info_t reg_info = {0};

    LOG_PRINT_INFO("ipc nng long app1 is running!");

    reg_info.module_id = E_IPC_NNG_LONG_MODULE_ID_TEST1;
    strncpy(reg_info.module_name, "ipc_nng_long_app1", sizeof(reg_info.module_name));
    reg_info.response_handler = ipc_nng_long_response_handler_cb;
    reg_info.async_handler = ipc_nng_long_async_handler_cb;
    reg_info.broadcast_handler = ipc_nng_long_broadcast_handler_cb;

    ret = ipc_nng_long_init(&reg_info);
    if (0 != ret)
    {
        LOG_PRINT_ERROR("ipc_nng_long_init fail, ret[%d]!", ret);
        return ret;
    }

    int notify = 1;
    ret = ipc_nng_long_send_notify(E_IPC_NNG_LONG_MODULE_ID_TEST1, E_IPC_NNG_LONG_MODULE_ID_TEST2, E_IPC_NNG_LONG_MSG_ID_TEST_NOTIFY,
                                   (uint8_t *)&notify, sizeof(int));
    if (0 != ret)
    {
        LOG_PRINT_ERROR("ipc_nng_long_send_notify fail, ret[%d]!", ret);
        return ret;
    }

    int sync_req = 2;
    int sync_resp = 0;
    size_t sync_resp_len = 0;
    ret =
        ipc_nng_long_send_sync(E_IPC_NNG_LONG_MODULE_ID_TEST1, E_IPC_NNG_LONG_MODULE_ID_TEST2, E_IPC_NNG_LONG_MSG_ID_TEST_SYNC,
                               (uint8_t *)&sync_req, sizeof(int), (uint8_t *)&sync_resp, &sync_resp_len, sizeof(int), 3000);
    if (0 != ret)
    {
        LOG_PRINT_ERROR("ipc_nng_long_send_sync fail, ret[%d]!", ret);
        return ret;
    }
    LOG_PRINT_INFO("recv sync_resp_len[%zd], sync_resp[%d]", sync_resp_len, sync_resp);

    int async_req = 3;
    ret = ipc_nng_long_send_async(E_IPC_NNG_LONG_MODULE_ID_TEST1, E_IPC_NNG_LONG_MODULE_ID_TEST2, E_IPC_NNG_LONG_MSG_ID_TEST_ASYNC,
                                  (uint8_t *)&async_req, sizeof(async_req), sizeof(int), 1500);
    if (0 != ret)
    {
        LOG_PRINT_ERROR("ipc_nng_long_send_async fail, ret[%d]!", ret);
        return ret;
    }

    int broadcast = 4;
    ret = ipc_nng_long_send_broadcast(E_IPC_NNG_LONG_MODULE_ID_TEST1, E_IPC_NNG_LONG_MSG_ID_TEST_BROADCAST, (uint8_t *)&broadcast,
                                      sizeof(int));
    if (0 != ret)
    {
        LOG_PRINT_ERROR("ipc_nng_long_send_broadcast fail, ret[%d]!", ret);
        return ret;
    }

    while (1)
    {
        sleep(1);
    }

    ipc_nng_long_destroy(E_IPC_NNG_LONG_MODULE_ID_TEST1);

    return 0;
}
