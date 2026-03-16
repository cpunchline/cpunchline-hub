#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include "ipc_nng_long.h"
#include "utility/utils.h"

static void signal_handler(int signum)
{
    LOG_PRINT_ERROR("signum[%d]", signum);
    exit(0);
}

static void _ipc_porxy_broadcast_cb(ipc_nng_long_msg_baseinfo_t *msg_baseinfo, const uint8_t *broadcast_data)
{
    LOG_PRINT_DEBUG("recv broadcast: src_id[%u]->dest_id[%u], req_data_len[%zd], broadcast_data[%p]",
                    msg_baseinfo->src_id,
                    msg_baseinfo->dest_id,
                    msg_baseinfo->send_len,
                    broadcast_data);
}

static void _ipc_porxy_response_cb(ipc_nng_long_msg_baseinfo_t *msg_baseinfo, const uint8_t *indata, uint8_t *outdata)
{
    LOG_PRINT_DEBUG("recv msg: src_id[%u]->dest_id[%u], req_data_len[%zd], req_data[%p], resp_data_max_len[%zd], resp_data[%p]",
                    msg_baseinfo->src_id,
                    msg_baseinfo->dest_id,
                    msg_baseinfo->send_len,
                    indata,
                    msg_baseinfo->recv_len,
                    outdata);
}

static int32_t _ipc_porxy_init(void)
{
    int32_t ret = -1;

    ipc_nng_long_register_info_t reg_info = {};

    reg_info.module_id = E_IPC_NNG_LONG_MODULE_ID_DAEMON;
    memcpy(reg_info.module_name, "ipc_daemon", sizeof("ipc_daemon"));
    reg_info.response_handler = _ipc_porxy_response_cb;
    reg_info.async_handler = NULL;
    reg_info.broadcast_handler = _ipc_porxy_broadcast_cb;
    reg_info.stream_handler = NULL;

    ret = ipc_nng_long_init(&reg_info);

    return ret;
}

int main(void)
{
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGQUIT, signal_handler);
    signal(SIGKILL, signal_handler);

    if (0 != _ipc_porxy_init())
    {
        LOG_PRINT_ERROR("_ipc_porxy_init fail!");
        return -1;
    }

    while (1)
    {
        sleep(10);
    }

    return 0;
}
