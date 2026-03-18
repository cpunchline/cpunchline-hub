#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <pthread.h>
#include <sys/timerfd.h>
#include <sys/eventfd.h>
#include <sys/epoll.h>
#include <inttypes.h>
#include "utility/utils.h"
#include "dsa/heap.h"
#include "dsa/rbtree.h"
#include "utility/timercluster.h"

typedef struct timernode_t
{
    struct heap_node heap_node;
    struct rb_node rb_node;
    timerid_t timer_id;
    timertype_e timer_type;
    uint32_t timer_interval;
    struct timespec expire_time;
    timercallback_t callback;
    void *userdata;
} timernode_t;

typedef struct timernode_inn_t
{
    timerid_t timer_id;
    timercallback_t callback;
    void *userdata;
} timernode_inn_t;

struct timercluster_t
{
    struct heap heap;
    struct rb_root rb;
    pthread_mutex_t lock;
    pthread_cond_t cond;
    pthread_t thread;
    bool running;
};

struct timercluster_v2_t
{
    int timer_fd;
    int event_wfd; // write fd for signaling
    int event_rfd; // read fd for epoll
    int epoll_fd;
    pthread_mutex_t lock;
    struct heap heap;
    struct rb_root rb;
    pthread_t thread;
    bool running;
};

static int compare_timer(const struct heap_node *a, const struct heap_node *b)
{
    const timernode_t *ta = (const timernode_t *)a;
    const timernode_t *tb = (const timernode_t *)b;
    return ta->expire_time.tv_sec < tb->expire_time.tv_sec ||
        (ta->expire_time.tv_sec == tb->expire_time.tv_sec &&
         ta->expire_time.tv_nsec < tb->expire_time.tv_nsec);
}

static int time_less_than(const struct timespec *a, const struct timespec *b)
{
    return a->tv_sec < b->tv_sec || (a->tv_sec == b->tv_sec && a->tv_nsec < b->tv_nsec);
}

static timernode_t *rbtree_search(struct rb_root *root, const timerid_t *timer_id)
{
    struct rb_node *n = root->rb_node;
    timernode_t *e = NULL;
    while (n)
    {
        e = rb_entry(n, timernode_t, rb_node);
        if (*timer_id < e->timer_id)
        {
            n = n->rb_left;
        }
        else if (*timer_id > e->timer_id)
        {
            n = n->rb_right;
        }
        else
        {
            return e;
        }
    }
    return NULL;
}

static int rbtree_insert(struct rb_root *root, timernode_t *entry)
{
    struct rb_node **n = &root->rb_node;
    struct rb_node *parent = NULL;
    timernode_t *e = NULL;
    while (*n)
    {
        parent = *n;
        e = rb_entry(*n, timernode_t, rb_node);
        if (entry->timer_id < e->timer_id)
        {
            n = &(*n)->rb_left;
        }
        else if (entry->timer_id > e->timer_id)
        {
            n = &(*n)->rb_right;
        }
        else
        {
            return TIMERCLUSTER_RET_ERR_OTHER;
        }
    }

    rb_link_node(&entry->rb_node, parent, n);
    rb_insert_color(&entry->rb_node, root);
    return TIMERCLUSTER_RET_SUCCESS;
}

