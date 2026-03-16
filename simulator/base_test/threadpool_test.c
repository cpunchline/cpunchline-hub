#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <stdatomic.h>
#include <time.h>
#include <errno.h>
#include <string.h>

#include "utility/threadpool.h"

// 全局计数器,用于验证任务执行
atomic_int g_task_counter = 0;
atomic_int g_malloc_task_counter = 0;
int local_ids[5][3] = {0};
int task_ids[8] = {0};
static int id = 0;
static int id2 = 0;

// ------------------------ 测试任务函数 ------------------------

// 普通任务:接收一个整数指针
static void task_function(void *arg)
{
    int task_id = *(int *)arg;
    // 模拟一些工作(100~300ms)
    usleep(100000 + (unsigned)rand() % 200000);
    atomic_fetch_add(&g_task_counter, 1);
    printf("[%ld]-Task[%d] executed\n", pthread_self(), task_id);
}

// 需要释放参数的任务
static void task_with_malloc_arg(void *arg)
{
    char *msg = (char *)arg;
    // 模拟一些工作(100~300ms)
    usleep(100000 + (unsigned)rand() % 200000);
    atomic_fetch_add(&g_malloc_task_counter, 1);
    printf("[%ld]-Received message: %s executed\n", pthread_self(), msg);
    // 注意:线程池会自动 free(arg) 因为 arg_type == 1
}

// 空任务,用于测试 NULL 参数
static void null_task(void *arg)
{
    printf("[%ld]-arg=%p\n", pthread_self(), arg);
}

// ------------------------ 工具函数 ------------------------

// 等待指定毫秒
static void wait_ms(int ms)
{
    usleep((unsigned)ms * 1000);
}

// ------------------------ 测试用例 ------------------------

static void test_create_destroy(void)
{
    g_task_counter = 0;
    printf("\n=== Test 1: Create and Destroy Thread Pool ===\n");
    fix_threadpool_t *pool = fix_threadpool_create(3, 10);
    if (!pool)
    {
        printf("Failed to create thread pool!\n");
        return;
    }
    printf("Thread pool created with 3 threads, queue size 10.\n");

    // 等待线程启动
    wait_ms(100);

    int ret = fix_threadpool_destroy(pool);
    printf("fix_threadpool_destroy returned: %d\n", ret);
}

static void test_submit_normal_tasks(void)
{
    g_task_counter = 0;
    printf("\n=== Test 2: Submit 8 Normal Tasks ===\n");
    fix_threadpool_t *pool = fix_threadpool_create(4, 20);
    if (!pool)
    {
        printf("Pool creation failed!\n");
        return;
    }

    for (int i = 0; i < 8; i++)
    {
        task_ids[i] = i + 100;
        int ret = fix_threadpool_addtask(pool, task_function, &task_ids[i]);
        if (ret != 0)
        {
            printf("Failed to add task %d, ret=%d\n", i, ret);
        }
        else
        {
            printf("Submitted task %d\n", task_ids[i]);
        }
    }

    // 等待足够时间让任务完成
    wait_ms(600);

    printf("Expected: 8 tasks, Actual: %d tasks executed.\n", g_task_counter);

    fix_threadpool_destroy(pool);
}

fix_threadpool_t *submitter_pool = NULL;

// 线程函数:提交3个任务
static void *submitter_func(void *arg)
{
    long thread_id = (long)arg;
    printf("thread_id=%ld\n", thread_id);

    for (int i = 0; i < 3; i++)
    {
        local_ids[thread_id][i] = (int)(thread_id * 10 + i);
        fix_threadpool_addtask(submitter_pool, task_function, &local_ids[thread_id][i]);
    }
    return NULL;
}

static void test_concurrent_submission(void)
{
    g_task_counter = 0;
    printf("\n=== Test 4: Concurrent Task Submission from 5 threads ===\n");

    submitter_pool = fix_threadpool_create(3, 15);
    if (!submitter_pool)
    {
        printf("Pool creation failed!\n");
        return;
    }

    // 多个线程同时提交任务
    pthread_t submitters[5];

    // 创建5个提交线程
    for (long i = 0; i < 5; i++)
    {
        pthread_create(&submitters[i], NULL, submitter_func, (void *)i);
    }

    // 等待所有提交线程完成
    for (int i = 0; i < 5; i++)
    {
        pthread_join(submitters[i], NULL);
    }

    wait_ms(1500);
    printf("Total tasks executed: %d (expected 15)\n", g_task_counter);

    fix_threadpool_destroy(submitter_pool);
}

static void test_reject_after_destroy(void)
{
    printf("\n=== Test 5: Reject Tasks After Destroy ===\n");
    fix_threadpool_t *pool = fix_threadpool_create(2, 5);
    if (!pool)
    {
        printf("Create failed!\n");
        return;
    }

    // 提交一个任务
    fix_threadpool_addtask(pool, task_function, &id);

    // 立即销毁
    fix_threadpool_destroy(pool);
    printf("Pool destroyed.\n");

    // 尝试添加新任务 → 应该失败
    int ret = fix_threadpool_addtask(pool, task_function, &id2);
    printf("Add task after destroy: ret = %d (expected -1)\n", ret);
}

static void test_edge_cases(void)
{
    printf("\n=== Test 6: Edge Cases (NULL checks) ===\n");

    // 测试 NULL pool
    int ret1 = fix_threadpool_addtask(NULL, task_function, &id);
    printf("AddTask(NULL_pool): %d (expected -1)\n", ret1);

    // 测试 NULL function
    fix_threadpool_t *pool = fix_threadpool_create(1, 1);
    int ret2 = fix_threadpool_addtask(pool, NULL, &id);
    printf("AddTask(NULL_func): %d (expected -1)\n", ret2);

    // 测试 NULL pool in destroy
    int ret3 = fix_threadpool_destroy(NULL);
    printf("Destroy(NULL): %d (expected -1)\n", ret3);

    // 正常销毁
    fix_threadpool_destroy(pool);
}

// ------------------------ 主函数 ------------------------

int main(void)
{
    srand((unsigned int)time(NULL));
    printf("🚀 Starting C Thread Pool Tests...\n");

    test_create_destroy();
    wait_ms(50);

    test_submit_normal_tasks();
    wait_ms(50);

    test_concurrent_submission();
    wait_ms(50);

    test_reject_after_destroy();
    wait_ms(50);

    test_edge_cases();

    printf("\n🎉 All C tests completed successfully!\n");
    return 0;
}