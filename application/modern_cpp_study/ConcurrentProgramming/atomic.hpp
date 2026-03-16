#include <atomic>
#include <cassert>
#include <iostream>
#include <thread>
#include <vector>

/*
内存顺序类型
memory_order_relaxed:最宽松的内存顺序, 不提供任何同步保证.它只保证原子操作本身是原子的, 但不保证操作之间的顺序.
memory_order_consume:消费者内存顺序, 用于同步依赖关系.它保证了依赖于原子操作结果的后续操作将按照正确的顺序执行.
memory_order_acquire:获取内存顺序, 用于同步对共享数据的访问.它保证了在获取操作之后对共享数据的所有读取操作都将看到最新的数据.
memory_order_release:释放内存顺序, 用于同步对共享数据的访问.它保证了在释放操作之前对共享数据的所有写入操作都已完成, 并且对其他线程可见.
memory_order_acq_rel:获取-释放内存顺序, 结合了获取和释放两种内存顺序的特点.它既保证了获取操作之后对共享数据的所有读取操作都将看到最新的数据, 又保证了在释放操作之前对共享数据的所有写入操作都已完成, 并且对其他线程可见.
memory_order_seq_cst:顺序一致性内存顺序, 提供了最严格的同步保证.它保证了所有线程都将看到相同的操作顺序, 并且所有原子操作都将按照程序顺序执行.(default)
*/

void test_dependency()
{
    /*
    存在控制依赖(data dependency): 必须先有 str 和 i 的值, 才能计算 str[i];
    编译器不能重排位置 3 到 1 或 2 之前;
    这种依赖称为 carries-a-dependency-to, 在内存模型中用于某些高级同步(如 dependency-ordered operations), 但此处是单线程, 不涉及并发.
    */
    // 1 处
    std::string str = "hello world!";
    // 2 处
    const size_t i = 3;
    // 3 处
    std::cout << str[i] << std::endl;
}

std::atomic<int> data(0);
std::atomic<bool> ready(false);

// 要确保"写 data 然后 写 ready" 对 "读 ready 然后 读 data" 有 happens-before 关系, 需使用 release-acquire 语义 或 seq_cst 语义;
void producer()
{
    data.store(42, std::memory_order_relaxed);
    // ready.store(true, std::memory_order_relaxed); // 即使逻辑上"先写 data 再写 ready", 编译器或 CPU 可能重排这两个 store 操作, 导致其他线程可能看到相反顺序;
    ready.store(true, std::memory_order_release);
}

void producer_fence()
{
    data.store(42, std::memory_order_relaxed);           // 1
    std::atomic_thread_fence(std::memory_order_release); // 2 内存屏障(fence)可以同步多个 relaxed 操作, 适用于"批量发布"场景;
    ready.store(true, std::memory_order_relaxed);        // 3
}

void consumer()
{
#if 0
    while (!ready.load(std::memory_order_relaxed)) // 即使 producer 编译器或 CPU 不重排, consumer看到 ready == true 时, 也可能还没看到 data == 42, 因为 relaxed 不建立同步关系;
    {
    }
#endif
    while (!ready.load(std::memory_order_acquire)) // producer 所有在 ready 的 release 之前的操作对 producer 中 ready 的 acquire 之后的操作可见;
    {
    }
    std::cout << data.load(std::memory_order_relaxed) << std::endl;
}

void consumer_fence()
{
    while (!ready.load(std::memory_order_relaxed)) // 4
    {
    }
    std::atomic_thread_fence(std::memory_order_acquire); // 5
    // acquire fence 与 release fence 同步, 写线程中 fence 之前的所有操作(包括 x=true)对读线程 fence 之后的操作可见;
    std::cout << data.load(std::memory_order_relaxed) << std::endl;
}

void product_consumer_release_acquire()
{
    std::thread t1(producer);
    std::thread t2(consumer);
    t1.join();
    t2.join();
}

void product_consumer_fence()
{
    std::thread t1(producer_fence);
    std::thread t2(consumer_fence);
    t1.join();
    t2.join();
}

void release_sequence()
{
    // acquire-release 同步要求 acquire 必须"匹配"特定的 release 写者.多写者场景需额外机制(如版本号,状态机);
    // RMW 操作(如 CAS,fetch_add)会延续 release 序列, 即使自身是 relaxed;

    std::vector<int> v{};
    std::atomic<int> flag{0};

    std::thread t1([&]()
                   {
                       v.push_back(42);                          //(1)
                       flag.store(1, std::memory_order_release); //(2) t1 执行 release store(值=1) → 建立 release 序列起点.
                   });

    std::thread t2([&]()
                   {
                       int expected = 1;
                       while (!flag.compare_exchange_strong(expected, 2, std::memory_order_relaxed)) // (3)
                           expected = 1;
                       // t2 执行 read-modify-write 操作(compare_exchange_strong), 即使使用 relaxed, 它也延续了 release 序列(C++ 标准规定:RMW 操作继承 release 语义).
                   });

    std::thread t3([&]()
                   {
                       while (flag.load(std::memory_order_acquire) < 2)
                           ;                  // (4)
                                              /*
                        t3 使用 acquire load 看到 flag >= 2, 意味着它看到了 t2 的写入,  而 t2 的写入属于 t1 启动的 release 序列, 因此 t3 与 t1 间接同步 → 能看到 v.push_back(42).
                        */
                       assert(v.at(0) == 42); // (5)
                   });

    t1.join();
    t2.join();
    t3.join();
}

int main()
{
    test_dependency();
    product_consumer_release_acquire();
    product_consumer_fence();
    release_sequence();
    return 0;
}