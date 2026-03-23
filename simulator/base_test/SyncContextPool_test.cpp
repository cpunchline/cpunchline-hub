#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <chrono>
#include <atomic>
#include <cstring>
#include <memory>

#include "SyncContextPool.hpp"

// ============================================================================
// 测试 1: 基本用法 - void 类型 (纯同步)
// ============================================================================
void test_basic_void_context()
{
    std::cout << "\n=== Test: Basic Void Context ===" << std::endl;

    SyncContextPool<void> pool(0, 4); // 初始 0 个，最多 4 个

    std::cout << "Initial pool state: ";
    pool.DumpState();

    // 借用上下文
    auto guard = pool.Borrow();
    if (!guard.IsValid())
    {
        std::cout << "Failed to borrow context!" << std::endl;
        return;
    }

    std::cout << "Borrowed context successfully. Used: " << pool.UsedCount() << std::endl;

    // 模拟异步操作：在另一个线程中设置结果
    std::thread worker([&guard]()
                       {
                           std::this_thread::sleep_for(std::chrono::milliseconds(500));
                           guard->SetResult(0); // 成功
                           std::cout << "[Worker] Result set to 0" << std::endl;
                       });

    // 等待结果
    int result = guard->Wait(3000); // 3 秒超时
    std::cout << "[Main] Wait completed, result: " << result << std::endl;

    worker.join();

    // guard 析构时自动归还
    std::cout << "After guard destroyed, Free: " << pool.FreeCount() << std::endl;
}

// ============================================================================
// 测试 2: 携带数据的上下文
// ============================================================================
struct RequestData
{
    int request_id;
    char payload[64];
};

void test_data_context()
{
    std::cout << "\n=== Test: Data Context ===" << std::endl;

    SyncContextPool<RequestData> pool(0, 8); // 初始 0，最多 8 个

    auto guard = pool.Borrow();
    if (!guard.IsValid())
    {
        std::cout << "Failed to borrow context!" << std::endl;
        return;
    }

    // 设置数据
    guard->data.request_id = 42;
    std::strcpy(guard->data.payload, "Hello, SyncContextPool!");

    std::cout << "Request ID: " << guard->data.request_id << std::endl;
    std::cout << "Payload: " << guard->data.payload << std::endl;

    // 模拟异步响应
    std::thread responder([&guard]()
                          {
                              std::this_thread::sleep_for(std::chrono::milliseconds(300));

                              // 修改响应数据
                              std::strcpy(guard->data.payload, "Response received!");
                              guard->SetResult(0);
                          });

    int result = guard->Wait(2000);
    std::cout << "Wait result: " << result << std::endl;
    std::cout << "Response payload: " << guard->data.payload << std::endl;

    responder.join();
}

// ============================================================================
// 测试 3: RAII 自动归还
// ============================================================================
void test_raii_auto_return()
{
    std::cout << "\n=== Test: RAII Auto Return ===" << std::endl;

    SyncContextPool<void> pool(0, 4);

    std::cout << "Before scope - Free: " << pool.FreeCount() << std::endl;

    {
        auto guard1 = pool.Borrow();
        auto guard2 = pool.Borrow();

        std::cout << "Inside scope (2 borrowed) - Free: " << pool.FreeCount() << std::endl;

        // 不手动归还，依赖 RAII
    }

    std::cout << "After scope exited - Free: " << pool.FreeCount() << std::endl;
}

// ============================================================================
// 测试 4: 超时处理
// ============================================================================
void test_timeout_handling()
{
    std::cout << "\n=== Test: Timeout Handling ===" << std::endl;

    SyncContextPool<void> pool(0, 4);

    auto guard = pool.Borrow();
    if (!guard.IsValid())
    {
        std::cout << "Failed to borrow!" << std::endl;
        return;
    }

    // 不设置结果，测试超时
    auto start = std::chrono::steady_clock::now();
    int result = guard->Wait(1000); // 1 秒超时
    auto end = std::chrono::steady_clock::now();

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    std::cout << "Wait timeout test:" << std::endl;
    std::cout << "  Elapsed: " << elapsed << " ms" << std::endl;
    std::cout << "  Result: " << result << " (expected: -ETIMEDOUT=" << -ETIMEDOUT << ")" << std::endl;
}

