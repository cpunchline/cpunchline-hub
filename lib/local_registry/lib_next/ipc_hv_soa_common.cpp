#include "ipc_hv_soa_manager.hpp"

std::shared_ptr<ipc_hv_soa_client_sync_ctx> ipc_hv_soa_manager::start_sync_ctx(uint32_t index)
{
    auto manager = ipc_hv_soa_manager::instance();
    std::shared_ptr<ipc_hv_soa_client_sync_ctx> p_sync_ctx = manager->m_sync_ctx_pool.Borrow();
    if (nullptr == p_sync_ctx)
    {
        LOG_PRINT_ERROR("Borrow ipc_hv_soa_client_sync_ctx fail");
        return nullptr;
    }

    p_sync_ctx->sync_ctx_index = index;
    p_sync_ctx->service_id = 0;
    p_sync_ctx->service_type = LOCAL_SERVICE_STATUS_DEFAULT;
    p_sync_ctx->service_seqid = 0;
    p_sync_ctx->dest = UINT32_MAX;
    p_sync_ctx->ret = IPC_HV_SOA_RET_FAIL;
    p_sync_ctx->data_len = 0;
    std::memset(p_sync_ctx->data, 0, LOCAL_REGISTRY_MSG_SIZE_MAX);

    std::lock_guard<std::mutex> lock(manager->m_sync_ctx_map_mutex);
    manager->m_sync_ctx_map.insert({index, p_sync_ctx});

    return p_sync_ctx;
}

void ipc_hv_soa_manager::end_sync_ctx(uint32_t index, std::shared_ptr<ipc_hv_soa_client_sync_ctx> p_sync_ctx)
{
    auto manager = ipc_hv_soa_manager::instance();

    {
        std::lock_guard<std::mutex> lock(manager->m_sync_ctx_map_mutex);
        manager->m_sync_ctx_map.erase(index);
    }

    manager->m_sync_ctx_pool.Return(p_sync_ctx);
}

std::shared_ptr<ipc_hv_soa_client_sync_ctx> ipc_hv_soa_manager::find_sync_ctx_by_dest(uint32_t sync_ctx_type, uint32_t dest)
{
    auto manager = ipc_hv_soa_manager::instance();
    std::lock_guard<std::mutex> lock(manager->m_sync_ctx_map_mutex);
    for (auto iter = manager->m_sync_ctx_map.begin(); iter != manager->m_sync_ctx_map.end(); iter++)
    {
        auto p_sync_ctx = iter->second;
        if (p_sync_ctx->sync_ctx_type == sync_ctx_type && p_sync_ctx->dest == dest)
        {
            return p_sync_ctx;
        }
    }

    return nullptr;
}

std::shared_ptr<ipc_hv_soa_client_sync_ctx> ipc_hv_soa_manager::find_sync_ctx_by_service_id(uint32_t sync_ctx_type, uint32_t service_id)
{
    auto manager = ipc_hv_soa_manager::instance();
    std::lock_guard<std::mutex> lock(manager->m_sync_ctx_map_mutex);
    for (auto iter = manager->m_sync_ctx_map.begin(); iter != manager->m_sync_ctx_map.end(); iter++)
    {
        auto p_sync_ctx = iter->second;
        if (p_sync_ctx->sync_ctx_type == sync_ctx_type && p_sync_ctx->service_id == service_id)
        {
            return p_sync_ctx;
        }
    }

    return nullptr;
}