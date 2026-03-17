#include <errno.h>
#include <sys/prctl.h>

#include "hv/hv.h"
#include "hv/hloop.h"
#include "hv/hlog.h"

#include "utility/utils.h"
#include "utility/syncctxpool.h"
#include "ipc_hv.h"

#define IPC_HV_LOOP_NUM_MAX        (3) // > 2
#define IPC_HV_SYNC_CTX_INIT_COUNT (3)

typedef struct _ipc_hv_manager_info_t
{
    ipc_hv_register_info_t reg_info;
    syncctxpool_t *syncctx_pool;
    hmutex_t s_mutex;
    hloop_t **worker_loops;
    hthread_t *worker_threads;
    hthread_t accept_thread;
    hloop_t *accept_loop;
    bool is_run;
} ipc_hv_manager_info_t;

static void on_close(hio_t *io);
static void on_close_clear(hio_t *io);
static void on_recv(hio_t *io, void *buf, int readbytes);
static void on_recv_async(hio_t *io, void *buf, int readbytes);
static void on_write(hio_t *io, const void *buf, int writebytes);

static ipc_hv_manager_info_t *g_ipc_minfo[E_IPC_HV_ID_MAX] = {NULL};
static const char *socket_prefix = NULL;
static unpack_setting_t g_unpack_setting = {
    .mode = UNPACK_BY_LENGTH_FIELD,
    .package_max_length = DEFAULT_PACKAGE_MAX_LENGTH,
    .body_offset = sizeof(ipc_hv_msg_t),
    .length_field_offset = sizeof(ipc_hv_msg_t) - 2 * sizeof(size_t),
    .length_field_bytes = sizeof(size_t),
    .length_adjustment = 0,
    .length_field_coding = ENCODE_BY_LITTEL_ENDIAN,
};

static hloop_t *get_idle_loop(ipc_hv_id_e id)
{
    hloop_t *idle_loop = g_ipc_minfo[id]->worker_loops[0];
    uint32_t min_nactives = hloop_nactives(idle_loop);
    uint32_t each_nactives = 0;

    hmutex_lock(&g_ipc_minfo[id]->s_mutex);
    for (size_t i = 1; i < IPC_HV_LOOP_NUM_MAX; ++i)
    {
        each_nactives = hloop_nactives(g_ipc_minfo[id]->worker_loops[i]);
        if (each_nactives < min_nactives)
        {
            min_nactives = each_nactives;
            idle_loop = g_ipc_minfo[id]->worker_loops[i];
        }
    }
    hmutex_unlock(&g_ipc_minfo[id]->s_mutex);

    return idle_loop;
}

static void on_close(hio_t *io)
{
    LOG_PRINT_DEBUG("on_close fd[%d] error[%d]", hio_fd(io), hio_error(io));
}

static void on_close_clear(hio_t *io)
{
    ipc_hv_msg_t *p_ipc_data = (ipc_hv_msg_t *)hevent_userdata(io);
    LOG_PRINT_DEBUG("on_close_clear fd[%d] error[%d]", hio_fd(io), hio_error(io));
    free(p_ipc_data);
}