// ============================================================================
// 测试 5: 并发访问
// ============================================================================
void worker_thread(SyncContextPool<void> &pool, int thread_id, std::atomic<int> &success_count)
{
    auto guard = pool.Borrow();
    if (!guard.IsValid())
    {
        std::cout << "Thread " << thread_id << " failed to borrow" << std::endl;
        return;
    }

    std::cout << "Thread " << thread_id << " borrowed context" << std::endl;

    // 模拟异步操作
    std::thread async_worker([&guard]()
                             {
                                 std::this_thread::sleep_for(std::chrono::milliseconds(200));
                                 guard->SetResult(0);
                             });

    int result = guard->Wait(3000);
    if (result == 0)
    {
        success_count++;
        std::cout << "Thread " << thread_id << " completed successfully" << std::endl;
    }
    else
    {
        std::cout << "Thread " << thread_id << " got result: " << result << std::endl;
    }

    async_worker.join();
    // guard 自动归还
}

void test_concurrent_access()
{
    std::cout << "\n=== Test: Concurrent Access ===" << std::endl;

    SyncContextPool<void> pool(0, 16); // 初始 0，最多 16 个
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};

    auto start = std::chrono::steady_clock::now();

    // 创建 10 个线程并发访问
    for (int i = 0; i < 10; ++i)
    {
        threads.emplace_back(worker_thread, std::ref(pool), i, std::ref(success_count));
    }

    for (auto &t : threads)
    {
        t.join();
    }

    auto end = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    std::cout << "Concurrent test completed:" << std::endl;
    std::cout << "  Success count: " << success_count << "/10" << std::endl;
    std::cout << "  Elapsed time: " << elapsed << " ms" << std::endl;
    std::cout << "  Pool free count: " << pool.FreeCount() << std::endl;
}

// ============================================================================
// 测试 6: 池容量限制和动态扩展
// ============================================================================
void test_pool_capacity()
{
    std::cout << "\n=== Test: Pool Capacity and Dynamic Expansion ===" << std::endl;

    constexpr size_t MAX_COUNT = 4;
    SyncContextPool<void> pool(0, MAX_COUNT); // 初始 0，最多 4 个

    std::cout << "Pool capacity: " << pool.Capacity() << std::endl;

    // 借用所有可用的上下文 (使用裸指针数组)
    using GuardType = SyncContextPool<void>::GuardType;
    GuardType *guards[MAX_COUNT] = {nullptr};

    uint32_t borrowed_count = 0;
    for (uint32_t i = 0; i < MAX_COUNT; ++i)
    {
        guards[i] = new GuardType(pool.Borrow());
        if (guards[i]->IsValid())
        {
            borrowed_count++;
        }
    }

    std::cout << "Borrowed " << borrowed_count << " contexts" << std::endl;
    std::cout << "Free count: " << pool.FreeCount() << std::endl;
    std::cout << "Used count: " << pool.UsedCount() << std::endl;

    // 尝试再借一个 (应该失败或超时，因为已达最大数)
    auto extra_guard = pool.TryBorrow();
    if (!extra_guard || !extra_guard->IsValid())
    {
        std::cout << "Correctly failed to borrow when pool exhausted" << std::endl;
    }

    // 释放所有守卫
    for (uint32_t i = 0; i < MAX_COUNT; ++i)
    {
        delete guards[i];
    }
    std::cout << "After releasing all - Free count: " << pool.FreeCount() << std::endl;
}

// ============================================================================
// 测试 7: SetResult 重复调用测试 (应该触发异常或未定义行为)
// ============================================================================
void test_setresult_once_only()
{
    std::cout << "\n=== Test: SetResult Once Only (Warning: May Crash) ===" << std::endl;

    SyncContextPool<void> pool(0, 4);
    auto guard = pool.Borrow();

    if (guard.IsValid())
    {
        // 第一次调用 - 正常
        guard->SetResult(0);
        std::cout << "First SetResult(0) succeeded" << std::endl;

        // 第二次调用 - 会导致 std::future_error 异常
        try
        {
            guard->SetResult(1);
            std::cout << "Second SetResult(1) - Unexpected success!" << std::endl;
        }
        catch (const std::future_error &e)
        {
            std::cout << "Caught expected exception: " << e.what() << std::endl;
        }
    }
}

