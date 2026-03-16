#include <iostream>
#include <thread>
#include <vector>
#include <deque>
#include <chrono>
#include <atomic>
#include <cassert>

#include "ThreadQueue.hpp"

// ============== Paste your UnBoundedQueue and BoundedQueue here ==============
// Your queue implementation
// ===========================================================================

#define LOG(msg) std::cout << "[TEST] " << msg << std::endl

int main()
{
    LOG("=== Testing UnBoundedQueue<int> ===");

    UnBoundedQueue<int> unbounded;
    int val = 0;
    (void)val;

    // Test 1: Push / tryPop
    LOG("Test Push and tryPop (single thread)");
    unbounded.Push(1);
    unbounded.Push(2);
    assert(unbounded.tryPop(val) && val == 1);
    assert(unbounded.tryPop(val) && val == 2);
    assert(!unbounded.tryPop(val)); // empty

    // Test 2: Pop blocking behavior
    LOG("Test Pop blocking");
    std::thread t1([&]
                   {
                       std::this_thread::sleep_for(std::chrono::milliseconds(100));
                       unbounded.Push(99);
                   });

    assert(unbounded.Pop(val) && val == 99);
    t1.join();

    // Test 3: Pop(std::deque<T>&) batch
    LOG("Test batch Pop");
    unbounded.Push(10);
    unbounded.Push(20);
    unbounded.Push(30);
    std::deque<int> batch;
    assert(unbounded.PopAll(batch) && batch.size() == 3);
    assert(batch[0] == 10 && batch[1] == 20 && batch[2] == 30);

    // Test 4: tryPop batch
    unbounded.Push(40);
    assert(unbounded.tryPopAll(batch) && batch.size() == 1 && batch[0] == 40);

    // Test 5: Size / Empty / Clear
    LOG("Test Size / Empty / Clear");
    assert(unbounded.Empty());
    assert(unbounded.Size() == 0);
    unbounded.Push(1);
    unbounded.Push(2);
    assert(unbounded.Size() == 2);
    unbounded.Clear();
    assert(unbounded.Empty());

    // Test 6: Quit() and Pop exit behavior
    LOG("Test Quit() behavior");
    unbounded.Push(100);
    assert(unbounded.Pop(val) && val == 100);

    std::atomic<bool> pop_failed{false};
    std::thread t2([&]
                   {
                       int dummy;
                       if (!unbounded.Pop(dummy))
                       {
                           pop_failed = true;
                       }
                   });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    unbounded.Quit();
    t2.join();

    assert(pop_failed.load());
    LOG("UnBoundedQueue test passed ✓\n");

    // ========================================================================
    LOG("=== Testing BoundedQueue<int, 3> ===");

    BoundedQueue<int, 3> bounded;
    int v = 0;
    (void)v;
    // Test 1: Basic Push/Pop
    LOG("Test basic Push/Pop");
    assert(bounded.Push(1));
    assert(bounded.Push(2));
    assert(bounded.Size() == 2);
    assert(!bounded.Full());

    assert(bounded.Pop(v) && v == 1);
    assert(bounded.Pop(v) && v == 2);
    assert(bounded.Empty());

    // Test 2: tryPush when full
    LOG("Test tryPush when full");
    assert(bounded.tryPush(10));
    assert(bounded.tryPush(20));
    assert(bounded.tryPush(30));
    assert(bounded.Full());
    assert(!bounded.tryPush(40));

    // Test 3: Pop batch
    std::deque<int> batch2;
    assert(bounded.PopAll(batch2) && batch2.size() == 3);
    assert(batch2[0] == 10 && batch2[1] == 20 && batch2[2] == 30);

    // Test 4: tryPop batch
    bounded.tryPush(100);
    assert(bounded.tryPopAll(batch2) && batch2.size() == 1 && batch2[0] == 100);

    // Test 5: Push blocking (producer waits)
    LOG("Test blocking Push");
    assert(bounded.tryPush(1));
    assert(bounded.tryPush(2));
    assert(bounded.tryPush(3));

    std::atomic<bool> push_blocked{false};
    std::thread producer([&]
                         {
                             push_blocked = true;
                             bool success = bounded.Push(4);
                             if (success)
                                 push_blocked = false;
                         });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    assert(push_blocked.load());

    // Free space
    assert(bounded.Pop(v) && v == 1);

    producer.join();
    assert(!push_blocked.load());

    // Test 6: Quit() wakes up blocked threads
    LOG("Test Quit() wakes blocked threads");
    bounded.Quit(); // Close queue

    std::atomic<bool> pop_blocked{true};
    std::thread consumer([&]
                         {
                             int dummy;
                             if (!bounded.Pop(dummy))
                             {
                                 pop_blocked = false; // Exits due to quit
                             }
                         });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    assert(!pop_blocked.load());

    consumer.join();

    LOG("BoundedQueue test passed ✓\n");
    LOG("All tests passed! ✓✓✓");

    return 0;
}