static void on_recv(hio_t *io, void *buf, int readbytes)
{
    ipc_hv_msg_t *p_im_data = (ipc_hv_msg_t *)buf;
    ipc_hv_manager_info_t *pinfo = (ipc_hv_manager_info_t *)hevent_userdata(io);

    uint8_t *resp_data = NULL;
    size_t resp_data_len = 0;
    size_t send_len = 0;
    ipc_hv_msg_t *send_msg = NULL;

    LOG_PRINT_DEBUG("on_recv fd[%d], readbytes[%d], msg_type[%u], src[%u]->dest[%u], msg_id[%u], timeout[%zu], send_len[%zu], recv_max_len[%zu]",
                    hio_fd(io),
                    readbytes,
                    p_im_data->msg_type,
                    p_im_data->src,
                    p_im_data->dest,
                    p_im_data->msg_id,
                    p_im_data->timeout,
                    p_im_data->send_len,
                    p_im_data->recv_max_len);

    if (E_IPC_HV_MSG_TYPE_SYNC_REQ == p_im_data->msg_type)
    {
        if (p_im_data->recv_max_len > 0)
        {
            resp_data = (uint8_t *)calloc(1, p_im_data->recv_max_len);
            if (NULL == resp_data)
            {
                return;
            }
            resp_data_len = p_im_data->recv_max_len;
            pinfo->reg_info.response_cb(p_im_data, resp_data, &resp_data_len);
        }
        else
        {
            pinfo->reg_info.response_cb(p_im_data, NULL, NULL);
            resp_data_len = 0;
        }

        send_len = sizeof(ipc_hv_msg_t) + resp_data_len;
        send_msg = (ipc_hv_msg_t *)calloc(1, send_len);
        if (NULL == send_msg)
        {
            return;
        }
        send_msg->src = p_im_data->dest;
        send_msg->dest = p_im_data->src;
        send_msg->msg_type = E_IPC_HV_MSG_TYPE_SYNC_REP;
        send_msg->msg_id = p_im_data->msg_id;
        send_msg->ctx = NULL;
        send_msg->timeout = 0;
        send_msg->send_len = resp_data_len;
        send_msg->recv_max_len = 0; // no need
        if (NULL != resp_data && send_msg->send_len > 0)
        {
            memcpy(send_msg->msg_data, resp_data, send_msg->send_len);
            free(resp_data);
        }

        hevent_set_userdata(io, send_msg);
        hio_setcb_close(io, on_close_clear);
        hio_setcb_write(io, on_write);
        hio_set_write_timeout(io, IPC_HV_WRITE_TIMEOUT);
        hio_write(io, (char *)send_msg, send_len);
    }
    else if (E_IPC_HV_MSG_TYPE_ASYNC_REQ == p_im_data->msg_type)
    {
        if (p_im_data->recv_max_len > 0)
        {
            resp_data = (uint8_t *)calloc(1, p_im_data->recv_max_len);
            if (NULL == resp_data)
            {
                return;
            }
            resp_data_len = p_im_data->recv_max_len;
            pinfo->reg_info.response_cb(p_im_data, resp_data, &resp_data_len);
        }
        else
        {
            pinfo->reg_info.response_cb(p_im_data, NULL, NULL);
            resp_data_len = 0;
        }

        send_len = sizeof(ipc_hv_msg_t) + resp_data_len;
        send_msg = (ipc_hv_msg_t *)calloc(1, send_len);
        if (NULL == send_msg)
        {
            return;
        }
        send_msg->src = p_im_data->dest;
        send_msg->dest = p_im_data->src;
        send_msg->msg_type = E_IPC_HV_MSG_TYPE_ASYNC_REP;
        send_msg->msg_id = p_im_data->msg_id;
        send_msg->ctx = NULL;
        send_msg->timeout = 0;
        send_msg->send_len = resp_data_len;
        send_msg->recv_max_len = 0; // no need
        if (NULL != resp_data && send_msg->send_len > 0)
        {
            memcpy(send_msg->msg_data, resp_data, send_msg->send_len);
            free(resp_data);
        }

        hevent_set_userdata(io, send_msg);
        hio_setcb_close(io, on_close_clear);
        hio_setcb_write(io, on_write);
        hio_set_write_timeout(io, IPC_HV_WRITE_TIMEOUT);
        hio_write(io, (char *)send_msg, send_len);
    }
    else if (E_IPC_HV_MSG_TYPE_NOTIFY == p_im_data->msg_type)
    {
        pinfo->reg_info.response_cb(p_im_data, NULL, NULL);
    }
    else
    {
        LOG_PRINT_ERROR("invalid msg_type[%u]", p_im_data->msg_type);
    }

    hio_close(io);
}

