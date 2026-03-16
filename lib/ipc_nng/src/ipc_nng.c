#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/poll.h>
#include <pthread.h>

#include "nng/nng.h"
#include "utility/utils.h"
#include "ipc_nng.h"

#define IPC_NNG_S_TO_MS  (1000)
#define IPC_NNG_MS_TO_US (1000)

#define D_IPC_NNG_SOCKET_ADDRESS_LEN_MAX (256)

#define D_IPC_NNG_BROADCAST_TOPIC     1
#define D_IPC_NNG_BROADCAST_TOPIC_STR "1"

// wait timeout
#define D_IPC_NNG_WAIT_CONTINUE (-1)
#define D_IPC_NNG_WAIT_TIMEOUT  (1500)
// sync wait timeout by self set

#define D_IPC_NNG_STREAM_RESULT (0x5AFE0002U)

#define D_INPROC_NNG_ASYNC_MSG_QUEUE_ADDRESS "inproc://nng-%04x-async-msg-queue"
#define D_IPC_NNG_REQ_RESP_ADDRESS           "ipc://%s/nng-%04x-req-resp.ipc"
#define D_IPC_NNG_BROADCAST_FRONTEND_ADDRESS "ipc://%s/nng-broadcast-frontend.ipc"
#define D_IPC_NNG_BROADCAST_BACKEND_ADDRESS  "ipc://%s/nng-broadcast-backend.ipc"

#define D_IPC_BROADCAST_PROXY_THREAD_NAME "ipc_broadcast_thread"
#define D_IPC_ASYNC_THREAD_NAME           "ipc_async_thread"

typedef struct _ipc_nng_manager_info_t ipc_nng_manager_info_t;

typedef struct _ipc_nng_broadcast_proxy_t
{
    nng_socket frontend;
    nng_socket backend;
    nng_listener front_ls;
    nng_listener back_ls;
    nng_thread *broadcast_proxy_thread;
} ipc_nng_broadcast_proxy_t;

typedef struct _ipc_nng_rep_worker_t
{
    nng_aio *aio;
    nng_ctx ctx;
    ipc_nng_manager_info_t *pinfo;
} ipc_nng_rep_worker_t;

typedef struct _ipc_nng_async_worker_t
{
    nng_socket push;
    nng_socket pull;
    nng_thread *async_thread;
    nng_aio *aio;
} ipc_nng_async_worker_t;

typedef struct _ipc_nng_rep_workers_t
{
    nng_socket rep;
    ipc_nng_rep_worker_t worker_array[E_IPC_NNG_MODULE_ID_MAX]; // load balance
} ipc_nng_rep_workers_t;

typedef struct _ipc_nng_broadcast_worker_t
{
    pthread_mutex_t mutex_pub;
    nng_socket pub;
    nng_socket sub;
    nng_aio *aio;
    ipc_nng_broadcast_proxy_t broadcast_proxy;
} ipc_nng_broadcast_worker_t;

struct _ipc_nng_manager_info_t
{
    ipc_nng_register_info_t reg_info;
    ipc_nng_rep_workers_t rep_workers;
    ipc_nng_async_worker_t async_worker;
    ipc_nng_broadcast_worker_t broadcast_worker;
    bool is_run;
};

static ipc_nng_manager_info_t *g_manager_info[E_IPC_NNG_MODULE_ID_MAX] = {NULL};
static uint32_t g_module_id = E_IPC_NNG_MODULE_ID_INVALID;
static const uint8_t topic_buff[] = {D_IPC_NNG_BROADCAST_TOPIC_STR};
static const char *socket_prefix = NULL;

static void get_nng_lib_version(void)
{
    LOG_PRINT_INFO("NNG Version[%s]", nng_version());
}

// nng_dial(url, 0)         同步, 不重试;
// nng_dial(url, NONBLOCK)  异步, 后台自动重试;
// nng_dialer_start()       后台自动重试, 自定义重试调度器;
/*
自动重连机制详解
参数	描述
NNG_OPT_RECONNMINT  第一次重连前等待的最短时间(毫秒)
NNG_OPT_RECONNMAXT  重连尝试的最大间隔时间(毫秒)
行为                 使用指数退避算法,每次失败后重连间隔逐渐增加,直到达到最大值
默认情况下,NNG 的重连策略是智能的:
初始间隔很短(比如 100ms)
每次失败后间隔翻倍(指数退避)
最多不超过 RECONNMAXT 设定的时间
*/
static int ipc_nng_dial_retry(nng_socket sid, const char *addr)
{
    int ret = -1;
    int retry_times = 10;

    do
    {
        ret = nng_dial(sid, addr, NULL, 0);
        if (0 == ret)
        {
            break;
        }
        LOG_PRINT_ERROR("nng_dial fail, retry[%d], nng_error[%d]!", retry_times, ret);
        usleep(100 * 1000);
    } while (retry_times-- > 0);

    if (retry_times > 0)
    {
        ret = 0;
    }
    else
    {
        ret = -1;
    }

    return ret;
}

