#include "ipc_hv_soa_inn.hpp"

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
                if (it->second.repeat != UINT32_MAX)
                {
                    it->second.repeat--;
                    if (it->second.repeat == 0)
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
    int32_t ret = IPC_HV_SOA_RET_SUCCESS;

    if (0 == repeat)
    {
        return IPC_HV_SOA_RET_ERR_ARG;
    }

    htimer_t *timer = nullptr;
    std::lock_guard<std::mutex> m_timers_map_lock(g_client->m_timers_map_mutex);
    if (g_client->m_timers_map.find(timer_id) != g_client->m_timers_map.end())
    {
        LOG_PRINT_ERROR("timer[%d] already exist", timer_id);
        return IPC_HV_SOA_RET_ERR_EXISTS;
    }

    timer = htimer_add(g_client->m_timer_loop, on_ipc_hv_soa_timer_cb, interval_ms, repeat);
    if (nullptr == timer)
    {
        ret = IPC_HV_SOA_RET_FAIL;
    }
    else
    {
        void *userdata = (void *)(uintptr_t)timer_id;
        hevent_set_userdata(timer, userdata);
        g_client->m_timers_map.insert({
            timer_id,
            {timer_id, repeat, interval_ms, timer_cb, timer, timer_data}
        });
        ret = IPC_HV_SOA_RET_SUCCESS;
    }

    return ret;
}

int32_t ipc_hv_soa_timer_reset(uint32_t timer_id, uint32_t interval_ms)
{
    int32_t ret = IPC_HV_SOA_RET_SUCCESS;

    std::lock_guard<std::mutex> m_timers_map_lock(g_client->m_timers_map_mutex);
    auto it = g_client->m_timers_map.find(timer_id);
    if (it == g_client->m_timers_map.end())
    {
        LOG_PRINT_ERROR("timer[%d] not exist", timer_id);
        ret = IPC_HV_SOA_RET_ERR_NOT_EXISTS;
    }
    else
    {
        htimer_reset(it->second.timer, interval_ms);
        ret = IPC_HV_SOA_RET_SUCCESS;
    }

    return ret;
}

int32_t ipc_hv_soa_timer_delete(uint32_t timer_id)
{
    int32_t ret = IPC_HV_SOA_RET_SUCCESS;

    std::lock_guard<std::mutex> m_timers_map_lock(g_client->m_timers_map_mutex);
    auto it = g_client->m_timers_map.find(timer_id);
    if (it == g_client->m_timers_map.end())
    {
        LOG_PRINT_ERROR("timer[%d] not exist", timer_id);
        ret = IPC_HV_SOA_RET_ERR_NOT_EXISTS;
    }
    else
    {
        htimer_del(it->second.timer);
        g_client->m_timers_map.erase(it);
        ret = IPC_HV_SOA_RET_SUCCESS;
    }

    return ret;
}

bool ipc_hv_soa_timer_exist(uint32_t timer_id)
{
    bool exists = false;
    std::lock_guard<std::mutex> m_timers_map_lock(g_client->m_timers_map_mutex);
    auto it = g_client->m_timers_map.find(timer_id);
    if (it == g_client->m_timers_map.end())
    {
        exists = false;
    }
    else
    {
        exists = true;
    }

    return exists;
}