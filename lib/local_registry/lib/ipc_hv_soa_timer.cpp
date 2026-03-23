#include "ipc_hv_soa_common.hpp"

extern std::atomic_bool g_init_flag;
extern std::shared_ptr<ipc_hv_soa_client> g_client;

static void on_ipc_hv_soa_timer_cb(htimer_t *timer)
{
    uint32_t timer_id = (uint32_t)(uintptr_t)hevent_userdata(timer);
    {
        // update timer info
        PF_IPC_HV_SOA_TIMER_CB timer_cb = nullptr;
        void *timer_data = nullptr;
        {
            std::lock_guard<std::mutex> m_timers_map_lock(g_client->m_timers_map_mutex);
            auto it = g_client->m_timers_map.find(timer_id);
            if (it == g_client->m_timers_map.end())
            {
            }
            else
            {
                if (it->second.repeat != IPC_HV_SOA_TIMER_REPEAT_CYCLE)
                {
                    it->second.repeat--;
                    if (it->second.repeat == IPC_HV_SOA_TIMER_REPEAT_INVALID)
                    {
                        g_client->m_timers_map.erase(it->second.id);
                    }
                }
                timer_cb = it->second.cb;
                timer_data = it->second.data;
            }
        }

        if (timer_cb)
        {
            timer_cb(timer_id, timer_data);
        }
    }
}

int32_t ipc_hv_soa_timer_create(uint32_t timer_id, uint32_t repeat, uint32_t interval_ms, PF_IPC_HV_SOA_TIMER_CB timer_cb, void *timer_data)
{
    if (IPC_HV_SOA_TIMER_REPEAT_INVALID == repeat)
    {
        return IPC_HV_SOA_RET_ERR_ARG;
    }

    htimer_t *timer = nullptr;
    std::lock_guard<std::mutex> m_timers_map_lock(g_client->m_timers_map_mutex);
    if (g_client->m_timers_map.find(timer_id) != g_client->m_timers_map.end())
    {
        LOG_PRINT_ERROR("timer[%u] already exist", timer_id);
        return IPC_HV_SOA_RET_ERR_EXISTS;
    }

    timer = htimer_add(g_client->m_timer_loop, on_ipc_hv_soa_timer_cb, interval_ms, repeat);
    if (nullptr == timer)
    {
        return IPC_HV_SOA_RET_FAIL;
    }

    void *userdata = (void *)(uintptr_t)timer_id;
    hevent_set_userdata(timer, userdata);

    g_client->m_timers_map.insert({
        timer_id,
        {timer_id, repeat, interval_ms, timer_cb, timer, timer_data}
    });

    return IPC_HV_SOA_RET_SUCCESS;
}

int32_t ipc_hv_soa_timer_reset(uint32_t timer_id, uint32_t interval_ms)
{
    std::lock_guard<std::mutex> m_timers_map_lock(g_client->m_timers_map_mutex);
    auto it = g_client->m_timers_map.find(timer_id);
    if (it == g_client->m_timers_map.end())
    {
        LOG_PRINT_ERROR("timer[%u] not exist", timer_id);
        return IPC_HV_SOA_RET_ERR_NOT_EXISTS;
    }

    htimer_reset(it->second.timer, interval_ms);
    return IPC_HV_SOA_RET_SUCCESS;
}

int32_t ipc_hv_soa_timer_delete(uint32_t timer_id)
{
    std::lock_guard<std::mutex> m_timers_map_lock(g_client->m_timers_map_mutex);
    auto it = g_client->m_timers_map.find(timer_id);
    if (it == g_client->m_timers_map.end())
    {
        LOG_PRINT_ERROR("timer[%u] not exist", timer_id);
        return IPC_HV_SOA_RET_ERR_NOT_EXISTS;
    }

    htimer_del(it->second.timer);
    g_client->m_timers_map.erase(it);
    return IPC_HV_SOA_RET_SUCCESS;
}

bool ipc_hv_soa_timer_exist(uint32_t timer_id)
{
    std::lock_guard<std::mutex> m_timers_map_lock(g_client->m_timers_map_mutex);
    auto it = g_client->m_timers_map.find(timer_id);
    return (it != g_client->m_timers_map.end());
}