static void server_rep_cb(void *arg)
{
    ipc_nng_rep_worker_t *p_worker = (ipc_nng_rep_worker_t *)arg;
    ipc_nng_manager_info_t *pinfo = p_worker->pinfo;

    int ret = -1;
    nng_err nret = NNG_OK;
    nng_msg *req_msg = NULL;
    nng_msg *resp_msg = NULL;
    ipc_nng_msg_baseinfo_t msg_baseinfo = {};

    nret = nng_aio_result(p_worker->aio);
    if (NNG_OK != nret)
    {
        LOG_PRINT_ERROR("nng_aio_result fail, nng_error[%s]!", nng_strerror(nret));
    }
    else
    {
        req_msg = nng_aio_get_msg(p_worker->aio);
        LOG_PRINT_INFO("recv rep msg_len[%zd]", nng_msg_len(req_msg));
        if (nng_msg_len(req_msg) >= sizeof(ipc_nng_msg_baseinfo_t))
        {
            memcpy(&msg_baseinfo, nng_msg_body(req_msg), sizeof(msg_baseinfo));
            nng_msg_trim(req_msg, sizeof(msg_baseinfo));
            if (NULL != pinfo->reg_info.response_handler &&
                (E_IPC_NNG_MSG_TYPE_SYNC == msg_baseinfo.msg_type || E_IPC_NNG_MSG_TYPE_ASYNC == msg_baseinfo.msg_type))
            {
                nng_msg_alloc(&resp_msg, msg_baseinfo.recv_len);
                pinfo->reg_info.response_handler(&msg_baseinfo, nng_msg_body(req_msg), nng_msg_body(resp_msg));
                ret = nng_ctx_sendmsg(p_worker->ctx, resp_msg, 0);
                if (0 != ret)
                {
                    LOG_PRINT_ERROR("nng_sendmsg fail, nng_error(%d)!", ret);
                    nng_msg_free(resp_msg);
                }
                // nng_msg_free(resp_msg);
            }
            else if (NULL != pinfo->reg_info.notify_handler && E_IPC_NNG_MSG_TYPE_NOTIFY == msg_baseinfo.msg_type)
            {
                nng_msg_alloc(&resp_msg, 0);
                pinfo->reg_info.notify_handler(&msg_baseinfo, nng_msg_body(req_msg));
                ret = nng_ctx_sendmsg(p_worker->ctx, resp_msg, 0);
                if (0 != ret)
                {
                    LOG_PRINT_ERROR("nng_sendmsg fail, nng_error(%d)!", ret);
                    nng_msg_free(resp_msg);
                }
                // nng_msg_free(resp_msg);
            }
            else if (NULL != pinfo->reg_info.stream_handler && E_IPC_NNG_MSG_TYPE_STREAM == msg_baseinfo.msg_type)
            {
                int32_t stream_ret = D_IPC_NNG_STREAM_RESULT;
                nng_msg_alloc(&resp_msg, msg_baseinfo.recv_len);
                nng_msg_append(resp_msg, &stream_ret, sizeof(stream_ret));
                ret = nng_ctx_sendmsg(p_worker->ctx, resp_msg, 0);
                if (0 != ret)
                {
                    LOG_PRINT_ERROR("nng_sendmsg fail, nng_error(%d)!", ret);
                    nng_msg_free(resp_msg);
                }
                // nng_msg_free(resp_msg);
                pinfo->reg_info.stream_handler(&msg_baseinfo, nng_msg_body(req_msg));
            }
            else
            {
                nng_msg_alloc(&resp_msg, 0);
                ret = nng_ctx_sendmsg(p_worker->ctx, resp_msg, 0);
                if (0 != ret)
                {
                    LOG_PRINT_ERROR("nng_sendmsg fail, nng_error(%d)!", ret);
                    nng_msg_free(resp_msg);
                }
                // nng_msg_free(resp_msg);
            }
        }
        else
        {
            LOG_PRINT_ERROR("invalid msg len[%zd]", nng_msg_len(req_msg));
        }
        nng_msg_free(req_msg);
    }
    nng_ctx_recv(p_worker->ctx, p_worker->aio);
}

static void server_broadcast_cb(void *arg)
{
    ipc_nng_manager_info_t *pinfo = (ipc_nng_manager_info_t *)arg;
    nng_err nret = NNG_OK;
    char topic[sizeof(topic_buff)] = {};
    ipc_nng_broadcast_msg_baseinfo_t broadcast_baseinfo = {};
    nng_msg *msg = NULL;
    nret = nng_aio_result(pinfo->broadcast_worker.aio);
    if (NNG_OK != nret)
    {
        LOG_PRINT_ERROR("nng_aio_result fail, nng_error(%s)!", nng_strerror(nret));
    }
    else
    {
        if (NULL != pinfo->reg_info.broadcast_handler)
        {
            msg = nng_aio_get_msg(pinfo->broadcast_worker.aio);
            LOG_PRINT_INFO("recv broadcast msg_len[%zd]", nng_msg_len(msg));
            if (nng_msg_len(msg) >= sizeof(topic_buff) + sizeof(ipc_nng_broadcast_msg_baseinfo_t))
            {
                memcpy(&topic, nng_msg_body(msg), sizeof(topic_buff));
                nng_msg_trim(msg, sizeof(topic_buff));
                if (0 == strncmp(topic, D_IPC_NNG_BROADCAST_TOPIC_STR, strlen(D_IPC_NNG_BROADCAST_TOPIC_STR)))
                {
                    if (nng_msg_len(msg) >= sizeof(ipc_nng_broadcast_msg_baseinfo_t))
                    {
                        memcpy(&broadcast_baseinfo, nng_msg_body(msg), sizeof(broadcast_baseinfo));
                        nng_msg_trim(msg, sizeof(broadcast_baseinfo));
                        pinfo->reg_info.broadcast_handler(&broadcast_baseinfo, nng_msg_body(msg));
                    }
                    else
                    {
                        LOG_PRINT_ERROR("invalid msg len[%zd]", nng_msg_len(msg));
                    }
                }
                else
                {
                    LOG_PRINT_ERROR("invalid topic[%s]", topic);
                }
            }
            else
            {
                LOG_PRINT_ERROR("invalid msg len[%zd]", nng_msg_len(msg));
            }
            nng_msg_free(msg);
        }
    }
    nng_socket_recv(pinfo->broadcast_worker.sub, pinfo->broadcast_worker.aio);
}