static void on_recv_async(hio_t *io, void *buf, int readbytes)
{
    ipc_hv_msg_t *p_im_data = (ipc_hv_msg_t *)buf;

    LOG_PRINT_DEBUG("on_recv_async fd[%d], readbytes[%d], msg_type[%u], src[%u]->dest[%u], msg_id[%u], timeout[%zu], send_len[%zu], recv_max_len[%zu]",
                    hio_fd(io),
                    readbytes,
                    p_im_data->msg_type,
                    p_im_data->src,
                    p_im_data->dest,
                    p_im_data->msg_id,
                    p_im_data->timeout,
                    p_im_data->send_len,
                    p_im_data->recv_max_len);

    ipc_hv_msg_t *p_ipc_msg = (ipc_hv_msg_t *)hevent_userdata(io);

    if (p_im_data->msg_type == E_IPC_HV_MSG_TYPE_SYNC_REP)
    {
        syncctx_t *ctx = (syncctx_t *)p_ipc_msg->ctx;
        hmutex_lock(&ctx->mutex);
        if (p_im_data->send_len > 0 && NULL != ctx->resp_data && NULL != ctx->resp_len)
        {
            size_t copy_len = (p_im_data->send_len < *ctx->resp_len) ? p_im_data->send_len : *ctx->resp_len;
            memcpy(ctx->resp_data, p_im_data->msg_data, copy_len);
            *ctx->resp_len = copy_len;
        }
        ctx->result = 0;
        hcondvar_signal(&ctx->cond);
        hmutex_unlock(&ctx->mutex);
    }
    else if (p_im_data->msg_type == E_IPC_HV_MSG_TYPE_ASYNC_REP)
    {
        if (NULL != p_ipc_msg->ctx)
        {
            ipc_hv_handle_cb_t async_cb = (ipc_hv_handle_cb_t)p_ipc_msg->ctx;
            async_cb(p_im_data, NULL, NULL);
        }
    }

    hio_close(io);
}

static void on_write(hio_t *io, const void *buf, int writebytes)
{
    if (!hio_write_is_complete(io))
    {
        return;
    }

    ipc_hv_msg_t *p_im_data = (ipc_hv_msg_t *)buf;
    LOG_PRINT_DEBUG("on_write fd[%d], writebytes[%d], msg_type[%u], src[%u]->dest[%u], msg_id[%u], timeout[%zu], send_len[%zu], recv_max_len[%zu]",
                    hio_fd(io),
                    writebytes,
                    p_im_data->msg_type,
                    p_im_data->src,
                    p_im_data->dest,
                    p_im_data->msg_id,
                    p_im_data->timeout,
                    p_im_data->send_len,
                    p_im_data->recv_max_len);
    if (p_im_data->msg_type == E_IPC_HV_MSG_TYPE_ASYNC_REQ || p_im_data->msg_type == E_IPC_HV_MSG_TYPE_SYNC_REQ)
    {
        hio_setcb_read(io, on_recv_async);
        hio_set_unpack(io, &g_unpack_setting);
        if (UINT32_MAX != p_im_data->timeout)
        {
            hio_set_read_timeout(io, (int)p_im_data->timeout);
        }
        hio_read(io);
    }
    else if (p_im_data->msg_type == E_IPC_HV_MSG_TYPE_NOTIFY || p_im_data->msg_type == E_IPC_HV_MSG_TYPE_SYNC_REP || p_im_data->msg_type == E_IPC_HV_MSG_TYPE_ASYNC_REP)
    {
        hio_close(io);
    }
    else
    {
    }
}

static void on_connect(hio_t *io)
{
    LOG_PRINT_DEBUG("on_connect fd[%d]", hio_fd(io));
    if (hio_is_connected(io))
    {
        ipc_hv_msg_t *p_ipc_msg = (ipc_hv_msg_t *)hevent_userdata(io);
        hio_setcb_write(io, on_write);
        hio_set_write_timeout(io, IPC_HV_WRITE_TIMEOUT);
        hio_write(io, p_ipc_msg, sizeof(ipc_hv_msg_t) + p_ipc_msg->send_len);
    }
    else
    {
        LOG_PRINT_ERROR("peer is closed");
        hio_close(io);
    }
}