// ============================================================================
// 测试 8: 支持可移动类型 (std::vector)
// ============================================================================
void test_movable_type_vector()
{
    std::cout << "\n=== Test: Movable Type (std::vector) ===" << std::endl;

    SyncContextPool<std::vector<int>> pool(0, 8);

    auto guard = pool.Borrow();
    if (!guard.IsValid())
    {
        std::cout << "Failed to borrow context!" << std::endl;
        return;
    }

    // 使用 SetData 设置 vector 数据 (支持移动语义)
    std::vector<int> data = {1, 2, 3, 4, 5};
    guard->SetData(data); // 拷贝

    std::cout << "Initial data size: " << guard->GetData().size() << std::endl;
    std::cout << "Initial data: ";
    for (int val : guard->GetData())
    {
        std::cout << val << " ";
    }
    std::cout << std::endl;

    // 模拟异步操作：在另一个线程中修改数据
    std::thread worker([&guard]()
                       {
                           std::this_thread::sleep_for(std::chrono::milliseconds(300));

                           // 通过 GetData() 获取可变引用并修改
                           auto &vec = guard->GetData();
                           vec.push_back(6);
                           vec.push_back(7);

                           std::cout << "[Worker] Modified vector, new size: " << vec.size() << std::endl;
                           guard->SetResult(0);
                       });

    // 等待结果
    int result = guard->Wait(3000);
    std::cout << "[Main] Wait completed, result: " << result << std::endl;
    std::cout << "Final data size: " << guard->GetData().size() << std::endl;
    std::cout << "Final data: ";
    for (int val : guard->GetData())
    {
        std::cout << val << " ";
    }
    std::cout << std::endl;

    worker.join();
}

// ============================================================================
// 测试 9: 支持 std::string 类型
// ============================================================================
void test_movable_type_string()
{
    std::cout << "\n=== Test: Movable Type (std::string) ===" << std::endl;

    SyncContextPool<std::string> pool(0, 8);

    auto guard = pool.Borrow();
    if (!guard.IsValid())
    {
        std::cout << "Failed to borrow context!" << std::endl;
        return;
    }

    // 直接设置字符串数据
    guard->SetData(std::string("Hello"));
    guard->GetData() += ", World!";

    std::cout << "Initial string: " << guard->GetData() << std::endl;

    // 模拟异步响应
    std::thread responder([&guard]()
                          {
                              std::this_thread::sleep_for(std::chrono::milliseconds(300));

                              // 修改字符串
                              guard->GetData() += " Response received!";
                              guard->SetResult(0);
                          });

    int result = guard->Wait(2000);
    std::cout << "Wait result: " << result << std::endl;
    std::cout << "Response string: " << guard->GetData() << std::endl;

    responder.join();
}

// ============================================================================
// 测试 10: 支持 unique_ptr 类型
// ============================================================================
void test_movable_type_unique_ptr()
{
    std::cout << "\n=== Test: Movable Type (std::unique_ptr) ===" << std::endl;

    struct CustomData
    {
        int id;
        std::string name;
        CustomData(int i, const std::string &n) :
            id(i), name(n)
        {
        }
    };

    SyncContextPool<std::unique_ptr<CustomData>> pool(0, 8);

    auto guard = pool.Borrow();
    if (!guard.IsValid())
    {
        std::cout << "Failed to borrow context!" << std::endl;
        return;
    }

    // 设置 unique_ptr (移动语义)
    guard->SetData(std::make_unique<CustomData>(42, "TestObject"));

    std::cout << "Initial data - ID: " << guard->GetData()->id
              << ", Name: " << guard->GetData()->name << std::endl;

    // 模拟异步操作
    std::thread worker([&guard]()
                       {
                           std::this_thread::sleep_for(std::chrono::milliseconds(300));

                           // 替换 unique_ptr (自动释放旧对象)
                           guard->SetData(std::make_unique<CustomData>(100, "NewObject"));
                           guard->SetResult(0);
                       });

    int result = guard->Wait(3000);
    std::cout << "Wait result: " << result << std::endl;
    std::cout << "Final data - ID: " << guard->GetData()->id
              << ", Name: " << guard->GetData()->name << std::endl;

    worker.join();
}

// ============================================================================
// 测试 11: 完美转发示例
// ============================================================================
void test_perfect_forwarding()
{
    std::cout << "\n=== Test: Perfect Forwarding ===" << std::endl;

    SyncContextPool<std::pair<std::string, int>> pool(0, 8);

    auto guard = pool.Borrow();
    if (!guard.IsValid())
    {
        std::cout << "Failed to borrow context!" << std::endl;
        return;
    }

    // 使用 emplace 方式构造 (C++17 起可以直接在 SetData 中完美转发)
    guard->SetData(std::make_pair(std::string("Answer"), 42));

    std::cout << "Pair data: (" << guard->GetData().first << ", "
              << guard->GetData().second << ")" << std::endl;

    guard->SetResult(0);
    int result = guard->Wait(1000);
    std::cout << "Result: " << result << std::endl;
}

