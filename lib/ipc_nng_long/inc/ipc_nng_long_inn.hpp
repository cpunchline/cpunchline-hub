#pragma once

#include <errno.h>
#include <linux/limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <inttypes.h>
#include <pthread.h>

#include <list>
#include <unordered_map>
#include <deque>
#include <algorithm>

#include "nng/nng.h"
#include "utility/utils.h"
#include "ipc_nng_long.h"

// #define D_IPC_NNG_LONG_EACH_SLEEP_100MS // 针对同一个模块的IPC消息, 间隔100ms

#define D_IPC_NNG_LONG_SOCKET_ADDRESS_LEN_MAX (256)
#ifdef NNG_USE_TCP
#define D_INPROC_NNG_LONG_ASYNC_MSG_QUEUE_ADDRESS "inproc://nng-long-%04x-async-msg-queue"
#define D_IPC_NNG_LONG_REP_ADDRESS                "tcp://127.0.0.1:23327"
#define D_IPC_NNG_LONG_BROADCAST_ADDRESS          "tcp://127.0.0.1:23328"
#define D_IPC_NNG_LONG_PAIR_ADDRESS               "tcp://127.0.0.1:%x"
#else
#define D_INPROC_NNG_LONG_ASYNC_MSG_QUEUE_ADDRESS "inproc://nng-long-%04x-async-msg-queue"
#define D_IPC_NNG_LONG_REP_ADDRESS                "ipc://%s/nng-long-proxy-rep.ipc"
#define D_IPC_NNG_LONG_BROADCAST_ADDRESS          "ipc://%s/nng-long-proxy-broadcast.ipc"
#define D_IPC_NNG_LONG_PAIR_ADDRESS               "ipc://%s/nng-long-%x-pair.ipc"
#endif
#define D_IPC_BROADCAST_PROXY_THREAD_NAME "ipc_broadcast_thread"
#define D_IPC_ASYNC_THREAD_NAME           "ipc_async_thread"

#define D_IPC_NNG_LONG_BROADCAST_TOPIC     1
#define D_IPC_NNG_LONG_BROADCAST_TOPIC_STR "1"

// wait timeout
#define D_IPC_NNG_LONG_WAIT_CONTINUE      (-1)
#define D_IPC_NNG_LONG_WAIT_TIMEOUT       (1500)
#define D_IPC_NNG_LONG_SEND_BLOCK_TIMEOUT (50)
// sync wait timeout by self set

#define D_IPC_NNG_LONG_PROTOCOL_SYNC      1ull
#define D_IPC_NNG_LONG_PROTOCOL_ASYNC     2ull
#define D_IPC_NNG_LONG_PROTOCOL_STREAM    3ull
#define D_IPC_NNG_LONG_PROTOCOL_BROADCAST 4ull

#define D_IPC_NNG_LONG_MSG_TAG            (0x5AFE0002U)
#define D_IPC_NNG_LONG_MESSAGE_PAIR_COUNT (8)
#define D_IPC_NNG_LONG_WORKS_COUNT        (3)

#define D_IPC_NNG_LONG_GENERATION_EVENT_ID(PROTOCOL, SRC_ID, DEST_ID, MSG_ID) \
    (uint64_t)((PROTOCOL << 60) + ((uint64_t)SRC_ID << 48) + (((uint64_t)DEST_ID << 32)) + ((uint64_t)MSG_ID << 16) + get_index())

#define D_IPC_NNG_LONG_CHECK_IPC_INIT()                              \
    do                                                               \
    {                                                                \
        if (0 != ipc_init_ret)                                       \
        {                                                            \
            LOG_PRINT_ERROR("ipc init fail, ret[%d]", ipc_init_ret); \
            return -1;                                               \
        }                                                            \
    } while (0)

#define D_IPC_NNG_LONG_CHECK_MODULE_ID_INVALID(module_id) (module_id <= E_IPC_NNG_LONG_MODULE_ID_DAEMON || module_id >= E_IPC_NNG_LONG_MODULE_ID_MAX)

