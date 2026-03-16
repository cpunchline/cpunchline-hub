#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <assert.h>
#include <time.h>

#include "utility/objectpool.h"

// ---------------------------------------------------------------------------
// 1. 定义测试用的数据结构
// ---------------------------------------------------------------------------

typedef struct
{
    int id;
    char data[64];
    int ref_count;
} TestObject;

typedef struct
{
    double x, y, z;
    int type;
} Vector3;

// 使用宏生成特定的内存池类型
// 参数: 类型名, 最大数量
DEFINE_FIXED_OBJECT_POOL(TestObject, 10);   // 小池,容易测满
DEFINE_FIXED_OBJECT_POOL(Vector3, 100);     // 大池
DEFINE_FIXED_MEM_POOL(RawBytePool, 32, 50); // 原始字节池,块大小32字节,共50块

// ---------------------------------------------------------------------------
// 2. 辅助函数:打印分隔线
// ---------------------------------------------------------------------------
void print_section(const char *title)
{
    printf("\n========================================\n");
    printf("  TEST: %s\n", title);
    printf("========================================\n");
}

// ---------------------------------------------------------------------------
// 3. 基础功能测试 (单线程)
// ---------------------------------------------------------------------------
void test_basic_lifecycle()
{
    print_section("Basic Lifecycle (Alloc/Free/Stats)");

    static TestObjectPool pool;
    TestObjectPool_Init(&pool);

    printf("Initial State:\n");
    TestObjectPool_Dump(&pool);
    assert(TestObjectPool_FreeCount(&pool) == 10);
    assert(TestObjectPool_UsedCount(&pool) == 0);

    // 1. 分配一个对象
    TestObject *obj1 = TestObjectPool_Create(&pool);
    assert(obj1 != NULL);
    obj1->id = 1001;
    strcpy(obj1->data, "Hello Pool");
    obj1->ref_count = 1;
    printf("Allocated obj1 (ID: %d, Data: %s)\n", obj1->id, obj1->data);

    assert(TestObjectPool_UsedCount(&pool) == 1);
    assert(TestObjectPool_Owns(&pool, obj1) == true);

    // 2. 分配多个对象直到池满
    TestObject *objs[9];
    for (int i = 0; i < 9; i++)
    {
        objs[i] = TestObjectPool_Create(&pool);
        assert(objs[i] != NULL);
        objs[i]->id = 1002 + i;
    }

    printf("Allocated 9 more objects.\n");
    TestObjectPool_Dump(&pool);
    assert(TestObjectPool_FreeCount(&pool) == 0);
    assert(TestObjectPool_UsedCount(&pool) == 10);

    // 3. 测试池满情况 (应返回 NULL)
    TestObject *obj_overflow = TestObjectPool_Create(&pool);
    (void)obj_overflow;
    assert(obj_overflow == NULL);
    printf("Correctly returned NULL when pool is full.\n");

    // 4. 释放一个对象
    TestObjectPool_Destroy(&pool, obj1);
    printf("Destroyed obj1.\n");
    assert(TestObjectPool_FreeCount(&pool) == 1);
    assert(TestObjectPool_UsedCount(&pool) == 9);

    // 5. 再次分配 (应复用刚才释放的空间)
    TestObject *obj_new = TestObjectPool_Create(&pool);
    assert(obj_new != NULL);
    printf("Re-allocated object successfully.\n");
    assert(TestObjectPool_FreeCount(&pool) == 0);

    // 6. 释放所有
    for (int i = 0; i < 9; i++)
    {
        TestObjectPool_Destroy(&pool, objs[i]);
    }
    TestObjectPool_Destroy(&pool, obj_new);

    assert(TestObjectPool_FreeCount(&pool) == 10);
    assert(TestObjectPool_UsedCount(&pool) == 0);
    printf("All objects freed. Pool reset.\n");
    TestObjectPool_Dump(&pool);
}

// ---------------------------------------------------------------------------
// 4. 所有权与错误处理测试
// ---------------------------------------------------------------------------
void test_ownership_and_errors()
{
    print_section("Ownership & Error Handling");

    static Vector3Pool pool;
    Vector3Pool_Init(&pool);

    Vector3 *v1 = Vector3Pool_Create(&pool);
    assert(v1 != NULL);
    v1->x = 1.0;
    v1->y = 2.0;
    v1->z = 3.0;

    // 1. 测试合法所有权
    assert(Vector3Pool_Owns(&pool, v1) == true);
    printf("Owns valid pointer: PASS\n");

    // 2. 测试非法所有权 (栈变量)
    Vector3 stack_var;
    assert(Vector3Pool_Owns(&pool, &stack_var) == false);
    printf("Owns stack variable: PASS (Correctly rejected)\n");

    // 3. 测试非法所有权 (NULL)
    assert(Vector3Pool_Owns(&pool, NULL) == false);
    printf("Owns NULL: PASS (Correctly rejected)\n");

    // 4. 测试释放 NULL (应不崩溃,无操作)
    Vector3Pool_Destroy(&pool, NULL);
    printf("Free NULL: PASS (No crash)\n");

    // 5. 测试释放非法指针 (应警告或忽略,不崩溃)
    // 注意:我们的实现会打印警告并返回
    Vector3Pool_Destroy(&pool, &stack_var);
    printf("Free invalid pointer: PASS (Handled safely)\n");

    // 清理
    Vector3Pool_Destroy(&pool, v1);
}

