#include <stdio.h>
#include <unistd.h>

#include "utility/utils.h"
#include "utility/timercluster.h"

static void test_single_callback(timerid_t id, void *userdata)
{
    (void)userdata;
    LOG_PRINT_INFO("Timer[%u] triggered: %s", id, "test_single");
}

static void test_cycle_callback(timerid_t id, void *userdata)
{
    (void)userdata;
    LOG_PRINT_INFO("Timer[%u] triggered: %s", id, "test_cycle");
}

static void test_v2_single_callback(timerid_t id, void *userdata)
{
    (void)userdata;
    LOG_PRINT_INFO("v2 Timer[%u] triggered: %s", id, "test_single");
}

static void test_v2_cycle_callback(timerid_t id, void *userdata)
{
    (void)userdata;
    LOG_PRINT_INFO("v2 Timer[%u] triggered: %s", id, "test_cycle");
}

static void test_timercluster(void)
{
    timercluster_t *cluster = NULL;
    cluster = timercluster_init();
    if (NULL == cluster)
    {
        LOG_PRINT_ERROR("timercluster_init fail!");
        return;
    }

    timercluster_timer_add(cluster, 0, TIMER_TYPE_SINGLE, 2000, test_single_callback, NULL);
    timercluster_timer_add(cluster, 1, TIMER_TYPE_CYCLE, 1000, test_cycle_callback, NULL);

    sleep(5); // 让定时器运行一段时间

    timercluster_timer_del(cluster, 0);
    timercluster_timer_del(cluster, 1);
    timercluster_destroy(cluster);
}

static void test_timercluster_v2(void)
{
    timercluster_v2_t *cluster = NULL;
    cluster = timercluster_v2_init();
    if (NULL == cluster)
    {
        LOG_PRINT_ERROR("timercluster_v2_init fail!");
        return;
    }

    timercluster_v2_timer_add(cluster, 0, TIMER_TYPE_SINGLE, 2000, test_v2_single_callback, NULL);
    timercluster_v2_timer_add(cluster, 1, TIMER_TYPE_CYCLE, 1000, test_v2_cycle_callback, NULL);

    sleep(5); // 让定时器运行一段时间

    timercluster_v2_timer_del(cluster, 0);
    timercluster_v2_timer_del(cluster, 1);
    timercluster_v2_destroy(cluster);
}

static timercluster_t *g_cluster_v1 = NULL;
static timercluster_v2_t *g_cluster_v2 = NULL;

static void test_v1_self_del_cb(timerid_t id, void *userdata)
{
    (void)userdata;
    LOG_PRINT_INFO("v1: Timer[%u] self-deleting...", id);
    timercluster_timer_del(g_cluster_v1, id); // 删除自己
    LOG_PRINT_INFO("v1: Timer[%u] self-delete done.", id);
}

static void test_v2_self_del_cb(timerid_t id, void *userdata)
{
    (void)userdata;
    LOG_PRINT_INFO("v2: Timer[%u] self-deleting...", id);
    timercluster_v2_timer_del(g_cluster_v2, id);
    LOG_PRINT_INFO("v2: Timer[%u] self-delete done.", id);
}

static void test_v1_self_delete(void)
{
    LOG_PRINT_INFO("=== Test v1: Self-delete ===");
    g_cluster_v1 = timercluster_init();
    if (!g_cluster_v1)
        return;

    // 周期性定时器:期望只触发一次(因为 self-del)
    timercluster_timer_add(g_cluster_v1, 10, TIMER_TYPE_CYCLE, 500, test_v1_self_del_cb, NULL);

    sleep(2); // 应触发 ~4 次?但期望只触发 1 次!

    timercluster_destroy(g_cluster_v1);
    g_cluster_v1 = NULL;
}

static void test_v2_self_delete(void)
{
    LOG_PRINT_INFO("=== Test v2: Self-delete ===");
    g_cluster_v2 = timercluster_v2_init();
    if (!g_cluster_v2)
        return;

    timercluster_v2_timer_add(g_cluster_v2, 20, TIMER_TYPE_CYCLE, 500, test_v2_self_del_cb, NULL);

    sleep(2);

    timercluster_v2_destroy(g_cluster_v2);
    g_cluster_v2 = NULL;
}