static void *timercluster_thread_func(void *arg)
{
    timercluster_t *cluster = (timercluster_t *)arg;
    LOG_PRINT_INFO("timer cluster run");

    while (1)
    {
        pthread_mutex_lock(&cluster->lock);
        if (!cluster->running)
        {
            pthread_mutex_unlock(&cluster->lock);
            break;
        }

        if (!cluster->heap.root)
        {
            pthread_cond_wait(&cluster->cond, &cluster->lock);
            pthread_mutex_unlock(&cluster->lock);
            continue;
        }

        timernode_t *top = (timernode_t *)cluster->heap.root;
        struct timespec now = util_time_mono();

        if (time_less_than(&now, &top->expire_time))
        {
            pthread_cond_timedwait(&cluster->cond, &cluster->lock, &top->expire_time);
            pthread_mutex_unlock(&cluster->lock);
            continue;
        }

        heap_dequeue(&cluster->heap);
        timernode_inn_t timernode_inn = {
            .timer_id = top->timer_id,
            .callback = top->callback,
            .userdata = top->userdata,
        };

        LOG_PRINT_DEBUG("timer_id[%u] timeout", timernode_inn.timer_id);
        if (top->timer_type == TIMER_TYPE_CYCLE)
        {
            top->expire_time = util_time_mono_after(top->timer_interval);
            heap_insert(&cluster->heap, &top->heap_node);
        }
        else
        {
            LOG_PRINT_INFO("timer_id[%u] del success", timernode_inn.timer_id);
            rb_erase(&top->rb_node, &cluster->rb);
            free(top);
        }

        pthread_mutex_unlock(&cluster->lock);

        if (timernode_inn.callback)
        {
            // todo: thread pool
            timernode_inn.callback(timernode_inn.timer_id, timernode_inn.userdata);
        }
    }

    LOG_PRINT_INFO("timer cluster stoped");
    return NULL;
}

timercluster_t *timercluster_init(void)
{
    timercluster_t *cluster = calloc(1, sizeof(timercluster_t));
    if (NULL == cluster)
    {
        LOG_PRINT_ERROR("calloc fail, errno[%d](%s)", errno, strerror(errno));
        return NULL;
    }
    heap_init(&cluster->heap, compare_timer);
    cluster->rb = RB_ROOT;

    pthread_mutexattr_t mutexattr = {};
    pthread_mutexattr_init(&mutexattr);
    (void)pthread_mutexattr_settype(&mutexattr, PTHREAD_MUTEX_ERRORCHECK);
    while ((pthread_mutex_init(&cluster->lock, &mutexattr) != 0) &&
           (pthread_mutex_init(&cluster->lock, NULL) != 0))
    {
        // We must have memory exhaustion -- ENOMEM, or
        // in some cases EAGAIN.  Wait a bit before we try to
        // give things a chance to settle down.
        util_msleep(10);
    }
    pthread_mutexattr_destroy(&mutexattr);

    pthread_condattr_t condattr = {};
    pthread_condattr_init(&condattr);
    pthread_condattr_setclock(&condattr, CLOCK_MONOTONIC);
    while (pthread_cond_init(&cluster->cond, &condattr) != 0)
    {
        util_msleep(10);
    }
    pthread_condattr_destroy(&condattr);
    cluster->running = true;
    pthread_create(&cluster->thread, NULL, timercluster_thread_func, cluster);
    char thread_name[16] = {};
    snprintf(thread_name, sizeof(thread_name), "timercluster");
    pthread_setname_np(cluster->thread, thread_name);
    LOG_PRINT_INFO("timer cluster inited");

    return cluster;
}

void timercluster_destroy(timercluster_t *cluster)
{
    if (NULL == cluster)
    {
        LOG_PRINT_ERROR("invalid param!");
        return;
    }

    pthread_mutex_lock(&cluster->lock);
    cluster->running = false;
    pthread_cond_broadcast(&cluster->cond);
    pthread_mutex_unlock(&cluster->lock);
    if (cluster->thread)
    {
        pthread_join(cluster->thread, NULL);
    }

    while (cluster->heap.root)
    {
        heap_dequeue(&cluster->heap);
    }

    struct rb_node *node = NULL;
    timernode_t *entry = NULL;
    while ((node = cluster->rb.rb_node))
    {
        entry = rb_entry(node, timernode_t, rb_node);
        rb_erase(node, &cluster->rb);
        free(entry);
        entry = NULL;
    }
    pthread_mutex_destroy(&cluster->lock);
    pthread_cond_destroy(&cluster->cond);
    free(cluster);

    LOG_PRINT_WARN("timer cluster destroyed");
}