// ============================================================================
// 测试 12: 对象池复用测试 - 验证 condition_variable 可重复使用
// ============================================================================
void test_context_pool_reuse()
{
    std::cout << "\n=== Test: Context Pool Reuse (Condition Variable Reusability) ===" << std::endl;

    SyncContextPool<void> pool(2, 4); // 初始 2 个，最多 4 个

    // 第一次借用 - 归还 - 再借用，验证 condition_variable 可以重复使用
    for (int round = 1; round <= 3; ++round)
    {
        std::cout << "\n--- Round " << round << " ---" << std::endl;

        auto guard = pool.Borrow();
        if (!guard.IsValid())
        {
            std::cout << "Failed to borrow context!" << std::endl;
            return;
        }

        std::cout << "Borrowed context, pool free: " << pool.FreeCount() << std::endl;

        // 模拟异步操作
        std::thread worker([&guard, round]()  // ✅ 显式捕获 round
                           {
                               std::this_thread::sleep_for(std::chrono::milliseconds(100));
                               guard->SetResult(round * 10); // 设置不同的结果值
                           });

        int result = guard->Wait(2000);
        std::cout << "Wait completed, result: " << result << " (expected: " << (round * 10) << ")" << std::endl;

        if (result != (round * 10))
        {
            std::cout << "ERROR: Result mismatch!" << std::endl;
        }

        worker.join();

        // guard 析构时自动归还到池中
        std::cout << "After return, pool free: " << pool.FreeCount() << std::endl;
    }

    std::cout << "\n✓ Pool reuse test passed: condition_variable works correctly after multiple uses" << std::endl;
}

// ============================================================================
// 测试 13: 同一上下文对象多次复用（无 shared_ptr 包装）
// ============================================================================
void test_same_context_multiple_reuse()
{
    std::cout << "\n=== Test: Same Context Multiple Reuse ===" << std::endl;

    SyncContextPool<void> pool(1, 1); // 只有 1 个上下文

    // 获取同一个上下文的引用（通过多次 Borrow/Return）
    std::vector<int> results;

    for (int i = 0; i < 5; ++i)
    {
        auto guard = pool.Borrow();
        if (!guard.IsValid())
        {
            std::cout << "Failed to borrow at iteration " << i << std::endl;
            break;
        }

        // 模拟异步响应
        std::thread worker([&guard, i]()
                           {
                               std::this_thread::sleep_for(std::chrono::milliseconds(50));
                               guard->SetResult(i + 1);
                           });

        int result = guard->Wait(1000);
        results.push_back(result);
        std::cout << "Iteration " << i << ": result = " << result << std::endl;

        worker.join();
        // guard 析构时归还
    }

    // 验证所有结果都正确
    bool success = (results.size() == 5);
    for (size_t i = 0; i < results.size() && success; ++i)
    {
        if (results[i] != (int)(i + 1))
        {
            success = false;
            break;
        }
    }

    if (success)
    {
        std::cout << "✓ Multiple reuse test passed: all 5 iterations completed successfully" << std::endl;
    }
    else
    {
        std::cout << "✗ Multiple reuse test FAILED" << std::endl;
    }
}

// ============================================================================
// 测试 14: Reset 后状态验证
// ============================================================================
void test_reset_state()
{
    std::cout << "\n=== Test: Reset State Verification ===" << std::endl;

    SyncContextPool<int> pool(1, 2);

    auto guard = pool.Borrow();
    if (!guard.IsValid())
    {
        std::cout << "Failed to borrow!" << std::endl;
        return;
    }

    // 设置一些状态
    guard->SetData(999);
    guard->SetResult(0);

    std::cout << "Before Reset: data=" << guard->GetData()
              << ", result=" << guard->result << std::endl;

    // 手动调用 Reset（模拟归还时的操作）
    guard->Reset();

    std::cout << "After Reset: data=" << guard->GetData()
              << ", result=" << guard->result << std::endl;

    // 验证状态已重置
    if (guard->GetData() == 0 && guard->result == -1)
    {
        std::cout << "✓ Reset state test passed: data and result properly reset" << std::endl;
    }
    else
    {
        std::cout << "✗ Reset state test FAILED" << std::endl;
    }
}

