#pragma once

#include "utility/utils.h"
#include "utility/debug_backtrace.h"
#include "utility/threadqueue.h"
#include "utility/timercluster.h"

typedef enum _demo_func_id_e
{
    E_DEMO_FUNC_ID_INVALID = 0,

    // function
    E_DEMO_FUNC_ID_EVENT_TEST,  // event
    E_DEMO_FUNC_ID_METHOD_TEST, // method

    // timer
    E_DEMO_FUNC_ID_TIMER_SINGLE_ECHO_PRINT, // single
    E_DEMO_FUNC_ID_TIMER_PERIOD_ECHO_PRINT, // period
} demo_func_id_e;

typedef struct _demo_workerqueue_data_t
{
    uint32_t func_id;
    uint32_t queue_data_len;
    uint8_t queue_data[4096];
} demo_workerqueue_data_t;

typedef struct _demo_userdata_t
{
    threadboundedqueue_t *workerqueue;
    timercluster_t *timercluster;
} demo_userdata_t;