bool timercluster_timer_exist(timercluster_t *cluster, timerid_t id)
{
    if (NULL == cluster)
    {
        LOG_PRINT_ERROR("invalid param!");
        return false;
    }

    pthread_mutex_lock(&cluster->lock);
    if (!cluster->running)
    {
        LOG_PRINT_ERROR("timercluster is not running!");
        pthread_mutex_unlock(&cluster->lock);
        return false;
    }

    bool exists = (rbtree_search(&cluster->rb, &id) != NULL) ? true : false;
    pthread_mutex_unlock(&cluster->lock);

    return exists;
}

bool timercluster_timer_add(timercluster_t *cluster, timerid_t id, timertype_e type, uint32_t interval, timercallback_t cb, void *userdata)
{
    if (NULL == cluster)
    {
        LOG_PRINT_ERROR("invalid param!");
        return false;
    }

    timernode_t *tnode = NULL;
    pthread_mutex_lock(&cluster->lock);
    if (!cluster->running)
    {
        LOG_PRINT_ERROR("timercluster is not running!");
        pthread_mutex_unlock(&cluster->lock);
        return false;
    }

    tnode = rbtree_search(&cluster->rb, &id);
    if (NULL != tnode)
    {
        LOG_PRINT_ERROR("timer_id[%u] is existed", id);
        pthread_mutex_unlock(&cluster->lock);
        return false;
    }

    tnode = (timernode_t *)calloc(1, sizeof(timernode_t));
    if (NULL == tnode)
    {
        LOG_PRINT_ERROR("calloc fail, errno[%d]", errno);
        pthread_mutex_unlock(&cluster->lock);
        return false;
    }

    tnode->timer_id = id;
    tnode->timer_type = type;
    tnode->timer_interval = interval;
    tnode->expire_time = util_time_mono_after(interval);
    tnode->callback = cb;
    tnode->userdata = userdata;
    if (0 != rbtree_insert(&cluster->rb, tnode))
    {
        LOG_PRINT_ERROR("rbtree_insert fail!");
        pthread_mutex_unlock(&cluster->lock);
        free(tnode);
        return false;
    }
    heap_insert(&cluster->heap, &tnode->heap_node);
    pthread_cond_signal(&cluster->cond);
    pthread_mutex_unlock(&cluster->lock);

    LOG_PRINT_INFO("timer_id[%u] add success", id);

    return true;
}

bool timercluster_timer_del(timercluster_t *cluster, timerid_t id)
{
    if (NULL == cluster)
    {
        LOG_PRINT_ERROR("invalid param!");
        return false;
    }

    timernode_t *tnode = NULL;
    pthread_mutex_lock(&cluster->lock);
    if (!cluster->running)
    {
        LOG_PRINT_ERROR("timercluster is not running!");
        pthread_mutex_unlock(&cluster->lock);
        return false;
    }

    tnode = rbtree_search(&cluster->rb, &id);
    if (NULL == tnode)
    {
        LOG_PRINT_ERROR("timer_id[%u] is not existed or already deleted", id);
        pthread_mutex_unlock(&cluster->lock);
        return false;
    }

    rb_erase(&tnode->rb_node, &cluster->rb);
    heap_remove(&cluster->heap, &tnode->heap_node);
    free(tnode);
    pthread_cond_signal(&cluster->cond);
    pthread_mutex_unlock(&cluster->lock);

    LOG_PRINT_INFO("timer_id[%u] del success", id);

    return true;
}