// ---------------------------------------------------------------------------
// 5. 原始字节池测试
// ---------------------------------------------------------------------------
void test_raw_memory_pool()
{
    print_section("Raw Memory Pool (Byte Array)");

    static RawBytePool pool;
    RawBytePool_Init(&pool);

    printf("Capacity: %u, BlockSize: %u\n", RawBytePool_FreeCount(&pool), (unsigned int)32);

    // 分配
    void *mem1 = RawBytePool_Allocate(&pool);
    void *mem2 = RawBytePool_Allocate(&pool);

    assert(mem1 != NULL);
    assert(mem2 != NULL);
    assert(mem1 != mem2); // 地址应该不同

    // 写入数据
    memset(mem1, 'A', 32);
    memset(mem2, 'B', 32);

    // 验证数据未损坏
    assert(((char *)mem1)[0] == 'A');
    assert(((char *)mem2)[0] == 'B');
    printf("Data integrity check: PASS\n");

    // 释放
    RawBytePool_Free(&pool, mem1);
    RawBytePool_Free(&pool, mem2);

    assert(RawBytePool_FreeCount(&pool) == 50);
    printf("Raw pool test completed.\n");
}

// ---------------------------------------------------------------------------
// 6. 多线程压力测试
// ---------------------------------------------------------------------------

#define THREAD_COUNT   4
#define OPS_PER_THREAD 500

typedef struct
{
    Vector3Pool *pool;
    int thread_id;
    int success_count;
    int fail_count;
} ThreadArg;

void *worker_thread(void *arg)
{
    ThreadArg *t_arg = (ThreadArg *)arg;
    Vector3 *objs[OPS_PER_THREAD];
    memset(objs, 0, sizeof(objs));

    srand((unsigned int)time(NULL));

    for (int i = 0; i < OPS_PER_THREAD; i++)
    {
        // 随机操作:0=分配, 1=释放(如果有已分配的)
        int op = rand() % 2;

        if (op == 0)
        {
            // Allocate
            Vector3 *v = Vector3Pool_Create(t_arg->pool);
            if (v)
            {
                v->x = (double)i;
                v->type = t_arg->thread_id;
                // 找一个空位存起来以便后续释放
                for (int k = 0; k < OPS_PER_THREAD; k++)
                {
                    if (objs[k] == NULL)
                    {
                        objs[k] = v;
                        break;
                    }
                }
                t_arg->success_count++;
            }
            else
            {
                t_arg->fail_count++; // 池满
            }
        }
        else
        {
            // Free (找一个已分配的释放)
            for (int k = 0; k < OPS_PER_THREAD; k++)
            {
                if (objs[k] != NULL)
                {
                    Vector3Pool_Destroy(t_arg->pool, objs[k]);
                    objs[k] = NULL;
                    break;
                }
            }
        }

        // 微小延迟,增加交织概率
        // usleep(10);
    }

    // 清理剩余未释放的
    for (int k = 0; k < OPS_PER_THREAD; k++)
    {
        if (objs[k] != NULL)
        {
            Vector3Pool_Destroy(t_arg->pool, objs[k]);
        }
    }

    return NULL;
}

void test_multithreading()
{
    print_section("Multithreading Stress Test");

    static Vector3Pool pool;
    Vector3Pool_Init(&pool);

    printf("Starting %d threads, each doing %d ops...\n", THREAD_COUNT, OPS_PER_THREAD);
    printf("capacity: %u", Vector3Pool_Capacity(&pool));

    pthread_t threads[THREAD_COUNT];
    ThreadArg args[THREAD_COUNT];

    clock_t start = clock();

    for (int i = 0; i < THREAD_COUNT; i++)
    {
        args[i].pool = &pool;
        args[i].thread_id = i;
        args[i].success_count = 0;
        args[i].fail_count = 0;
        pthread_create(&threads[i], NULL, worker_thread, &args[i]);
    }

    for (int i = 0; i < THREAD_COUNT; i++)
    {
        pthread_join(threads[i], NULL);
    }

    clock_t end = clock();
    double time_spent = (double)(end - start) / CLOCKS_PER_SEC;

    // 汇总结果
    int total_success = 0;
    int total_fail = 0;
    for (int i = 0; i < THREAD_COUNT; i++)
    {
        total_success += args[i].success_count;
        total_fail += args[i].fail_count;
    }

    printf("\n--- Results ---\n");
    printf("Time elapsed: %.4f seconds\n", time_spent);
    printf("Total Alloc Attempts: %d\n", total_success + total_fail); // 近似,因为一半操作可能是free
    printf("Successful Allocs: %d\n", total_success);
    printf("Failed Allocs (Pool Full): %d\n", total_fail);

    // 最终状态检查
    uint32_t used = Vector3Pool_UsedCount(&pool);
    uint32_t free = Vector3Pool_FreeCount(&pool);

    printf("Final Pool State: Used=%u, Free=%u\n", used, free);

    // 由于线程最后都清理了各自持有的对象,最终 Used 应该为 0
    if (used == 0 && free == Vector3Pool_Capacity(&pool))
    {
        printf("Thread Safety Check: PASS (No leaks, counts match)\n");
    }
    else
    {
        printf("Thread Safety Check: FAIL (Leak detected or count mismatch!)\n");
        Vector3Pool_Dump(&pool);
        exit(1);
    }
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int main()
{
    printf("Starting FixedMemPool/FixedObjectPool Test Suite\n");
    printf("C Version Implementation\n");

    test_basic_lifecycle();
    test_ownership_and_errors();
    test_raw_memory_pool();
    test_multithreading();

    printf("\n========================================\n");
    printf("  ALL TESTS PASSED SUCCESSFULLY!\n");
    printf("========================================\n");

    return 0;
}