static void ipc_nng_broadcast_proxy_handler(void *arg)
{
    ipc_nng_broadcast_proxy_t *broadcast_proxy = arg;
    nng_err nret = NNG_OK;

    LOG_PRINT_INFO("broadcast_proxy run!");
    nret = nng_device(broadcast_proxy->frontend, broadcast_proxy->backend);
    if (NNG_OK != nret)
    {
        LOG_PRINT_ERROR("nng_device fail, nng_error(%s)!", nng_strerror(nret));
        nng_listener_close(broadcast_proxy->front_ls);
        nng_listener_close(broadcast_proxy->back_ls);
        nng_socket_close(broadcast_proxy->frontend);
        nng_socket_close(broadcast_proxy->backend);
        return;
    }

    nng_listener_close(broadcast_proxy->front_ls);
    nng_listener_close(broadcast_proxy->back_ls);
    nng_socket_close(broadcast_proxy->frontend);
    nng_socket_close(broadcast_proxy->backend);
}

static void server_async_cb(void *arg)
{
    ipc_nng_manager_info_t *pinfo = (ipc_nng_manager_info_t *)arg;
    nng_err nret = NNG_OK;
    int ret = -1;
    nng_msg *async_req_msg = NULL;
    nng_msg *async_resp_msg = NULL;
    ipc_nng_msg_baseinfo_t msg_baseinfo = {};

    while (pinfo->is_run)
    {
        nng_socket_recv(pinfo->async_worker.pull, pinfo->async_worker.aio);
        nng_aio_wait(pinfo->async_worker.aio);
        nret = nng_aio_result(pinfo->async_worker.aio);
        if (NNG_OK != nret)
        {
            LOG_PRINT_ERROR("nng_aio_result fail, nng_error(%s)!", nng_strerror(nret));
        }
        else
        {
            async_req_msg = nng_aio_get_msg(pinfo->async_worker.aio);
            LOG_PRINT_INFO("recv async req msg_len[%zd]", nng_msg_len(async_req_msg));
            if (nng_msg_len(async_req_msg) >= sizeof(msg_baseinfo))
            {
                memcpy(&msg_baseinfo, nng_msg_body(async_req_msg), sizeof(msg_baseinfo));
            }
            else
            {
                LOG_PRINT_ERROR("invalid msg len[%zd]", nng_msg_len(async_req_msg));
                nng_msg_free(async_req_msg);
                continue;
            }

            nng_socket async = NNG_SOCKET_INITIALIZER;
            char address_req[D_IPC_NNG_SOCKET_ADDRESS_LEN_MAX] = {};
            snprintf(address_req, sizeof(address_req), D_IPC_NNG_REQ_RESP_ADDRESS, socket_prefix, msg_baseinfo.dest_id);

            ret = nng_req0_open(&async);
            if (0 != ret)
            {
                LOG_PRINT_ERROR("nng_req0_open fail, nng_error(%d)!", ret);
                nng_msg_free(async_req_msg);
                continue;
            }

#if 0
            // req identity
            uint16_t proto_id = -1;
            uint16_t peer_id = -1;
            const char *proto_name = NULL;
            const char *peer_name = NULL;
            nng_socket_proto_id(async, &proto_id);
            nng_socket_peer_id(async, &peer_id);
            nng_socket_proto_name(async, &proto_name);
            nng_socket_peer_name(async, &peer_name);

            LOG_PRINT_DEBUG("async proto_id[%d](%d); proto_name[%u](%u), proto_name[%s](%s), peer_name[%s](%s)",
                          proto_id, 0x30,
                          peer_id, 0x31,
                          proto_name, "req",
                          peer_name, "rep");
#endif

            nng_socket_set_ms(async, NNG_OPT_REQ_RESENDTIME, 0); // no retry; NNG_ECONNRESET
            nng_socket_set_ms(async, NNG_OPT_SENDTIMEO, D_IPC_NNG_WAIT_TIMEOUT);
            nng_socket_set_ms(async, NNG_OPT_RECVTIMEO, D_IPC_NNG_WAIT_TIMEOUT);

            ret = ipc_nng_dial_retry(async, address_req);
            if (0 != ret)
            {
                LOG_PRINT_ERROR("ipc_nng_dial_retry fail!");
                nng_msg_free(async_req_msg);
                nng_socket_close(async);
                continue;
            }

            ret = nng_sendmsg(async, async_req_msg, 0);
            if (0 != ret)
            {
                LOG_PRINT_ERROR("nng_sendmsg fail, nng_error(%d)!", ret);
                nng_msg_free(async_req_msg);
                nng_socket_close(async);
                continue;
            }
            // nng_msg_free(async_req_msg);

            ret = nng_recvmsg(async, &async_resp_msg, 0);
            if (0 != ret)
            {
                LOG_PRINT_ERROR("nng_recvmsg fail, nng_error(%d)!", ret);
            }
            else
            {
                if (NULL != pinfo->reg_info.async_handler)
                {
                    pinfo->reg_info.async_handler(&msg_baseinfo, nng_msg_body(async_resp_msg));
                }
                nng_msg_free(async_resp_msg);
            }
            nng_socket_close(async);
        }
    }
}