static void on_post_event_cb(hevent_t *ev)
{
    hloop_t *loop = ev->loop;
    hio_t *io = (hio_t *)hevent_userdata(ev);
    hio_attach(loop, io);

    LOG_PRINT_DEBUG("on_accept fd[%d]", hio_fd(io));
    hio_setcb_read(io, on_recv);
    hio_set_unpack(io, &g_unpack_setting);
    hio_set_read_timeout(io, IPC_HV_READ_TIMEOUT);
    hio_read(io);
}

static void on_accept(hio_t *io)
{
    hio_detach(io);

    ipc_hv_manager_info_t *pinfo = hevent_userdata(io);
    hloop_t *worker_loop = get_idle_loop(pinfo->reg_info.module_id);
    hevent_t ev = {};
    ev.loop = worker_loop;
    ev.cb = on_post_event_cb;
    ev.userdata = io;
    hloop_post_event(worker_loop, &ev);
}

static HTHREAD_ROUTINE(worker_thread)
{
    hloop_t *loop = (hloop_t *)userdata;
    prctl(PR_SET_NAME, "workers");
    LOG_PRINT_DEBUG("engine[%s]-pid[%ld]-tid[%ld]", hio_engine(), hloop_pid(loop), hloop_tid(loop));
    hloop_run(loop);
    return 0;
}

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

static int ipc_hv_run(ipc_hv_id_e id)
{
    prctl(PR_SET_NAME, "acceptor");
    LOG_PRINT_DEBUG("engine[%s]-pid[%ld]-tid[%ld]", hio_engine(), hloop_pid(g_ipc_minfo[id]->accept_loop), hloop_tid(g_ipc_minfo[id]->accept_loop));
    return hloop_run(g_ipc_minfo[id]->accept_loop);
}

static void *ipc_thread_func(void *arg)
{
    ipc_hv_register_info_t *reg_info = (ipc_hv_register_info_t *)arg;
    ipc_hv_run(reg_info->module_id);
    return NULL;
}