bool timercluster_timer_reset(timercluster_t *cluster, timerid_t id, uint32_t new_interval)
{
    if (NULL == cluster)
    {
        LOG_PRINT_ERROR("invalid param!");
        return false;
    }

    timernode_t *tnode = NULL;
    pthread_mutex_lock(&cluster->lock);
    if (!cluster->running)
    {
        LOG_PRINT_ERROR("timercluster is not running!");
        pthread_mutex_unlock(&cluster->lock);
        return false;
    }

    tnode = rbtree_search(&cluster->rb, &id);
    if (NULL == tnode)
    {
        LOG_PRINT_ERROR("timer_id[%u] is not existed or already deleted", id);
        pthread_mutex_unlock(&cluster->lock);
        return false;
    }

    tnode->timer_interval = new_interval;
    tnode->expire_time = util_time_mono_after(new_interval);

    heap_remove(&cluster->heap, &tnode->heap_node);
    heap_insert(&cluster->heap, &tnode->heap_node);
    pthread_cond_signal(&cluster->cond);
    pthread_mutex_unlock(&cluster->lock);

    return true;
}

const char *timercluster_strerror(int err)
{
    switch (err)
    {
#define X(code, name, msg)        \
    case TIMERCLUSTER_RET_##name: \
        return msg;
        TIMERCLUSTER_FOREACH_ERR(X)
#undef X
        default:
            return "Unknown timer cluster error";
    }
}

static void timerfd_set_next_timeout(int timer_fd, struct heap *heap)
{
    struct itimerspec its = {};
    if (heap->root)
    {
        timernode_t *top = (timernode_t *)heap->root;
        struct timespec now = util_time_mono();
        if (!time_less_than(&now, &top->expire_time))
        {
            // Already expired, trigger immediately
            its.it_value.tv_sec = 0;
            its.it_value.tv_nsec = 1; // minimal non-zero
        }
        else
        {
            // Compute relative timeout
            its.it_value.tv_sec = top->expire_time.tv_sec - now.tv_sec;
            its.it_value.tv_nsec = top->expire_time.tv_nsec - now.tv_nsec;
            if (its.it_value.tv_nsec < 0)
            {
                its.it_value.tv_sec -= 1;
                its.it_value.tv_nsec += 1000000000L;
            }
        }
    }
    else
    {
        // disarm timer by setting zero (already zero-initialized)
        its.it_value.tv_sec = 0;
        its.it_value.tv_nsec = 0;
    }

    if (timerfd_settime(timer_fd, 0, &its, NULL) < 0)
    {
        LOG_PRINT_WARN("timerfd_settime fail, errno[%d](%s)", errno, strerror(errno));
    }
}

static void timercluster_v2_wakeup(timercluster_v2_t *cluster)
{
    util_eventfd_raise(cluster->event_wfd);
}