static int32_t ipc_nng_create_broadcast_proxy(ipc_nng_manager_info_t *pinfo)
{
    int32_t ret = -1;

    char frontend_socket_path[D_IPC_NNG_SOCKET_ADDRESS_LEN_MAX] = {};
    char backend_socket_path[D_IPC_NNG_SOCKET_ADDRESS_LEN_MAX] = {};

    ret = nng_sub0_open_raw(&pinfo->broadcast_worker.broadcast_proxy.frontend);
    if (0 != ret)
    {
        LOG_PRINT_ERROR("nng_sub0_open_raw fail, nng_error(%d)!", ret);
        return -1;
    }

    ret = nng_pub0_open_raw(&pinfo->broadcast_worker.broadcast_proxy.backend);
    if (0 != ret)
    {
        LOG_PRINT_ERROR("nng_pub0_open_raw fail, nng_error(%d)!", ret);
        return -1;
    }

    snprintf(frontend_socket_path, sizeof(frontend_socket_path), D_IPC_NNG_BROADCAST_FRONTEND_ADDRESS, socket_prefix);
    ret = nng_listener_create(&pinfo->broadcast_worker.broadcast_proxy.front_ls, pinfo->broadcast_worker.broadcast_proxy.frontend,
                              frontend_socket_path);
    if (0 != ret)
    {
        LOG_PRINT_ERROR("nng_listener_create fail, nng_error(%d)!", ret);
        return -1;
    }

    snprintf(backend_socket_path, sizeof(backend_socket_path), D_IPC_NNG_BROADCAST_BACKEND_ADDRESS, socket_prefix);
    ret = nng_listener_create(&pinfo->broadcast_worker.broadcast_proxy.back_ls, pinfo->broadcast_worker.broadcast_proxy.backend,
                              backend_socket_path);
    if (0 != ret)
    {
        LOG_PRINT_ERROR("nng_listener_create fail, nng_error(%d)!", ret);
        return -1;
    }

    ret = nng_listener_start(pinfo->broadcast_worker.broadcast_proxy.front_ls, 0);
    if (0 != ret)
    {
        LOG_PRINT_ERROR("nng_listener_start fail, nng_error(%d)!", ret);
        return -1;
    }

    ret = nng_listener_start(pinfo->broadcast_worker.broadcast_proxy.back_ls, 0);
    if (0 != ret)
    {
        LOG_PRINT_ERROR("nng_listener_start fail, nng_error(%d)!", ret);
        return -1;
    }

    ret = nng_thread_create(&pinfo->broadcast_worker.broadcast_proxy.broadcast_proxy_thread, ipc_nng_broadcast_proxy_handler, &pinfo->broadcast_worker.broadcast_proxy);
    if (0 != ret)
    {
        LOG_PRINT_ERROR("nng_thread_create fail, nng_error(%d)!", ret);
        return -1;
    }
    nng_thread_set_name(pinfo->broadcast_worker.broadcast_proxy.broadcast_proxy_thread, D_IPC_BROADCAST_PROXY_THREAD_NAME);

    return ret;
}

static int32_t ipc_nng_init_broadcast(ipc_nng_manager_info_t *pinfo)
{
    int32_t ret = -1;
    nng_err nret = NNG_OK;
    char frontend_socket_path[D_IPC_NNG_SOCKET_ADDRESS_LEN_MAX] = {};
    char backend_socket_path[D_IPC_NNG_SOCKET_ADDRESS_LEN_MAX] = {};

    pthread_mutex_init(&pinfo->broadcast_worker.mutex_pub, NULL);
    ret = nng_pub0_open_raw(&pinfo->broadcast_worker.pub);
    if (0 != ret)
    {
        LOG_PRINT_ERROR("nng_pub0_open_raw fail, nng_error(%d)!", ret);
        return -1;
    }

    ret = nng_sub0_open_raw(&pinfo->broadcast_worker.sub);
    if (0 != ret)
    {
        LOG_PRINT_ERROR("nng_sub0_open_raw fail, nng_error(%d)!", ret);
        return -1;
    }

    if (E_IPC_NNG_MODULE_ID_MANAGER == pinfo->reg_info.module_id)
    {
        ret = ipc_nng_create_broadcast_proxy(pinfo);
        if (0 != ret)
        {
            LOG_PRINT_ERROR("ipc_nng_create_broadcast_proxy fail, ret[%d]!", ret);
            return -1;
        }
    }

    nret = nng_aio_alloc(&pinfo->broadcast_worker.aio, server_broadcast_cb, pinfo);
    if (NNG_OK != nret)
    {
        LOG_PRINT_ERROR("nng_aio_alloc fail, nng_error(%s)!", nng_strerror(nret));
        return -1;
    }
    nng_aio_set_timeout(pinfo->broadcast_worker.aio, NNG_DURATION_INFINITE);

    snprintf(backend_socket_path, sizeof(backend_socket_path), D_IPC_NNG_BROADCAST_BACKEND_ADDRESS, socket_prefix);
    ret = ipc_nng_dial_retry(pinfo->broadcast_worker.sub, backend_socket_path);
    if (0 != ret)
    {
        LOG_PRINT_ERROR("ipc_nng_dial_retry fail!");
        return -1;
    }
    nng_socket_recv(pinfo->broadcast_worker.sub, pinfo->broadcast_worker.aio);

    // connect with manager module
    snprintf(frontend_socket_path, sizeof(frontend_socket_path), D_IPC_NNG_BROADCAST_FRONTEND_ADDRESS, socket_prefix);
    ret = ipc_nng_dial_retry(pinfo->broadcast_worker.pub, frontend_socket_path);
    if (0 != ret)
    {
        LOG_PRINT_ERROR("ipc_nng_dial_retry fail!");
        return -1;
    }

    return ret;
}

static int32_t ipc_nng_init_rep(ipc_nng_manager_info_t *pinfo)
{
    int32_t ret = -1;
    nng_err nret = NNG_OK;
    char rep_address[D_IPC_NNG_SOCKET_ADDRESS_LEN_MAX] = {0};

    ret = nng_rep0_open(&pinfo->rep_workers.rep);
    if (0 != ret)
    {
        LOG_PRINT_ERROR("nng_rep0_open fail, nng_error(%d)!", ret);
        return -1;
    }

    snprintf(rep_address, sizeof(rep_address), D_IPC_NNG_REQ_RESP_ADDRESS, socket_prefix, pinfo->reg_info.module_id);
    ret = nng_listen(pinfo->rep_workers.rep, rep_address, NULL, 0);
    if (0 != ret)
    {
        LOG_PRINT_ERROR("nng_listen fail, nng_error(%d)!", ret);
        if (ret == NNG_EADDRINUSE)
        {
            LOG_PRINT_ERROR("Another instance is already running.");
            exit(0);
        }
        return -1;
    }

    for (size_t i = 0; i < E_IPC_NNG_MODULE_ID_MAX; ++i)
    {
        pinfo->rep_workers.worker_array[i].pinfo = pinfo;
        nret = nng_aio_alloc(&pinfo->rep_workers.worker_array[i].aio, server_rep_cb, &pinfo->rep_workers.worker_array[i]);
        if (NNG_OK != nret)
        {
            LOG_PRINT_ERROR("nng_aio_alloc fail, nng_error(%s)!", nng_strerror(nret));
            return -1;
        }
        nng_aio_set_timeout(pinfo->rep_workers.worker_array[i].aio, NNG_DURATION_INFINITE);

        ret = nng_ctx_open(&pinfo->rep_workers.worker_array[i].ctx, pinfo->rep_workers.rep);
        if (0 != ret)
        {
            LOG_PRINT_ERROR("nng_ctx_open fail, nng_error(%d)!", ret);
            return -1;
        }
        nng_ctx_recv(pinfo->rep_workers.worker_array[i].ctx, pinfo->rep_workers.worker_array[i].aio);
    }

    return ret;
}

