#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <assert.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <pthread.h>
#include <unistd.h>

#include "zmq.h"
#include "utility/utils.h"
#include "ipc_zmq.h"

#define IPC_ZMQ_S_TO_MS  (1000)
#define IPC_ZMQ_MS_TO_US (1000)
#define IPC_ZMQ_MS_TO_NS (1000000)

#define IPC_ZMQ_STREAM_RESULT (INT32_MAX)

#define D_IPC_ZMQ_BROADCAST_TOPIC     1
#define D_IPC_ZMQ_BROADCAST_TOPIC_STR "1"

#define D_IPC_ZMQ_SOCKET_ADDRESS_LEN_MAX (256)

// wait timeout
#define D_IPC_ZMQ_WAIT_CONTINUE (-1)
#define D_IPC_ZMQ_WAIT_TIMEOUT  (1500)
// sync wait timeout by self set

#define D_INPROC_ZMQ_ASYNC_QUEUE_ADDRESS     "inproc://zmq-%04x-async-msg-queue"
#define D_INPROC_ZMQ_BACK_REQ_RESP_ADDRESS   "inproc://zmq-%04x-back-response"
#define D_IPC_ZMQ_REQ_RESP_ADDRESS           "ipc://%s/zmq-%04x-req-resp.ipc"
#define D_IPC_ZMQ_BROADCAST_FRONTEND_ADDRESS "ipc://%s/zmq-broadcast-frontend.ipc"
#define D_IPC_ZMQ_BROADCAST_BACKEND_ADDRESS  "ipc://%s/zmq-broadcast-backend.ipc"

typedef struct _ipc_zmq_manager_info_t
{
    ipc_zmq_register_info_t reg_info;
    void *ctx;

    // broadcast
    void *pub; // pub
    void *sub; // sub
    pthread_mutex_t mutex_pub;
    void *broadcast_thread;
    // each send by pub on lock

    // broadcast proxy
    void *frontend; // xsub
    void *backend;  // xpub
    void *broadcast_proxy_thread;

    // async
    void *dealer; // dealer
    void *async_thread;
    // each send by create dealer
    // another async thread wait the dealer
    // then another async thread send by create req

    // notify/sync/stream
    void *rep; // rep
    void *rep_thread;
    // each send by create req

    bool is_run;
} ipc_zmq_manager_info_t;

void get_zmq_lib_version();
int32_t ipc_zmq_poll(void *socket, long timeout);
void error_receive_handle(void *rep);
void ipc_zmq_msg_free(void *data_, void *hint_);

#ifdef __cplusplus
}
#endif