static int _ipc_hv_init(ipc_hv_register_info_t *reg_info)
{
    size_t i = 0;
    char socket_path[UNIX_SOCKET_NAME_LEN_MAX] = {};

    hlog_set_handler(_ipc_hv_logger);
    hlog_set_level(LOG_LEVEL_INFO);
    hlog_set_format("HV-%L %s");

    socket_prefix = getenv("XDG_RUNTIME_DIR");
    if (!socket_prefix || access(socket_prefix, W_OK) != 0)
    {
        socket_prefix = "/tmp";
    }
    snprintf(socket_path, sizeof(socket_path), "%s/ipc_hv_%d.ipc", socket_prefix, reg_info->module_id);
    if (access(socket_path, F_OK) == 0)
    {
        if (0 > unlink(socket_path))
        {
            return -1;
        }
    }

    if (NULL != g_ipc_minfo[reg_info->module_id])
    {
        return -1;
    }

    g_ipc_minfo[reg_info->module_id] = calloc(1, sizeof(ipc_hv_manager_info_t));
    if (NULL == g_ipc_minfo[reg_info->module_id])
    {
        LOG_PRINT_ERROR("calloc fail, errno[%d]", errno);
        return -1;
    }

    g_ipc_minfo[reg_info->module_id]->reg_info = *reg_info;

    g_ipc_minfo[reg_info->module_id]->syncctx_pool = syncctxpool_create(IPC_HV_SYNC_CTX_INIT_COUNT);
    if (NULL == g_ipc_minfo[reg_info->module_id]->syncctx_pool)
    {
        syncctxpool_destroy(g_ipc_minfo[reg_info->module_id]->syncctx_pool);
        return -1;
    }

    hmutex_init(&g_ipc_minfo[reg_info->module_id]->s_mutex);
    g_ipc_minfo[reg_info->module_id]->worker_loops = (hloop_t **)calloc(IPC_HV_LOOP_NUM_MAX, sizeof(hloop_t *));
    if (NULL == g_ipc_minfo[reg_info->module_id]->worker_loops)
    {
        hmutex_destroy(&g_ipc_minfo[reg_info->module_id]->s_mutex);
        free(g_ipc_minfo[reg_info->module_id]);
        g_ipc_minfo[reg_info->module_id] = NULL;
        return -1;
    }

    g_ipc_minfo[reg_info->module_id]->worker_threads = (hthread_t *)calloc(IPC_HV_LOOP_NUM_MAX, sizeof(hthread_t));
    if (NULL == g_ipc_minfo[reg_info->module_id]->worker_threads)
    {
        hmutex_destroy(&g_ipc_minfo[reg_info->module_id]->s_mutex);
        free(g_ipc_minfo[reg_info->module_id]->worker_loops);
        g_ipc_minfo[reg_info->module_id]->worker_loops = NULL;
        free(g_ipc_minfo[reg_info->module_id]);
        g_ipc_minfo[reg_info->module_id] = NULL;
        return -1;
    }

    hmutex_lock(&g_ipc_minfo[reg_info->module_id]->s_mutex);
    for (i = 0; i < IPC_HV_LOOP_NUM_MAX; ++i)
    {
        g_ipc_minfo[reg_info->module_id]->worker_loops[i] = hloop_new(HLOOP_FLAG_AUTO_FREE);
        g_ipc_minfo[reg_info->module_id]->worker_threads[i] = hthread_create(worker_thread, g_ipc_minfo[reg_info->module_id]->worker_loops[i]);
        while (HLOOP_STATUS_RUNNING != hloop_status(g_ipc_minfo[reg_info->module_id]->worker_loops[i]))
        {
            hv_delay(1);
        }
    }
    hmutex_unlock(&g_ipc_minfo[reg_info->module_id]->s_mutex);

    g_ipc_minfo[reg_info->module_id]->accept_loop = hloop_new(HLOOP_FLAG_AUTO_FREE);
    hio_t *listenio = hio_create_socket(g_ipc_minfo[reg_info->module_id]->accept_loop, socket_path, -1, HIO_TYPE_SOCK_STREAM, HIO_SERVER_SIDE);
    if (NULL == listenio)
    {
        for (i = 0; i < IPC_HV_LOOP_NUM_MAX; ++i)
        {
            hloop_stop(g_ipc_minfo[reg_info->module_id]->worker_loops[i]);
            hthread_join(g_ipc_minfo[reg_info->module_id]->worker_threads[i]);
        }
        hmutex_destroy(&g_ipc_minfo[reg_info->module_id]->s_mutex);
        free(g_ipc_minfo[reg_info->module_id]->worker_threads);
        g_ipc_minfo[reg_info->module_id]->worker_threads = NULL;
        free(g_ipc_minfo[reg_info->module_id]->worker_loops);
        g_ipc_minfo[reg_info->module_id]->worker_loops = NULL;
        free(g_ipc_minfo[reg_info->module_id]);
        g_ipc_minfo[reg_info->module_id] = NULL;
        return -1;
    }
    hevent_set_userdata(listenio, g_ipc_minfo[reg_info->module_id]);
    hio_setcb_close(listenio, on_close);
    hio_setcb_accept(listenio, on_accept);
    hio_accept(listenio);

    g_ipc_minfo[reg_info->module_id]->accept_thread = hthread_create(ipc_thread_func, reg_info);
    // usleep(100 * 1000); // to wait thread run

    while (HLOOP_STATUS_RUNNING != hloop_status(g_ipc_minfo[reg_info->module_id]->accept_loop))
    {
        hv_delay(1);
    }

    g_ipc_minfo[reg_info->module_id]->is_run = true;

    return 0;
}

int ipc_hv_init(ipc_hv_register_info_t *reg_info)
{
    int ret = -1;

    if (NULL == reg_info)
    {
        LOG_PRINT_ERROR("ipc not inited!");
        return -1;
    }

    ret = _ipc_hv_init(reg_info);

    return ret;
}