static int32_t ipc_nng_init_async(ipc_nng_manager_info_t *pinfo)
{
    int32_t ret = -1;
    nng_err nret = NNG_OK;
    char async_queue_adress[D_IPC_NNG_SOCKET_ADDRESS_LEN_MAX] = {};
    snprintf(async_queue_adress, sizeof(async_queue_adress), D_INPROC_NNG_ASYNC_MSG_QUEUE_ADDRESS, g_module_id);

    ret = nng_pull0_open(&pinfo->async_worker.pull);
    if (0 != ret)
    {
        LOG_PRINT_ERROR("nng_pull0_open fail, nng_error(%d)!", ret);
        return -1;
    }

    ret = nng_listen(pinfo->async_worker.pull, async_queue_adress, NULL, 0);
    if (0 != ret)
    {
        LOG_PRINT_ERROR("nng_listen fail, nng_error(%d)!", ret);
        return -1;
    }

    ret = nng_push0_open(&pinfo->async_worker.push);
    if (0 != ret)
    {
        LOG_PRINT_ERROR("nng_push0_open fail, nng_error(%d)!", ret);
        return -1;
    }

    ret = ipc_nng_dial_retry(pinfo->async_worker.push, async_queue_adress);
    if (0 != ret)
    {
        LOG_PRINT_ERROR("ipc_nng_dial_retry fail!");
        return -1;
    }

    nret = nng_aio_alloc(&pinfo->async_worker.aio, NULL, NULL);
    if (NNG_OK != nret)
    {
        LOG_PRINT_ERROR("nng_aio_alloc fail, nng_error(%s)!", nng_strerror(nret));
        return -1;
    }
    nng_aio_set_timeout(pinfo->async_worker.aio, NNG_DURATION_INFINITE);

    ret = nng_thread_create(&pinfo->async_worker.async_thread, server_async_cb, pinfo);
    if (0 != ret)
    {
        LOG_PRINT_ERROR("nng_thread_create fail, nng_error(%d)!", ret);
        return -1;
    }
    nng_thread_set_name(pinfo->async_worker.async_thread, D_IPC_ASYNC_THREAD_NAME);

    return ret;
}

static void _ipc_nng_logger(nng_log_level level, nng_log_facility fac, const char *msgid, const char *msg)
{
    (void)fac;

    switch (level)
    {
        case NNG_LOG_ERR:
            LOG_PRINT_ERROR("[%s]: [%s]", msgid, msg);
            break;
        case NNG_LOG_WARN:
        case NNG_LOG_NOTICE:
            LOG_PRINT_WARN("[%s]: [%s]", msgid, msg);
            break;
        case NNG_LOG_INFO:
            LOG_PRINT_INFO("[%s]: [%s]", msgid, msg);
            break;
        case NNG_LOG_DEBUG:
            LOG_PRINT_DEBUG("[%s]: [%s]", msgid, msg);
            break;
        case NNG_LOG_NONE:
            break;
        default:
            break;
    }
}

int32_t ipc_nng_init(ipc_nng_register_info_t *reg_info)
{
    int32_t ret = 0;
    nng_err err = NNG_OK;
    char socket_path[D_IPC_NNG_SOCKET_ADDRESS_LEN_MAX] = {0};

    get_nng_lib_version();

    err = nng_init(NULL);
    if (NNG_OK != err)
    {
        LOG_PRINT_ERROR("Failed to init library, %s\n", nng_strerror(err));
        return -1;
    }

    if (NULL == reg_info)
    {
        LOG_PRINT_ERROR("invalid params!");
        return -1;
    }

    if (NULL != g_manager_info[reg_info->module_id])
    {
        LOG_PRINT_ERROR("ipc inited!");
        return -1;
    }

    nng_log_set_logger(_ipc_nng_logger);
    nng_log_set_level(NNG_LOG_DEBUG);

    socket_prefix = getenv("XDG_RUNTIME_DIR");
    if (!socket_prefix || access(socket_prefix, W_OK) != 0)
    {
        socket_prefix = "/tmp";
    }
    snprintf(socket_path, sizeof(socket_path), D_IPC_NNG_REQ_RESP_ADDRESS, socket_prefix, reg_info->module_id);

    g_manager_info[reg_info->module_id] = (ipc_nng_manager_info_t *)malloc(sizeof(ipc_nng_manager_info_t));
    if (NULL == g_manager_info[reg_info->module_id])
    {
        LOG_PRINT_ERROR("malloc fail, errno[%d](%s)!", errno, strerror(errno));
        return -1;
    }
    memset(g_manager_info[reg_info->module_id], 0x00, sizeof(ipc_nng_manager_info_t));
    memcpy(&g_manager_info[reg_info->module_id]->reg_info, reg_info, sizeof(ipc_nng_register_info_t));
    g_module_id = reg_info->module_id;

    g_manager_info[reg_info->module_id]->is_run = true;

    ret = ipc_nng_init_broadcast(g_manager_info[reg_info->module_id]);
    if (0 != ret)
    {
        LOG_PRINT_ERROR("ipc_nng_init_broadcast fail!");
        return -1;
    }

    ret = ipc_nng_init_rep(g_manager_info[reg_info->module_id]);
    if (0 != ret)
    {
        LOG_PRINT_ERROR("ipc_nng_init_rep fail!");
        return -1;
    }

    ret = ipc_nng_init_async(g_manager_info[reg_info->module_id]);
    if (0 != ret)
    {
        LOG_PRINT_ERROR("ipc_nng_init_async fail!");
        return -1;
    }

    return ret;
}