static void *timercluster_v2_thread_func(void *arg)
{
    timercluster_v2_t *cluster = (timercluster_v2_t *)arg;
    struct epoll_event events[2] = {};
    int nfds = 0;

    LOG_PRINT_INFO("timer cluster v2 run");
    while (1)
    {
        memset(events, 0x00, sizeof(events));
        nfds = epoll_wait(cluster->epoll_fd, events, UTIL_ARRAY_SIZE(events), -1);
        if (nfds <= 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            LOG_PRINT_ERROR("epoll_wait fail, errno[%d](%s)", errno, strerror(errno));
            break;
        }

        for (int i = 0; i < nfds; ++i)
        {
            if (events[i].data.fd == cluster->timer_fd)
            {
                // read timerfd to ack the event
                uint64_t expirations = 0;
                ssize_t s = 0;
                s = read(cluster->timer_fd, &expirations, sizeof(expirations));
                if (s != sizeof(expirations))
                {
                    LOG_PRINT_ERROR("read timerfd fail, errno[%d](%s)", errno, strerror(errno));
                    continue;
                }
            }
            else if (events[i].data.fd == cluster->event_rfd)
            {
                util_eventfd_clear(cluster->event_rfd);
            }
            else
            {
                LOG_PRINT_WARN("unknown fd in epoll");
            }
        }

        pthread_mutex_lock(&cluster->lock);
        if (!cluster->running)
        {
            pthread_mutex_unlock(&cluster->lock);
            break;
        }
        pthread_mutex_unlock(&cluster->lock);

        // Process all expired timers
        while (1)
        {
            pthread_mutex_lock(&cluster->lock);
            if (!cluster->running)
            {
                pthread_mutex_unlock(&cluster->lock);
                break;
            }

            if (!cluster->heap.root)
            {
                pthread_mutex_unlock(&cluster->lock);
                break;
            }

            timernode_t *top = (timernode_t *)cluster->heap.root;
            struct timespec now = util_time_mono();
            if (time_less_than(&now, &top->expire_time))
            {
                pthread_mutex_unlock(&cluster->lock);
                break;
            }

            heap_dequeue(&cluster->heap);
            timernode_inn_t timernode_inn_v2 = {
                .timer_id = top->timer_id,
                .callback = top->callback,
                .userdata = top->userdata

            };
            LOG_PRINT_DEBUG("timer_id[%u] timeout", timernode_inn_v2.timer_id);

            if (top->timer_type == TIMER_TYPE_CYCLE)
            {
                top->expire_time = util_time_mono_after(top->timer_interval);
                heap_insert(&cluster->heap, &top->heap_node);
            }
            else
            {
                LOG_PRINT_INFO("timer_id[%u] del success", timernode_inn_v2.timer_id);
                rb_erase(&top->rb_node, &cluster->rb);
                free(top);
            }
            pthread_mutex_unlock(&cluster->lock);

            if (timernode_inn_v2.callback)
            {
                // todo thread pool
                timernode_inn_v2.callback(timernode_inn_v2.timer_id, timernode_inn_v2.userdata);
            }
        }

        pthread_mutex_lock(&cluster->lock);
        if (cluster->running)
        {
            timerfd_set_next_timeout(cluster->timer_fd, &cluster->heap);
        }
        pthread_mutex_unlock(&cluster->lock);
    }

    LOG_PRINT_INFO("timer cluster v2 stopped");
    return NULL;
}

timercluster_v2_t *timercluster_v2_init(void)
{
    timercluster_v2_t *cluster = calloc(1, sizeof(timercluster_v2_t));
    if (NULL == cluster)
    {
        LOG_PRINT_ERROR("calloc fail, errno[%d](%s)", errno, strerror(errno));
        return NULL;
    }

    cluster->timer_fd = -1;
    cluster->event_wfd = -1;
    cluster->event_rfd = -1;
    cluster->epoll_fd = -1;

    do
    {
        cluster->timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
        if (cluster->timer_fd < 0)
        {
            LOG_PRINT_ERROR("timerfd_create fail, errno[%d](%s)", errno, strerror(errno));
            break;
        }

        if (util_eventfd_open(&cluster->event_wfd, &cluster->event_rfd) != 0)
        {
            LOG_PRINT_ERROR("util_eventfd_open fail");
            break;
        }

        cluster->epoll_fd = epoll_create1(EPOLL_CLOEXEC);
        if (cluster->epoll_fd < 0)
        {
            LOG_PRINT_ERROR("epoll_create1 fail, errno[%d](%s)", errno, strerror(errno));
            break;
        }

        struct epoll_event ev = {};
        ev.events = EPOLLIN;
        ev.data.fd = cluster->timer_fd;
        if (epoll_ctl(cluster->epoll_fd, EPOLL_CTL_ADD, cluster->timer_fd, &ev) < 0)
        {
            LOG_PRINT_ERROR("epoll_ctl add timerfd fail, errno[%d](%s)", errno, strerror(errno));
            break;
        }

        ev.events = EPOLLIN;
        ev.data.fd = cluster->event_rfd;
        if (epoll_ctl(cluster->epoll_fd, EPOLL_CTL_ADD, cluster->event_rfd, &ev) < 0)
        {
            LOG_PRINT_ERROR("epoll_ctl add event_rfd fail, errno[%d](%s)", errno, strerror(errno));
            break;
        }

        heap_init(&cluster->heap, compare_timer);
        cluster->rb = RB_ROOT;
        pthread_mutexattr_t mutexattr = {};
        pthread_mutexattr_init(&mutexattr);
        (void)pthread_mutexattr_settype(&mutexattr, PTHREAD_MUTEX_ERRORCHECK);
        while ((pthread_mutex_init(&cluster->lock, &mutexattr) != 0) &&
               (pthread_mutex_init(&cluster->lock, NULL) != 0))
        {
            // We must have memory exhaustion -- ENOMEM, or
            // in some cases EAGAIN.  Wait a bit before we try to
            // give things a chance to settle down.
            util_msleep(10);
        }
        pthread_mutexattr_destroy(&mutexattr);
        cluster->running = true;
        pthread_create(&cluster->thread, NULL, timercluster_v2_thread_func, cluster);
        char thread_name[16] = {};
        snprintf(thread_name, sizeof(thread_name), "timercluster_v2");
        pthread_setname_np(cluster->thread, thread_name);
        LOG_PRINT_INFO("timer cluster v2 inited");

        return cluster;
    } while (0);

    if (cluster->event_wfd > 0 && cluster->event_rfd > 0)
    {
        util_eventfd_close(cluster->event_wfd, cluster->event_rfd);
    }

    if (cluster->timer_fd > 0)
    {
        close(cluster->timer_fd);
    }

    if (cluster->epoll_fd > 0)
    {
        close(cluster->epoll_fd);
    }

    if (NULL != cluster)
    {
        free(cluster);
        cluster = NULL;
    }

    return cluster;
}