void ipc_hv_destroy(ipc_hv_id_e id)
{
    if (NULL == g_ipc_minfo[id] || !g_ipc_minfo[id]->is_run)
    {
        LOG_PRINT_ERROR("ipc not inited!");
        return;
    }

    hloop_stop(g_ipc_minfo[id]->accept_loop);
    hthread_join(g_ipc_minfo[id]->accept_thread);

    hmutex_lock(&g_ipc_minfo[id]->s_mutex);
    for (size_t i = 0; i < IPC_HV_LOOP_NUM_MAX; ++i)
    {
        hloop_stop(g_ipc_minfo[id]->worker_loops[i]);
        hthread_join(g_ipc_minfo[id]->worker_threads[i]);
    }

    free(g_ipc_minfo[id]->worker_threads);
    g_ipc_minfo[id]->worker_threads = NULL;
    free(g_ipc_minfo[id]->worker_loops);
    g_ipc_minfo[id]->worker_loops = NULL;
    hmutex_unlock(&g_ipc_minfo[id]->s_mutex);
    hmutex_destroy(&g_ipc_minfo[id]->s_mutex);

    syncctxpool_destroy(g_ipc_minfo[id]->syncctx_pool);

    free(g_ipc_minfo[id]);
    g_ipc_minfo[id] = NULL;
}

int ipc_hv_send_notify(uint32_t src, uint32_t dest, uint32_t msg_id, const uint8_t *notify_data, size_t notify_len)
{
    if (NULL == g_ipc_minfo[src] || !g_ipc_minfo[src]->is_run)
    {
        LOG_PRINT_ERROR("ipc not inited!");
        return -1;
    }

    size_t send_len = 0;
    char socket_path[UNIX_SOCKET_NAME_LEN_MAX] = {};
    hio_t *connio = NULL;
    ipc_hv_msg_t *send_msg = NULL;

    send_len = sizeof(ipc_hv_msg_t) + notify_len;
    send_msg = (ipc_hv_msg_t *)calloc(1, send_len);
    if (NULL == send_msg)
    {
        return -1;
    }
    send_msg->src = src;
    send_msg->dest = dest;
    send_msg->msg_type = E_IPC_HV_MSG_TYPE_NOTIFY;
    send_msg->msg_id = msg_id;
    send_msg->ctx = NULL;
    send_msg->timeout = 0;
    send_msg->send_len = notify_len;
    send_msg->recv_max_len = 0;
    if (NULL != notify_data && send_msg->send_len > 0)
    {
        memcpy(send_msg->msg_data, notify_data, send_msg->send_len);
    }

    snprintf(socket_path, UNIX_SOCKET_NAME_LEN_MAX, "%s/ipc_hv_%u.ipc", socket_prefix, dest);
    connio = hio_create_socket(get_idle_loop(src), socket_path, -1, HIO_TYPE_SOCK_STREAM, HIO_CLIENT_SIDE);
    if (NULL == connio)
    {
        LOG_PRINT_ERROR("hio_create_socket fail!");
        free(send_msg);
        return -1;
    }
    hevent_set_userdata(connio, send_msg);
    hio_setcb_close(connio, on_close_clear);
    hio_setcb_connect(connio, on_connect);
    hio_set_connect_timeout(connio, IPC_HV_CONNECT_TIMEOUT);
    if (0 != hio_connect(connio))
    {
        LOG_PRINT_ERROR("hio_connect fail, errno[%d]!", hio_error(connio));
        free(send_msg);
        return -1;
    }

    return 0;
}

