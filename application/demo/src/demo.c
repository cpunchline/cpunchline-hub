#include "demo.h"

static void _demo_notify_timer_cb(timerid_t id, void *userdata)
{
    demo_userdata_t *p_userdata = (demo_userdata_t *)userdata;
    demo_workerqueue_data_t workerqueue_data = {};
    workerqueue_data.func_id = (uint32_t)id;
    int32_t queue_ret = threadboundedqueue_push_nonblock(p_userdata->workerqueue, &workerqueue_data, sizeof(workerqueue_data));
    if (THREADQUEUE_RET_SUCCESS != queue_ret)
    {
        LOG_PRINT_ERROR("threadboundedqueue_push_nonblock failed, queue_ret[%d]", queue_ret);
    }
}

static void handler_single_timer_echo_print(demo_userdata_t *userdata, const void *queue_data, size_t queue_data_len)
{
    (void)userdata;
    (void)queue_data;
    (void)queue_data_len;
    LOG_PRINT_INFO("single timeout");
    if (!timercluster_timer_add(userdata->timercluster, E_DEMO_FUNC_ID_TIMER_PERIOD_ECHO_PRINT, TIMER_TYPE_CYCLE, 1000, _demo_notify_timer_cb, userdata))
    {
        return;
    }
}

static void handler_period_timer_echo_print(demo_userdata_t *userdata, const void *queue_data, size_t queue_data_len)
{
    static size_t count = 0;
    (void)userdata;
    (void)queue_data;
    (void)queue_data_len;
    if (count++ >= 5)
    {
        demo_workerqueue_data_t workerqueue_data = {};
        workerqueue_data.func_id = E_DEMO_FUNC_ID_METHOD_TEST;
        memcpy(&workerqueue_data.queue_data, &count, sizeof(count));
        workerqueue_data.queue_data_len = sizeof(count);
        int32_t queue_ret = threadboundedqueue_push_nonblock(userdata->workerqueue, &workerqueue_data, sizeof(workerqueue_data));
        if (THREADQUEUE_RET_SUCCESS != queue_ret)
        {
            LOG_PRINT_ERROR("threadboundedqueue_push_nonblock failed, queue_ret[%d]", queue_ret);
        }
        timercluster_timer_del(userdata->timercluster, E_DEMO_FUNC_ID_TIMER_PERIOD_ECHO_PRINT);
    }
    else
    {
        demo_workerqueue_data_t workerqueue_data = {};
        workerqueue_data.func_id = E_DEMO_FUNC_ID_EVENT_TEST;
        memcpy(&workerqueue_data.queue_data, &count, sizeof(count));
        workerqueue_data.queue_data_len = sizeof(count);
        int32_t queue_ret = threadboundedqueue_push_nonblock(userdata->workerqueue, &workerqueue_data, sizeof(workerqueue_data));
        if (THREADQUEUE_RET_SUCCESS != queue_ret)
        {
            LOG_PRINT_ERROR("threadboundedqueue_push_nonblock failed, queue_ret[%d]", queue_ret);
        }
    }

    LOG_PRINT_INFO("period timeout[%zd]", count);
}

static void handler_event_test(demo_userdata_t *userdata, const void *queue_data, size_t queue_data_len)
{
    (void)userdata;
    (void)queue_data_len;
    LOG_PRINT_INFO("event test[%d]", *(int32_t *)queue_data);
}

static void handler_method_test(demo_userdata_t *userdata, const void *queue_data, size_t queue_data_len)
{
    (void)userdata;
    (void)queue_data_len;
    LOG_PRINT_INFO("method test[%d]", *(int32_t *)queue_data);
}

static void _demo_handler(uint32_t func_id, demo_userdata_t *userdata, const void *queue_data, size_t queue_data_len)
{
    static const struct
    {
        uint32_t func_id; // see demo_func_id_e
        void (*handler)(demo_userdata_t *userdata, const void *queue_data, size_t queue_data_len);
    } handlers_table[] = {
        {E_DEMO_FUNC_ID_INVALID,                 NULL                           },
        {E_DEMO_FUNC_ID_EVENT_TEST,              handler_event_test             },
        {E_DEMO_FUNC_ID_METHOD_TEST,             handler_method_test            },
        {E_DEMO_FUNC_ID_TIMER_SINGLE_ECHO_PRINT, handler_single_timer_echo_print},
        {E_DEMO_FUNC_ID_TIMER_PERIOD_ECHO_PRINT, handler_period_timer_echo_print},
    };

    for (size_t i = 0; i < UTIL_ARRAY_SIZE(handlers_table); ++i)
    {
        if ((handlers_table[i].func_id == func_id) && NULL != handlers_table[i].handler)
        {
            handlers_table[i].handler(userdata, queue_data, queue_data_len);
            break;
        }
    }
}

int main(void)
{
    static demo_userdata_t demo_userdata = {}; // only used in main thread

    bool init_flag = false;
    debug_backtrace_init(NULL);

    do
    {
        demo_userdata.timercluster = timercluster_init();
        if (NULL == demo_userdata.timercluster)
        {
            break;
        }

        demo_userdata.workerqueue = threadboundedqueue_create(16, sizeof(demo_workerqueue_data_t));
        if (NULL == demo_userdata.workerqueue)
        {
            break;
        }

        if (!timercluster_timer_add(demo_userdata.timercluster, E_DEMO_FUNC_ID_TIMER_SINGLE_ECHO_PRINT, TIMER_TYPE_SINGLE, 1000, _demo_notify_timer_cb, &demo_userdata))
        {
            break;
        }

        init_flag = true;
    } while (0);

    int32_t queue_ret = THREADQUEUE_RET_SUCCESS;
    demo_workerqueue_data_t workerqueue_data = {};
    size_t queue_data_len = 0;

    while (init_flag)
    {
        memset(&workerqueue_data, 0x00, sizeof(workerqueue_data));
        queue_data_len = sizeof(workerqueue_data);
        queue_ret = threadboundedqueue_pop_block(demo_userdata.workerqueue, &workerqueue_data, &queue_data_len, 0);
        if (THREADQUEUE_RET_SUCCESS == queue_ret)
        {
            _demo_handler(workerqueue_data.func_id, &demo_userdata, workerqueue_data.queue_data, queue_data_len);
        }
    }

    if (demo_userdata.timercluster)
    {
        timercluster_destroy(demo_userdata.timercluster);
    }

    if (demo_userdata.workerqueue)
    {
        threadboundedqueue_stop(demo_userdata.workerqueue);
        threadboundedqueue_uninit(&demo_userdata.workerqueue);
    }

    return EXIT_SUCCESS;
}