void timercluster_v2_destroy(timercluster_v2_t *cluster)
{
    if (NULL == cluster)
    {
        LOG_PRINT_ERROR("invalid param!");
        return;
    }

    pthread_mutex_lock(&cluster->lock);
    cluster->running = false;
    pthread_mutex_unlock(&cluster->lock);

    timercluster_v2_wakeup(cluster);

    if (cluster->thread)
    {
        pthread_join(cluster->thread, NULL);
    }

    while (cluster->heap.root)
    {
        heap_dequeue(&cluster->heap);
    }

    struct rb_node *node = NULL;
    timernode_t *entry = NULL;
    while ((node = cluster->rb.rb_node))
    {
        entry = rb_entry(node, timernode_t, rb_node);
        rb_erase(node, &cluster->rb);
        free(entry);
        entry = NULL;
    }
    close(cluster->timer_fd);
    util_eventfd_close(cluster->event_wfd, cluster->event_rfd);
    close(cluster->epoll_fd);
    pthread_mutex_destroy(&cluster->lock);
    free(cluster);

    LOG_PRINT_WARN("timer cluster v2 destroyed");
}

bool timercluster_v2_timer_exist(timercluster_v2_t *cluster, timerid_t id)
{
    if (NULL == cluster)
    {
        LOG_PRINT_ERROR("invalid param!");
        return false;
    }

    pthread_mutex_lock(&cluster->lock);
    if (!cluster->running)
    {
        LOG_PRINT_ERROR("timercluster is not running!");
        pthread_mutex_unlock(&cluster->lock);
        return false;
    }

    bool exists = (rbtree_search(&cluster->rb, &id) != NULL) ? true : false;
    pthread_mutex_unlock(&cluster->lock);

    return exists;
}