int32_t ipc_nng_destroy(uint32_t module_id)
{
    (void)module_id;
    pthread_mutex_destroy(&g_manager_info[module_id]->broadcast_worker.mutex_pub);
    nng_fini();
    return 0;
}

int32_t ipc_nng_send_notify(uint32_t src_id, uint32_t dest_id, uint32_t msg_id, const void *notify_data, size_t notify_data_len)
{
    int32_t ret = -1;
    char address[D_IPC_NNG_SOCKET_ADDRESS_LEN_MAX] = {0};
    ipc_nng_msg_baseinfo_t msg_baseinfo = {};
    nng_msg *notify_req_msg = NULL;
    nng_msg *notify_resp_msg = NULL;
    nng_socket notify = NNG_SOCKET_INITIALIZER;

    ret = nng_req0_open(&notify);
    if (0 != ret)
    {
        LOG_PRINT_ERROR("nng_req0_open fail, nng_error(%d)!", ret);
        return ret;
    }

#if 0
    // req identity
    uint16_t proto_id = -1;
    uint16_t peer_id = -1;
    const char *proto_name = NULL;
    const char *peer_name = NULL;
    nng_socket_proto_id(notify, &proto_id);
    nng_socket_peer_id(notify, &peer_id);
    nng_socket_proto_name(notify, &proto_name);
    nng_socket_peer_name(notify, &peer_name);

    LOG_PRINT_DEBUG("notify proto_id[%d](%d); proto_name[%u](%u), proto_name[%s](%s), peer_name[%s](%s)",
                  proto_id, 0x30,
                  peer_id, 0x31,
                  proto_name, "req",
                  peer_name, "rep");
#endif

    nng_socket_set_ms(notify, NNG_OPT_REQ_RESENDTIME, 0); // no retry; NNG_ECONNRESET
    nng_socket_set_ms(notify, NNG_OPT_SENDTIMEO, D_IPC_NNG_WAIT_TIMEOUT);
    nng_socket_set_ms(notify, NNG_OPT_RECVTIMEO, D_IPC_NNG_WAIT_TIMEOUT);

    snprintf(address, sizeof(address), D_IPC_NNG_REQ_RESP_ADDRESS, socket_prefix, dest_id);
    ret = ipc_nng_dial_retry(notify, address);
    if (0 != ret)
    {
        LOG_PRINT_ERROR("ipc_nng_dial_retry fail!");
        nng_socket_close(notify);
        return ret;
    }

    msg_baseinfo.src_id = src_id;
    msg_baseinfo.dest_id = dest_id;
    msg_baseinfo.msg_type = E_IPC_NNG_MSG_TYPE_NOTIFY;
    msg_baseinfo.msg_id = msg_id;
    msg_baseinfo.send_len = notify_data_len;
    msg_baseinfo.recv_len = 0;

    nng_msg_alloc(&notify_req_msg, 0);
    nng_msg_append(notify_req_msg, &msg_baseinfo, sizeof(msg_baseinfo));
    if (NULL != notify_data && notify_data_len > 0)
    {
        nng_msg_append(notify_req_msg, notify_data, notify_data_len);
    }
    ret = nng_sendmsg(notify, notify_req_msg, 0);
    if (0 != ret)
    {
        LOG_PRINT_ERROR("nng_sendmsg fail, nng_error(%d)!", ret);
        nng_msg_free(notify_req_msg);
        nng_socket_close(notify);
        return ret;
    }
    // nng_msg_free(notify_req_msg);

    ret = nng_recvmsg(notify, &notify_resp_msg, 0);
    if (0 != ret)
    {
        LOG_PRINT_ERROR("nng_recvmsg fail, nng_error(%d)!", ret);
        nng_socket_close(notify);
        return ret;
    }
    LOG_PRINT_ERROR("recv notify resp len[%zd]!", nng_msg_len(notify_resp_msg));
    nng_msg_free(notify_resp_msg);
    nng_socket_close(notify);

    return ret;
}

