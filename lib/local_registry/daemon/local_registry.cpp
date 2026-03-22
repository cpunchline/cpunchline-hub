#include "local_registry.hpp"
#include "generator_autolib.h"

SINGLETON_IMPL(LocalRegistry);

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

LocalRegistry::LocalRegistry()
{
    hlog_set_handler(_ipc_hv_logger);
    hlog_set_level(LOG_LEVEL_INFO);
    hlog_set_format("HV-%L %s");

    unpack_setting_t m_unpack_setting;
    m_unpack_setting.mode = UNPACK_BY_LENGTH_FIELD;
    m_unpack_setting.package_max_length = LOCAL_REGISTRY_MSG_HEADER_SIZE + LOCAL_REGISTRY_MSG_SIZE_MAX;
    m_unpack_setting.body_offset = LOCAL_REGISTRY_MSG_HEADER_SIZE;
    m_unpack_setting.length_field_offset = LOCAL_REGISTRY_MSG_HEADER_SIZE - sizeof(uint32_t);
    m_unpack_setting.length_field_bytes = sizeof(uint32_t);
    m_unpack_setting.length_adjustment = 0;
    m_unpack_setting.length_field_coding = ENCODE_BY_LITTEL_ENDIAN;

    int listen_fd = m_server.createsocket(LOCAL_REGISTRY_SOCKET_PATH);
    if (listen_fd < 0)
    {
        LOG_PRINT_ERROR("createsocket fail");
        return;
    }
    LOG_PRINT_INFO("uds server listen on path[%s], listen_fd[%d]", LOCAL_REGISTRY_SOCKET_PATH, listen_fd);

    m_server.onConnection = LocalRegistry::onConnection;
    m_server.onMessage = LocalRegistry::onMessage;
    m_server.onWriteComplete = LocalRegistry::onWriteComplete;
    m_server.setMaxConnectionNum(UINT32_MAX);
    m_server.setLoadBalance(LB_LeastConnections);
    m_server.setThreadNum(0);
    m_server.setUnpack(&m_unpack_setting);
    LOG_PRINT_INFO("LocalRegistry start...");
    m_server.start(true);
    LOG_PRINT_INFO("LocalRegistry start completed");
}

LocalRegistry::~LocalRegistry()
{
    LOG_PRINT_INFO("LocalRegistry stop...");
    m_server.stop(true);
    LOG_PRINT_INFO("LocalRegistry stop completed");
}

int main()
{
    debug_backtrace_init(NULL);

    LOG_PRINT_INFO("Starting LocalRegistry daemon...");
    auto *registry = LocalRegistry::instance();
    (void)registry;

    while (1)
    {
        sleep(1);
    }
    LocalRegistry::exitInstance();
    return 0;
}
