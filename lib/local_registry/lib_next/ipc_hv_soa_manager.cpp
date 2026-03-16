#include "ipc_hv_soa_manager.hpp"

SINGLETON_IMPL(ipc_hv_soa_manager);

static void _ipc_hv_logger(int level, const char *buf, int len)
{
    (void)len;
    switch (level)
    {
        case LOG_LEVEL_FATAL:
            LOG_PRINT_CRIT("%s", buf);
            break;
        case LOG_LEVEL_ERROR:
            LOG_PRINT_ERROR("%s", buf);
            break;
        case LOG_LEVEL_WARN:
            LOG_PRINT_WARN("%s", buf);
            break;
        case LOG_LEVEL_INFO:
            LOG_PRINT_INFO("%s", buf);
            break;
        case LOG_LEVEL_DEBUG:
        case LOG_LEVEL_SILENT:
        case LOG_LEVEL_VERBOSE:
            LOG_PRINT_DEBUG("%s", buf);
            break;
        default:
            break;
    }
}

ipc_hv_soa_manager::ipc_hv_soa_manager()
{
    int32_t ret = IPC_HV_SOA_RET_SUCCESS;

    hlog_set_handler(_ipc_hv_logger);
    hlog_set_level(LOG_LEVEL_INFO);
    hlog_set_format("HV-%L %s");

    m_worker_loop_pool.setThreadNum(LOCAL_REGISTRY_CLIENT_HV_LOOP_NUM_MAX);
    m_worker_loop_pool.start(true);

    pid_t pid = getpid();
    char pname[LOCAL_REGISTRY_CLIENT_NAME_MAX] = {};
    if (0 != util_get_exec_name(pid, pname, sizeof(pname)))
    {
        LOG_PRINT_ERROR("util_get_exec_name fail!");
        return;
    }

    char client_localaddr[LOCAL_REGISTRY_SOCKET_LEN_MAX] = {};
    snprintf(client_localaddr, sizeof(client_localaddr), LOCAL_REGISTEY_SOCKET_FMT, pid, pname);

    int connfd = m_daemon_client.createsocket(LOCAL_REGISTRY_SOCKET_PATH);
    if (connfd < 0)
    {
        LOG_PRINT_ERROR("createsocket fail");
        return;
    }

    m_daemon_client.bind(client_localaddr);
    unpack_setting_t daemon_unpack_setting = {};
    daemon_unpack_setting.mode = UNPACK_BY_LENGTH_FIELD;
    daemon_unpack_setting.package_max_length = LOCAL_REGISTRY_MSG_HEADER_SIZE + LOCAL_REGISTRY_MSG_SIZE_MAX;
    daemon_unpack_setting.body_offset = LOCAL_REGISTRY_MSG_HEADER_SIZE;
    daemon_unpack_setting.length_field_offset = sizeof(uint32_t);
    daemon_unpack_setting.length_field_bytes = sizeof(uint32_t);
    daemon_unpack_setting.length_adjustment = 0;
    daemon_unpack_setting.length_field_coding = ENCODE_BY_LITTEL_ENDIAN;
    m_daemon_client.setUnpack(&daemon_unpack_setting);
    m_daemon_client.onConnection = ipc_hv_soa_manager::onDaemonConnection;
    m_daemon_client.onMessage = ipc_hv_soa_manager::onDaemonMessage;
    m_daemon_client.onWriteComplete = ipc_hv_soa_manager::onDaemonWriteComplete;

    ret = connect_to_daemon();
    if (ret != IPC_HV_SOA_RET_SUCCESS)
    {
        LOG_PRINT_ERROR("connect_to_daemon fail");
        return;
    }

    st_register_client_req req = st_register_client_req_init_zero;
    st_register_client_resp resp = st_register_client_resp_init_zero;
    uint32_t resp_size = sizeof(st_register_client_resp);
    req.client_pid = pid;
    std::strncpy(req.client_name, pname, sizeof(req.client_name));
    LOG_PRINT_DEBUG("register client id req client_pid[%d], client_name[%s]", req.client_pid, req.client_name);
    ret = send_sync_to_daemon(LOCAL_REGISTRY_SERVICE_ID_METHOD_REGISTER_CLIENT, &req, st_register_client_req_fields,
                              st_register_client_req_size, &resp, &resp_size);
    if (ret != IPC_HV_SOA_RET_SUCCESS)
    {
        LOG_PRINT_ERROR("send_sync_to_daemon fail");
        return;
    }

    LOG_PRINT_INFO("register client id resp client_id[%u]", resp.client_id);
    char client_localaddr1[LOCAL_REGISTRY_SOCKET_LEN_MAX] = {};
    snprintf(client_localaddr1, sizeof(client_localaddr1), LOCAL_REGISTEY_SOCKET_FMT1, resp.client_id);
    m_client.client_id = resp.client_id;
    m_client.client_name = pname;
    m_client.client_localaddr1 = client_localaddr1;
    m_client.client_send_channel_id = 0;
    m_client.client_recv_channel_id = 0;
    m_client.send_msg_seqid = 0;
    m_client.send_msg_map.clear();
}

ipc_hv_soa_manager::~ipc_hv_soa_manager()
{
    int32_t ret = IPC_HV_SOA_RET_SUCCESS;
    ret = disconnect_to_daemon();
    if (ret != IPC_HV_SOA_RET_SUCCESS)
    {
        LOG_PRINT_ERROR("disconnect_to_daemon fail");
        return;
    }
}