int32_t ipc_nng_send_sync(uint32_t src_id, uint32_t dest_id, uint32_t msg_id, const void *sync_req_data, size_t sync_req_data_len,
                          void *sync_resp_data, size_t *sync_resp_data_len, size_t sync_resp_data_max_len, size_t timeout)
{
    int32_t ret = -1;
    char address[D_IPC_NNG_SOCKET_ADDRESS_LEN_MAX] = {0};
    ipc_nng_msg_baseinfo_t msg_baseinfo = {};
    nng_msg *sync_req_msg = NULL;
    nng_msg *sync_resp_msg = NULL;
    nng_socket sync = NNG_SOCKET_INITIALIZER;

    ret = nng_req0_open(&sync);
    if (0 != ret)
    {
        LOG_PRINT_ERROR("nng_req0_open fail, nng_error(%d)!", ret);
        return ret;
    }

#if 0
    // req identity
    uint16_t proto_id = -1;
    uint16_t peer_id = -1;
    const char *proto_name = NULL;
    const char *peer_name = NULL;
    nng_socket_proto_id(sync, &proto_id);
    nng_socket_peer_id(sync, &peer_id);
    nng_socket_proto_name(sync, &proto_name);
    nng_socket_peer_name(sync, &peer_name);

    LOG_PRINT_DEBUG("sync proto_id[%d](%d); proto_name[%u](%u), proto_name[%s](%s), peer_name[%s](%s)",
                  proto_id, 0x30,
                  peer_id, 0x31,
                  proto_name, "req",
                  peer_name, "rep");
#endif

    nng_socket_set_ms(sync, NNG_OPT_REQ_RESENDTIME, 0); // no retry; NNG_ECONNRESET
    nng_socket_set_ms(sync, NNG_OPT_SENDTIMEO, D_IPC_NNG_WAIT_TIMEOUT);
    nng_socket_set_ms(sync, NNG_OPT_RECVTIMEO, (nng_duration)timeout);

    snprintf(address, sizeof(address), D_IPC_NNG_REQ_RESP_ADDRESS, socket_prefix, dest_id);
    ret = ipc_nng_dial_retry(sync, address);
    if (0 != ret)
    {
        LOG_PRINT_ERROR("ipc_nng_dial_retry fail!");
        nng_socket_close(sync);
        return ret;
    }

    msg_baseinfo.src_id = src_id;
    msg_baseinfo.dest_id = dest_id;
    msg_baseinfo.msg_type = E_IPC_NNG_MSG_TYPE_SYNC;
    msg_baseinfo.msg_id = msg_id;
    msg_baseinfo.send_len = sync_req_data_len;
    msg_baseinfo.recv_len = sync_resp_data_max_len;

    nng_msg_alloc(&sync_req_msg, 0);
    nng_msg_append(sync_req_msg, &msg_baseinfo, sizeof(msg_baseinfo));
    if (NULL != sync_req_data && sync_req_data_len > 0)
    {
        nng_msg_append(sync_req_msg, sync_req_data, sync_req_data_len);
    }
    ret = nng_sendmsg(sync, sync_req_msg, 0);
    if (0 != ret)
    {
        LOG_PRINT_ERROR("nng_sendmsg fail, nng_error(%d)!", ret);
        nng_msg_free(sync_req_msg);
        nng_socket_close(sync);
        return ret;
    }
    // nng_msg_free(msg);

    ret = nng_recvmsg(sync, &sync_resp_msg, 0);
    if (0 != ret)
    {
        LOG_PRINT_ERROR("nng_recvmsg fail, nng_error(%d)!", ret);
        nng_socket_close(sync);
        return ret;
    }

    LOG_PRINT_ERROR("recv sync resp len[%zd]!", nng_msg_len(sync_resp_msg));
    if (nng_msg_len(sync_resp_msg) <= sync_resp_data_max_len)
    {
        if (NULL != sync_resp_data_len)
        {
            *sync_resp_data_len = nng_msg_len(sync_resp_msg);
        }
        if (NULL != sync_resp_data)
        {
            memcpy(sync_resp_data, nng_msg_body(sync_resp_msg), nng_msg_len(sync_resp_msg));
        }
    }
    nng_msg_free(sync_resp_msg);
    nng_socket_close(sync);

    return ret;
}

int32_t ipc_nng_send_async(uint32_t src_id, uint32_t dest_id, uint32_t msg_id, const void *async_req_data, size_t async_req_data_len, size_t async_resp_data_len)
{
    int32_t ret = -1;
    ipc_nng_msg_baseinfo_t msg_baseinfo = {};
    nng_msg *msg = NULL;

#if 0
    // push identity
    uint16_t proto_id = -1;
    uint16_t peer_id = -1;
    const char *proto_name = NULL;
    const char *peer_name = NULL;
    nng_socket_proto_id(g_manager_info[src_id]->async_worker.push, &proto_id);
    nng_socket_peer_id(g_manager_info[src_id]->async_worker.push, &peer_id);
    nng_socket_proto_name(g_manager_info[src_id]->async_worker.push, &proto_name);
    nng_socket_peer_name(g_manager_info[src_id]->async_worker.push, &peer_name);

    LOG_PRINT_DEBUG("push proto_id[%d](%d); proto_name[%u](%u), proto_name[%s](%s), peer_name[%s](%s)",
                  proto_id, 0x50,
                  peer_id, 0x51,
                  proto_name, "push",
                  peer_name, "pull");
#endif

    msg_baseinfo.src_id = src_id;
    msg_baseinfo.dest_id = dest_id;
    msg_baseinfo.msg_type = E_IPC_NNG_MSG_TYPE_ASYNC;
    msg_baseinfo.msg_id = msg_id;
    msg_baseinfo.send_len = async_req_data_len;
    msg_baseinfo.recv_len = async_resp_data_len;

    nng_msg_alloc(&msg, 0);
    nng_msg_append(msg, &msg_baseinfo, sizeof(msg_baseinfo));
    if (NULL != async_req_data && async_req_data_len > 0)
    {
        nng_msg_append(msg, async_req_data, async_req_data_len);
    }
    ret = nng_sendmsg(g_manager_info[src_id]->async_worker.push, msg, 0);
    if (0 != ret)
    {
        LOG_PRINT_ERROR("nng_sendmsg fail, nng_error(%d)!", ret);
        nng_msg_free(msg);
        return ret;
    }

    // nng_msg_free(msg);

    return ret;
}