static int _ipc_hv_send_async(uint32_t src, uint32_t dest, uint32_t msg_type, uint32_t msg_id, const uint8_t *req_data, size_t req_len, size_t resp_max_len, void *ctx, size_t timeout)
{
    if (NULL == g_ipc_minfo[src] || !g_ipc_minfo[src]->is_run)
    {
        LOG_PRINT_ERROR("ipc not inited!");
        return -1;
    }

    size_t send_len = 0;
    char socket_path[UNIX_SOCKET_NAME_LEN_MAX] = {};
    hio_t *connio = NULL;
    ipc_hv_msg_t *send_msg = NULL;

    send_len = sizeof(ipc_hv_msg_t) + req_len;
    send_msg = (ipc_hv_msg_t *)calloc(1, send_len);
    if (NULL == send_msg)
    {
        return -1;
    }
    send_msg->src = src;
    send_msg->dest = dest;
    send_msg->msg_type = msg_type;
    send_msg->msg_id = msg_id;
    send_msg->ctx = ctx;
    send_msg->timeout = timeout;
    send_msg->send_len = req_len;
    send_msg->recv_max_len = resp_max_len;
    if (NULL != req_data && send_msg->send_len > 0)
    {
        memcpy(send_msg->msg_data, req_data, send_msg->send_len);
    }

    snprintf(socket_path, UNIX_SOCKET_NAME_LEN_MAX, "%s/ipc_hv_%u.ipc", socket_prefix, dest);
    connio = hio_create_socket(get_idle_loop(src), socket_path, -1, HIO_TYPE_SOCK_STREAM, HIO_CLIENT_SIDE);
    if (NULL == connio)
    {
        LOG_PRINT_ERROR("hio_create_socket fail!");
        free(send_msg);
        return -1;
    }
    hevent_set_userdata(connio, send_msg);
    hio_setcb_close(connio, on_close_clear);
    hio_setcb_connect(connio, on_connect);
    hio_set_connect_timeout(connio, IPC_HV_CONNECT_TIMEOUT);
    if (0 != hio_connect(connio))
    {
        LOG_PRINT_ERROR("hio_connect fail, errno[%d]!", hio_error(connio));
        free(send_msg);
        return -1;
    }

    return 0;
}

int ipc_hv_send_async(uint32_t src, uint32_t dest, uint32_t msg_id, const uint8_t *req_data, size_t req_len, size_t resp_max_len, ipc_hv_handle_cb_t async_cb, size_t timeout)
{
    return _ipc_hv_send_async(src, dest, E_IPC_HV_MSG_TYPE_ASYNC_REQ, msg_id, req_data, req_len, resp_max_len, (void *)async_cb, timeout);
}

int ipc_hv_send_sync(uint32_t src, uint32_t dest, uint32_t msg_id, const uint8_t *req_data, size_t req_len, uint8_t *resp_data, size_t *resp_len, size_t timeout)
{
    if (NULL == g_ipc_minfo[src] || !g_ipc_minfo[src]->is_run)
    {
        LOG_PRINT_ERROR("ipc not inited!");
        return -1;
    }

    if (NULL == resp_len)
    {
        return -1;
    }

    int ret = -1;
    syncctx_t *p_sync_ctx = NULL;

    p_sync_ctx = syncctxpool_borrow(g_ipc_minfo[src]->syncctx_pool);
    if (NULL == p_sync_ctx)
    {
        LOG_PRINT_WARN("syncctxpool_borrow fail!");
        return -1; // need wait and retry;
    }

    p_sync_ctx->resp_data = resp_data;
    p_sync_ctx->resp_len = resp_len;
    p_sync_ctx->result = -1;

    do
    {
        ret = _ipc_hv_send_async(src, dest, E_IPC_HV_MSG_TYPE_SYNC_REQ, msg_id, req_data, req_len, *resp_len, p_sync_ctx, timeout);
        if (0 != ret)
        {
            break;
        }

        hmutex_lock(&p_sync_ctx->mutex);
        ret = hcondvar_wait_for(&p_sync_ctx->cond, &p_sync_ctx->mutex, (unsigned int)timeout);
        if (ret == 1)
        {
            ret = p_sync_ctx->result;
        }
        else
        {
            ret = -8;
            break;
        }
        hmutex_unlock(&p_sync_ctx->mutex);
    } while (0);
    syncctxpool_return(g_ipc_minfo[src]->syncctx_pool, p_sync_ctx);

    return ret;
}
