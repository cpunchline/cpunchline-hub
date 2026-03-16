#include <time.h>
#include <sys/time.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "ipc_zmq.h"
#include "utility/utils.h"

static void ipc_zmq_notify_handler_cb(ipc_zmq_msg_baseinfo_t *msg_baseinfo, const uint8_t *notify_data)
{
    LOG_PRINT_DEBUG("notify src_id[%u]-dest_id[%u]-msg_id[%u]", msg_baseinfo->src_id, msg_baseinfo->dest_id,
                    msg_baseinfo->msg_id);
    if (E_IPC_ZMQ_MSG_ID_TEST_NOTIFY == msg_baseinfo->msg_id)
    {
        LOG_PRINT_BUF("notify_data", notify_data, (uint32_t)msg_baseinfo->send_len);
    }
}

static void ipc_zmq_async_handler_cb(ipc_zmq_msg_baseinfo_t *msg_baseinfo, uint8_t *response_data)
{
    LOG_PRINT_DEBUG("async src_id[%u]-dest_id[%u]-msg_id[%u]", msg_baseinfo->src_id, msg_baseinfo->dest_id,
                    msg_baseinfo->msg_id);
    if (E_IPC_ZMQ_MSG_ID_TEST_ASYNC == msg_baseinfo->msg_id)
    {
        LOG_PRINT_BUF("async_data_resp", response_data, (uint32_t)msg_baseinfo->recv_len);
    }
}

static void ipc_zmq_broadcast_handler_cb(ipc_zmq_broadcast_msg_baseinfo_t *bmsg_baseinfo, const uint8_t *broadcast_data)
{
    LOG_PRINT_DEBUG("broadcast src_id[%u]-msg_id[%u]", bmsg_baseinfo->src_id, bmsg_baseinfo->msg_id);
    if (E_IPC_ZMQ_MSG_ID_TEST_BROADCAST == bmsg_baseinfo->msg_id)
    {
        LOG_PRINT_BUF("broadcast_data", broadcast_data, (uint32_t)sizeof(int));
    }
}

static void ipc_zmq_response_handler_cb(ipc_zmq_msg_baseinfo_t *msg_baseinfo, const uint8_t *indata, uint8_t *outdata)
{
    LOG_PRINT_DEBUG("response src_id[%u]-dest_id[%u]-msg_id[%u]", msg_baseinfo->src_id, msg_baseinfo->dest_id,
                    msg_baseinfo->msg_id);
    LOG_PRINT_BUF("indata", indata, (uint32_t)msg_baseinfo->send_len);

    if (E_IPC_ZMQ_MSG_ID_TEST_SYNC == msg_baseinfo->msg_id)
    {
        int sync_resp = 2;
        memcpy(outdata, &sync_resp, sizeof(sync_resp));
        LOG_PRINT_DEBUG("send sync response");
    }
    else if (E_IPC_ZMQ_MSG_ID_TEST_ASYNC == msg_baseinfo->msg_id)
    {
        int async_resp = 3;
        memcpy(outdata, &async_resp, sizeof(async_resp));
        LOG_PRINT_DEBUG("send async response");
    }
}

static void ipc_zmq_stream_handler_cb(ipc_zmq_msg_baseinfo_t *msg_baseinfo, const uint8_t *stream_data)
{
    LOG_PRINT_DEBUG("stream src_id[%u]-dest_id[%u]-msg_id[%u]", msg_baseinfo->src_id, msg_baseinfo->dest_id,
                    msg_baseinfo->msg_id);
    LOG_PRINT_BUF("stream_data", stream_data, (uint32_t)msg_baseinfo->send_len);
}

int main()
{
    int ret = -1;
    ipc_zmq_register_info_t reg_info = {0};

    LOG_PRINT_INFO("ipc zmq manager is running!");

    reg_info.module_id = E_IPC_ZMQ_MODULE_ID_MANAGER;
    reg_info.work_mode = E_IPC_ZMQ_WORKMODE_MULT_WORKER;
    strncpy(reg_info.module_name, "ipc_zmq_manager", sizeof(reg_info.module_name));
    reg_info.notify_handler = ipc_zmq_notify_handler_cb;
    reg_info.response_handler = ipc_zmq_response_handler_cb;
    reg_info.async_handler = ipc_zmq_async_handler_cb;
    reg_info.broadcast_handler = ipc_zmq_broadcast_handler_cb;
    reg_info.stream_handler = ipc_zmq_stream_handler_cb;

    ret = ipc_zmq_init(&reg_info);
    if (0 != ret)
    {
        LOG_PRINT_ERROR("ipc_zmq_init fail, ret[%d]!", ret);
        return ret;
    }

    while (1)
    {
        sleep(1);
    }

    ipc_zmq_destroy(E_IPC_ZMQ_MODULE_ID_MANAGER);

    return 0;
}