int32_t ipc_nng_send_stream(uint32_t src_id, uint32_t dest_id, uint32_t msg_id, const void *stream_data, size_t stream_data_len, size_t timeout)
{
    int32_t ret = -1;
    char address[D_IPC_NNG_SOCKET_ADDRESS_LEN_MAX] = {0};
    ipc_nng_msg_baseinfo_t msg_baseinfo = {};
    nng_msg *sync_req_msg = NULL;
    nng_msg *sync_resp_msg = NULL;
    int32_t stream_ret = INT32_MAX;
    nng_socket sync = NNG_SOCKET_INITIALIZER;

    ret = nng_req0_open(&sync);
    if (0 != ret)
    {
        LOG_PRINT_ERROR("nng_req0_open fail, nng_error(%d)!", ret);
        return ret;
    }

#if 0
    // req identity
    uint16_t proto_id = -1;
    uint16_t peer_id = -1;
    const char *proto_name = NULL;
    const char *peer_name = NULL;
    nng_socket_proto_id(sync, &proto_id);
    nng_socket_peer_id(sync, &peer_id);
    nng_socket_proto_name(sync, &proto_name);
    nng_socket_peer_name(sync, &peer_name);


    LOG_PRINT_DEBUG("sync proto_id[%d](%d); proto_name[%u](%u), proto_name[%s](%s), peer_name[%s](%s)",
                  proto_id, 0x30,
                  peer_id, 0x31,
                  proto_name, "req",
                  peer_name, "rep");
#endif

    nng_socket_set_ms(sync, NNG_OPT_REQ_RESENDTIME, 0); // no retry; NNG_ECONNRESET
    nng_socket_set_ms(sync, NNG_OPT_SENDTIMEO, D_IPC_NNG_WAIT_TIMEOUT);
    nng_socket_set_ms(sync, NNG_OPT_RECVTIMEO, (nng_duration)timeout);

    snprintf(address, sizeof(address), D_IPC_NNG_REQ_RESP_ADDRESS, socket_prefix, dest_id);
    ret = ipc_nng_dial_retry(sync, address);
    if (0 != ret)
    {
        LOG_PRINT_ERROR("ipc_nng_dial_retry fail!");
        nng_socket_close(sync);
        return ret;
    }

    msg_baseinfo.src_id = src_id;
    msg_baseinfo.dest_id = dest_id;
    msg_baseinfo.msg_type = E_IPC_NNG_MSG_TYPE_SYNC;
    msg_baseinfo.msg_id = msg_id;
    msg_baseinfo.send_len = stream_data_len;
    msg_baseinfo.recv_len = sizeof(stream_ret);

    nng_msg_alloc(&sync_req_msg, 0);
    nng_msg_append(sync_req_msg, &msg_baseinfo, sizeof(msg_baseinfo));
    if (NULL != stream_data && stream_data_len > 0)
    {
        nng_msg_append(sync_req_msg, stream_data, stream_data_len);
    }
    ret = nng_sendmsg(sync, sync_req_msg, 0);
    if (0 != ret)
    {
        LOG_PRINT_ERROR("nng_sendmsg fail, nng_error(%d)!", ret);
        nng_msg_free(sync_req_msg);
        nng_socket_close(sync);
        return ret;
    }
    // nng_msg_free(msg);

    ret = nng_recvmsg(sync, &sync_resp_msg, 0);
    if (0 != ret)
    {
        LOG_PRINT_ERROR("nng_recvmsg fail, nng_error(%d)!", ret);
        nng_socket_close(sync);
        return ret;
    }

    LOG_PRINT_ERROR("recv sync resp len[%zd]!", nng_msg_len(sync_resp_msg));
    if (nng_msg_len(sync_resp_msg) <= sizeof(stream_ret))
    {
        memcpy(&stream_ret, nng_msg_body(sync_resp_msg), nng_msg_len(sync_resp_msg));
    }
    nng_msg_free(sync_resp_msg);
    nng_socket_close(sync);

    if (0 != stream_ret)
    {
        LOG_PRINT_ERROR("invalid stream ret[%d]!\n", stream_ret);
        ret = -2;
    }

    return ret;
}

int32_t ipc_nng_send_broadcast(uint32_t src_id, uint32_t msg_id, const void *broadcast_data, size_t broadcast_data_len)
{
    int32_t ret = -1;
    ipc_nng_broadcast_msg_baseinfo_t bmsg_baseinfo = {};
    nng_msg *msg = NULL;

#if 0
    // pub identity
    uint16_t proto_id = -1;
    uint16_t peer_id = -1;
    const char *proto_name = NULL;
    const char *peer_name = NULL;
    nng_socket_proto_id(g_manager_info[src_id]->broadcast_worker.pub, &proto_id);
    nng_socket_peer_id(g_manager_info[src_id]->broadcast_worker.pub, &peer_id);
    nng_socket_proto_name(g_manager_info[src_id]->broadcast_worker.pub, &proto_name);
    nng_socket_peer_name(g_manager_info[src_id]->broadcast_worker.pub, &peer_name);

    LOG_PRINT_DEBUG("pub proto_id[%d](%d); proto_name[%u](%u), proto_name[%s](%s), peer_name[%s](%s)",
                  proto_id, 0x20,
                  peer_id, 0x21,
                  proto_name, "pub",
                  peer_name, "sub");
#endif

    bmsg_baseinfo.sub = 1;
    bmsg_baseinfo.msg_id = msg_id;
    bmsg_baseinfo.src_id = src_id;
    bmsg_baseinfo.msg_type = E_IPC_NNG_MSG_TYPE_BROADCAST;

    nng_msg_alloc(&msg, 0);
    nng_msg_append(msg, topic_buff, sizeof(topic_buff));
    nng_msg_append(msg, &bmsg_baseinfo, sizeof(bmsg_baseinfo));
    if (NULL != broadcast_data && broadcast_data_len > 0)
    {
        nng_msg_append(msg, broadcast_data, broadcast_data_len);
    }

    pthread_mutex_lock(&g_manager_info[src_id]->broadcast_worker.mutex_pub);
    ret = nng_sendmsg(g_manager_info[src_id]->broadcast_worker.pub, msg, 0);
    if (0 != ret)
    {
        pthread_mutex_unlock(&g_manager_info[src_id]->broadcast_worker.mutex_pub);
        LOG_PRINT_ERROR("nng_sendmsg fail, nng_error(%d)!", ret);
        nng_msg_free(msg);
        return ret;
    }
    pthread_mutex_unlock(&g_manager_info[src_id]->broadcast_worker.mutex_pub);

    // nng_msg_free(msg);

    return ret;
}