#if D_IPC_NNG_LONG_EACH_SLEEP_100MS
#define D_IPC_NNG_LONG_CHECK_DEST_SAME(last_id, cur_id) \
    do                                                  \
    {                                                   \
        if (last_id == cur_id)                          \
        {                                               \
            usleep(100);                                \
        }                                               \
        last_id = cur_id;                               \
    } while (0)
#endif

#define D_IPC_NNG_LONG_NESTED_CALL(dest, msg_id)                                                      \
    do                                                                                                \
    {                                                                                                 \
        char thread_name[16] = {};                                                                    \
        if (0 == prctl(PR_GET_NAME, thread_name, 0, 0, 0) && 0 == strcmp(thread_name, "nng:task"))    \
        {                                                                                             \
            LOG_PRINT_WARN("thread nng:task should not call ipc msg_id[%u] of [0x%x]", msg_id, dest); \
        }                                                                                             \
    } while (0)

typedef struct _ipc_nng_long_message_pair_t
{
    bool is_used;
    uint64_t event_id;
    nng_mtx *mtx;
    nng_cv *cv;
    bool cond_true;
    nng_aio *timer_aio;
    nng_time expire;
    size_t out_data_size;
    void *out_data;
    int ret;
} ipc_nng_long_message_pair_t;

typedef struct _ipc_nng_long_map_item_t
{
    ipc_nng_long_register_info_t reg_info;
#ifdef D_IPC_NNG_LONG_EACH_SLEEP_100MS
    uint32_t dest;
#endif
    ipc_nng_long_message_pair_t msg_pair[D_IPC_NNG_LONG_MESSAGE_PAIR_COUNT];
} ipc_nng_long_map_item_t;

typedef struct _ipc_nng_long_work_t
{
    enum
    {
        INIT,
        RECV,
        WAIT,
        SEND
    } state;
    nng_aio *aio;
    nng_socket sock;
    nng_msg *msg;
} ipc_nng_long_work_t;

typedef struct _ipc_nng_long_client_t
{
    uint32_t module_id;
    nng_socket sock_sub;
    nng_socket sock_req;
    nng_socket sock_pair;
    nng_aio *sub_aio;
    ipc_nng_long_work_t *works[D_IPC_NNG_LONG_WORKS_COUNT];
} ipc_nng_long_client_t;

typedef struct _ipc_nng_long_service_t
{
    nng_socket sock_pub;
    nng_socket sock_rep;
    const char *name;
    nng_aio *rep_recv_aio;
} ipc_nng_long_service_t;

typedef struct _ipc_nng_long_expire_item_t
{
    nng_socket sock_pair;
    nng_aio *time_aio;
    nng_msg *msg;
} ipc_nng_long_expire_item_t;

typedef struct _ipc_nng_long_reg_item_t
{
    uint32_t tag;
    bool closed;
    uint32_t module_id;
    nng_socket sock_pair;
    char address[D_IPC_NNG_LONG_SOCKET_ADDRESS_LEN_MAX];
    nng_aio *recv_aio;
    nng_aio *unreg_aio;
    std::list<uint32_t> list;
    nng_mtx *mtx;
} ipc_nng_long_reg_item_t;

typedef struct _ipc_nng_long_sock_aio_t
{
    nng_aio *aio;
} ipc_nng_long_sock_aio_t;

typedef struct _ipc_nng_long_mult_dest_t
{
    uint32_t num;
    uint32_t module_array[E_IPC_NNG_LONG_MODULE_ID_MAX];
} ipc_nng_long_mult_dest_t;

void get_nng_lib_version(void);
uint16_t get_index(void);
bool get_process_name(char *process_name);

int32_t ipc_init_dial(nng_socket sock, const char *url);
int32_t sock_aio_alloc_sendmsg(nng_socket sock, nng_msg *msg);
int32_t sock_aio_alloc_send(nng_socket sock, const void *data, size_t len);