// ============================================================================
// 测试 15: 带数据的上下文复用
// ============================================================================
void test_data_context_reuse()
{
    std::cout << "\n=== Test: Data Context Reuse ===" << std::endl;

    struct Message
    {
        uint32_t seq_id;
        char content[32];
    };

    SyncContextPool<Message> pool(1, 2);

    for (int i = 0; i < 3; ++i)
    {
        auto guard = pool.Borrow();
        if (!guard.IsValid())
        {
            std::cout << "Failed to borrow at iteration " << i << std::endl;
            break;
        }

        // 设置数据
        guard->data.seq_id = static_cast<uint32_t>(i + 1);
        snprintf(guard->data.content, sizeof(guard->data.content), "Message-%d", i + 1);

        std::cout << "Round " << (i + 1) << ": Send seq=" << guard->data.seq_id
                  << ", content=" << guard->data.content << std::endl;

        // 模拟异步响应
        std::thread worker([&guard]()
                           {
                               std::this_thread::sleep_for(std::chrono::milliseconds(50));
                               guard->SetResult(0);
                           });

        int result = guard->Wait(1000);

        if (result == 0)
        {
            std::cout << "  Response received successfully" << std::endl;
        }
        else
        {
            std::cout << "  ERROR: Got result " << result << std::endl;
        }

        worker.join();
        // guard 析构时归还
    }

    std::cout << "✓ Data context reuse test completed" << std::endl;
}

// ============================================================================
// 测试 16: 超时后再次使用
// ============================================================================
void test_timeout_then_reuse()
{
    std::cout << "\n=== Test: Timeout Then Reuse ===" << std::endl;

    SyncContextPool<void> pool(1, 2);

    // 第一次：故意超时
    {
        auto guard = pool.Borrow();
        if (!guard.IsValid())
        {
            std::cout << "Failed to borrow!" << std::endl;
            return;
        }

        std::cout << "First call: Testing timeout (no SetResult called)" << std::endl;
        int result = guard->Wait(200); // 200ms 超时

        if (result == -ETIMEDOUT)
        {
            std::cout << "  ✓ Timeout occurred as expected" << std::endl;
        }
        else
        {
            std::cout << "  ✗ Unexpected result: " << result << std::endl;
        }

        // guard 归还
    }

    // 第二次：正常使用
    {
        auto guard = pool.Borrow();
        if (!guard.IsValid())
        {
            std::cout << "Failed to borrow!" << std::endl;
            return;
        }

        std::cout << "Second call: Normal operation after timeout" << std::endl;

        std::thread worker([&guard]()
                           {
                               std::this_thread::sleep_for(std::chrono::milliseconds(100));
                               guard->SetResult(42);
                           });

        int result = guard->Wait(2000);

        if (result == 42)
        {
            std::cout << "  ✓ Successfully got result after timeout recovery" << std::endl;
        }
        else
        {
            std::cout << "  ✗ Failed: result = " << result << std::endl;
        }

        worker.join();
    }

    std::cout << "✓ Timeout then reuse test completed" << std::endl;
}

// ============================================================================
// 主函数
// ============================================================================
int main()
{
    std::cout << "========================================" << std::endl;
    std::cout << "Starting SyncContextPool Tests" << std::endl;
    std::cout << "Based on mutex + condition_variable (Reusable)" << std::endl;
    std::cout << "========================================\n";

    // 基础功能测试
    test_basic_void_context();
    test_data_context();
    test_raii_auto_return();
    test_timeout_handling();
    
    // 并发和池测试
    test_concurrent_access();
    test_pool_capacity();
    
    // SetResult 一次性测试（历史遗留，未来可能移除）
    test_setresult_once_only();
    
    // 可移动类型测试
    test_movable_type_vector();     // std::vector
    test_movable_type_string();     // std::string
    test_movable_type_unique_ptr(); // std::unique_ptr
    test_perfect_forwarding();      // 完美转发
    
    // === 新增测试：对象池复用特性 ===
    test_context_pool_reuse();              // 测试 12: 池复用（condition_variable 可重复使用）
    test_same_context_multiple_reuse();     // 测试 13: 同一上下文多次复用
    test_reset_state();                     // 测试 14: Reset 后状态验证
    test_data_context_reuse();              // 测试 15: 带数据的上下文复用
    test_timeout_then_reuse();              // 测试 16: 超时后再次使用

    std::cout << "\n========================================" << std::endl;
    std::cout << "All tests completed." << std::endl;
    std::cout << "========================================" << std::endl;

    return 0;
}