bool timercluster_v2_timer_add(timercluster_v2_t *cluster, timerid_t id, timertype_e type, uint32_t interval, timercallback_t cb, void *userdata)
{
    if (NULL == cluster)
    {
        LOG_PRINT_ERROR("invalid param!");
        return false;
    }

    timernode_t *tnode = NULL;
    pthread_mutex_lock(&cluster->lock);
    if (!cluster->running)
    {
        LOG_PRINT_ERROR("timercluster is not running!");
        pthread_mutex_unlock(&cluster->lock);
        return false;
    }

    tnode = rbtree_search(&cluster->rb, &id);
    if (NULL != tnode)
    {
        LOG_PRINT_ERROR("timer_id[%u] is existed", id);
        pthread_mutex_unlock(&cluster->lock);
        return false;
    }

    tnode = (timernode_t *)calloc(1, sizeof(timernode_t));
    if (NULL == tnode)
    {
        LOG_PRINT_ERROR("calloc fail, errno[%d]", errno);
        pthread_mutex_unlock(&cluster->lock);
        return false;
    }

    tnode->timer_id = id;
    tnode->timer_type = type;
    tnode->timer_interval = interval;
    tnode->expire_time = util_time_mono_after(interval);
    tnode->callback = cb;
    tnode->userdata = userdata;
    if (0 != rbtree_insert(&cluster->rb, tnode))
    {
        LOG_PRINT_ERROR("rbtree_insert fail!");
        pthread_mutex_unlock(&cluster->lock);
        free(tnode);
        return false;
    }
    heap_insert(&cluster->heap, &tnode->heap_node);
    timercluster_v2_wakeup(cluster);
    pthread_mutex_unlock(&cluster->lock);

    LOG_PRINT_INFO("timer_id[%u] add success", id);

    return true;
}

bool timercluster_v2_timer_del(timercluster_v2_t *cluster, timerid_t id)
{
    if (NULL == cluster)
    {
        LOG_PRINT_ERROR("invalid param!");
        return false;
    }

    timernode_t *tnode = NULL;
    pthread_mutex_lock(&cluster->lock);
    if (!cluster->running)
    {
        LOG_PRINT_ERROR("timercluster is not running!");
        pthread_mutex_unlock(&cluster->lock);
        return false;
    }

    tnode = rbtree_search(&cluster->rb, &id);
    if (NULL == tnode)
    {
        LOG_PRINT_ERROR("timer_id[%u] is not existed or already deleted", id);
        pthread_mutex_unlock(&cluster->lock);
        return false;
    }

    rb_erase(&tnode->rb_node, &cluster->rb);
    heap_remove(&cluster->heap, &tnode->heap_node);
    free(tnode);
    timercluster_v2_wakeup(cluster);
    pthread_mutex_unlock(&cluster->lock);

    LOG_PRINT_INFO("timer_id[%u] del success", id);

    return true;
}

bool timercluster_v2_timer_reset(timercluster_v2_t *cluster, timerid_t id, uint32_t new_interval)
{
    if (NULL == cluster)
    {
        LOG_PRINT_ERROR("invalid param!");
        return false;
    }

    timernode_t *tnode = NULL;
    pthread_mutex_lock(&cluster->lock);
    if (!cluster->running)
    {
        LOG_PRINT_ERROR("timercluster is not running!");
        pthread_mutex_unlock(&cluster->lock);
        return false;
    }

    tnode = rbtree_search(&cluster->rb, &id);
    if (NULL == tnode)
    {
        LOG_PRINT_ERROR("timer_id[%u] is not existed or already deleted", id);
        pthread_mutex_unlock(&cluster->lock);
        return false;
    }

    tnode->timer_interval = new_interval;
    tnode->expire_time = util_time_mono_after(new_interval);

    heap_remove(&cluster->heap, &tnode->heap_node);
    heap_insert(&cluster->heap, &tnode->heap_node);
    timercluster_v2_wakeup(cluster);
    pthread_mutex_unlock(&cluster->lock);

    return true;
}

const char *timercluster_v2_strerror(int err)
{
    switch (err)
    {
#define X(code, name, msg)        \
    case TIMERCLUSTER_RET_##name: \
        return msg;
        TIMERCLUSTER_FOREACH_ERR(X)
#undef X
        default:
            return "Unknown timer cluster v2 error";
    }
}