// v1
static void test_v1_del_future_cb(timerid_t id, void *userdata)
{
    (void)userdata;
    LOG_PRINT_INFO("v1: Timer[%u] deleting future timer[99]...", id);
    timercluster_timer_del(g_cluster_v1, 99); // 删除一个 3s 后才到期的定时器
    LOG_PRINT_INFO("v1: delete future timer done.");
}

static void test_v1_cb_3s(timerid_t id, void *userdata)
{
    (void)userdata;
    LOG_PRINT_INFO("v1: Future timer[%u] SHOULD NOT trigger!", id);
}

static void test_v1_delete_future(void)
{
    LOG_PRINT_INFO("=== Test v1: Delete future timer ===");
    g_cluster_v1 = timercluster_init();
    if (!g_cluster_v1)
        return;

    timercluster_timer_add(g_cluster_v1, 88, TIMER_TYPE_SINGLE, 1000, test_v1_del_future_cb, NULL);
    timercluster_timer_add(g_cluster_v1, 99, TIMER_TYPE_SINGLE, 3000, test_v1_cb_3s, NULL);

    sleep(4); // 99 应该被 88 的回调删除,不会触发

    timercluster_destroy(g_cluster_v1);
    g_cluster_v1 = NULL;
}

// v2
static void test_v2_del_future_cb(timerid_t id, void *userdata)
{
    (void)userdata;
    LOG_PRINT_INFO("v2: Timer[%u] deleting future timer[199]...", id);
    timercluster_v2_timer_del(g_cluster_v2, 199);
    LOG_PRINT_INFO("v2: delete future timer done.");
}

static void test_v2_cb_3s(timerid_t id, void *userdata)
{
    (void)userdata;
    LOG_PRINT_INFO("v2: Future timer[%u] SHOULD NOT trigger!", id);
}

static void test_v2_delete_future(void)
{
    LOG_PRINT_INFO("=== Test v2: Delete future timer ===");
    g_cluster_v2 = timercluster_v2_init();
    if (!g_cluster_v2)
        return;

    timercluster_v2_timer_add(g_cluster_v2, 188, TIMER_TYPE_SINGLE, 1000, test_v2_del_future_cb, NULL);
    timercluster_v2_timer_add(g_cluster_v2, 199, TIMER_TYPE_SINGLE, 3000, test_v2_cb_3s, NULL);

    sleep(4);

    timercluster_v2_destroy(g_cluster_v2);
    g_cluster_v2 = NULL;
}

static void test_v2_del_expired_pending_cb_A(timerid_t id, void *userdata)
{
    (void)userdata;
    LOG_PRINT_INFO("v2: Timer[A=%u] deleting pending expired timer[B=302]...", id);
    timercluster_v2_timer_del(g_cluster_v2, 302); // 删除 B
    LOG_PRINT_INFO("v2: delete pending timer done.");
}

static void test_v2_del_expired_pending_cb_B(timerid_t id, void *userdata)
{
    (void)userdata;
    LOG_PRINT_INFO("v2: Timer[B=%u] triggered! (This should NOT happen if del works)", id);
}

static void test_v2_delete_expired_pending(void)
{
    LOG_PRINT_INFO("=== Test v2: Delete expired-but-pending timer ===");
    g_cluster_v2 = timercluster_v2_init();
    if (!g_cluster_v2)
        return;

    // 两个定时器同时到期
    timercluster_v2_timer_add(g_cluster_v2, 301, TIMER_TYPE_SINGLE, 1000, test_v2_del_expired_pending_cb_A, NULL);
    timercluster_v2_timer_add(g_cluster_v2, 302, TIMER_TYPE_SINGLE, 1000, test_v2_del_expired_pending_cb_B, NULL);

    sleep(2); // 两者应同时到期,A 在回调中删 B

    // 预期:B 不应触发!
    timercluster_v2_destroy(g_cluster_v2);
    g_cluster_v2 = NULL;
}

int main(void)
{
    test_timercluster();
    test_timercluster_v2();
    test_v1_self_delete();
    test_v2_self_delete();
    test_v1_delete_future();
    test_v2_delete_future();
    test_v2_delete_expired_pending();

    return 0;
}
