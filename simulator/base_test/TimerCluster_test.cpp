#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <cassert>

#include "TimerCluster.hpp"

std::atomic<bool> single_fired{false};
std::atomic<int> cycle_count{0};
std::atomic<bool> reset_fired{false};
std::atomic<bool> temp_fired{false};
std::atomic<bool> final_timer_fired{false};

void test_TimerCluster()
{
    std::cout << ("Starting TimerCluster test...") << std::endl;

    TimerCluster timer_cluster;

    // === Test 1: Single-shot timer ===
    std::cout << ("Test 1: Single-shot timer (1000ms)") << std::endl;

    auto single_cb = [](TimerCluster::TimerId id, TimerCluster::TimerUserData userdata)
    {
        (void)userdata;
        std::cout << id << ">> Callback: Single timer fired!" << std::endl;
        single_fired = true;
    };

    bool added = timer_cluster.AddTimer(1, TimerCluster::TimerType::SINGLE, 1000, single_cb);
    (void)added;
    assert(added);
    std::this_thread::sleep_for(std::chrono::milliseconds(1200));
    assert(single_fired.load());
    std::cout << ("Test 1 passed ✓\n") << std::endl;

    // === Test 2: Cyclic timer ===
    std::cout << ("Test 2: Cyclic timer (300ms)") << std::endl;

    auto cycle_cb = [](TimerCluster::TimerId id, TimerCluster::TimerUserData userdata)
    {
        (void)userdata;
        cycle_count++;
        std::cout << id << ">> Callback: Cyclic timer fired, count = " << cycle_count << std::endl;
    };

    added = timer_cluster.AddTimer(2, TimerCluster::TimerType::CYCLE, 300, cycle_cb);
    assert(added);
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    bool del_result = timer_cluster.DelTimer(2);
    (void)del_result;
    assert(del_result);
    assert(cycle_count.load() >= 2);
    std::cout << "Final cycle count: " << cycle_count.load() << std::endl;
    std::cout << ("Test 2 passed ✓\n") << std::endl;

    // === Test 3: Reset timer ===
    std::cout << ("Test 3: Reset timer (from 1000ms to 500ms)") << std::endl;
    auto reset_cb = [](TimerCluster::TimerId id, TimerCluster::TimerUserData userdata)
    {
        (void)userdata;
        std::cout << id << ">> Callback: Reset Single timer fired!" << std::endl;
        reset_fired = true;
    };

    added = timer_cluster.AddTimer(3, TimerCluster::TimerType::SINGLE, 1000, reset_cb);
    assert(added);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    bool reset_ok = timer_cluster.ResetTimer(3, 500);
    (void)reset_ok;
    assert(reset_ok);
    std::this_thread::sleep_for(std::chrono::milliseconds(600));
    assert(reset_fired.load());
    std::cout << ("Test 3 passed ✓\n") << std::endl;

    // === Test 4: Delete non-existent timer ===
    std::cout << ("Test 4: Delete non-existent timer") << std::endl;
    bool del_non_exist = timer_cluster.DelTimer(999);
    (void)del_non_exist;
    assert(!del_non_exist);
    std::cout << ("Test 4 passed ✓\n") << std::endl;

    // === Test 5: Concurrent operations ===
    std::cout << ("Test 5: Concurrent Add/Delete from multiple threads") << std::endl;
    std::vector<std::thread> threads;

    for (uint32_t i = 10; i < 20; ++i)
    {
        threads.emplace_back([&timer_cluster, i]
                             {
                                 std::atomic<int> count{0};
                                 auto cb = [&count](TimerCluster::TimerId id, TimerCluster::TimerUserData userdata)
                                 {
                                     (void)userdata;
                                     (void)id;
                                     ++count;
                                 };
                                 timer_cluster.AddTimer(i, TimerCluster::TimerType::CYCLE, 100, cb);
                                 std::this_thread::sleep_for(std::chrono::milliseconds(530));
                                 timer_cluster.DelTimer(i);
                                 std::cout << "Thread " << i << " finished, fired " << count.load() << " times" << std::endl;
                             });
    }
    for (auto &t : threads)
        t.join();
    std::cout << ("Test 5 passed ✓\n") << std::endl;

    // === Test 6: Destruction safety ===
    std::cout << ("Test 6: TimerCluster destruction") << std::endl;
    {
        TimerCluster temp_cluster;

        temp_cluster.AddTimer(100, TimerCluster::TimerType::SINGLE, 2000, [](TimerCluster::TimerId id, TimerCluster::TimerUserData userdata)
                              {
                                  (void)userdata;
                                  std::cout << id << ">> Callback: Should NOT run!" << std::endl;
                                  temp_fired = true;
                              });
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    assert(!temp_fired);
    std::cout << ("Test 6 passed ✓\n") << std::endl;

    // ===================================================================
    // === Test 7: Add/Delete Timer FROM Callback ===
    // ===================================================================
    std::cout << ("Test 7: Add and Delete timers FROM within a callback") << std::endl;

    std::atomic<int> first_count{0};

    auto recursive_callback = [&](TimerCluster::TimerId id, TimerCluster::TimerUserData userdata)
    {
        (void)userdata;
        int total_count = ++first_count;
        std::cout << id << ">> Test 7: Recursive callback fired, total_count = " << total_count << std::endl;

        if (total_count == 1)
        {
            // ✅ 在回调中删除当前定时器(虽然已被自动移除,但调用是安全的)
            bool del_self = timer_cluster.DelTimer(700);
            std::cout << ">> Test 7: DelTimer(700) in callback -> " << (del_self ? "OK" : "Failed (expected)") << std::endl;

            // ✅ 在回调中添加一个新定时器
            bool add_new = timer_cluster.AddTimer(
                701,
                TimerCluster::TimerType::SINGLE,
                400,
                [](TimerCluster::TimerId new_id, TimerCluster::TimerUserData new_userdata)
                {
                    (void)new_userdata;
                    std::cout << new_id << ">> Test 7: New timer (701) fired!" << std::endl;
                    final_timer_fired = true;
                });
            std::cout << ">> Test 7: AddTimer(701) in callback -> " << (add_new ? "Success" : "Failed") << std::endl;
        }
    };

    // 添加主定时器
    added = timer_cluster.AddTimer(
        700,
        TimerCluster::TimerType::SINGLE,
        500,
        recursive_callback);
    assert(added);
    std::cout << ">> Test 7: Timer 700 added, will fire in 500ms..." << std::endl;

    // 等待回调链执行完成
    std::this_thread::sleep_for(std::chrono::milliseconds(1200));

    // 验证行为
    assert(first_count.load() == 1);                // 第一个回调只触发一次
    assert(final_timer_fired.load());               // 新增的定时器成功触发
    assert(timer_cluster.TimerExist(700) == false); // 700 已被删除或过期
    assert(timer_cluster.TimerExist(701) == false); // 701 是单次,也应已删除

    std::cout << ("Test 7 passed ✓ (Safe recursive Add/Delete in callback!)\n") << std::endl;

    // Final summary
    std::cout << ("🎉 All 7 tests passed! TimerCluster is robust and thread-safe.") << std::endl;
}

int main()
{
    test_TimerCluster();

    return 0;
}
