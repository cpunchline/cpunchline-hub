#include <thread>
#include "mpmc_queue.h"
#include "mpmc_queue_pack.h"
#include "pointer_mpmc_queue.h"
#include "shared_mpmc_queue.h"

// 1. 定义测试数据类型
struct Message
{
    uint64_t id;
    double timestamp;
    int data[8]; // 模拟一些数据

    Message(uint64_t i, double t) :
        id(i), timestamp(t)
    {
        for (int &d : data)
            d = static_cast<int>(i % 100);
    }

    void print(bool is_push) const
    {
        std::cout << (is_push ? "PUSH" : "POP") << ":" << "Message{id=" << id << ", ts=" << timestamp << ", data[0]=" << data[0] << "}\n";
    }
};

int main()
{
    static_assert(8 == sizeof(std::unique_ptr<Message>), "sizeof(std::unique_ptr<T>) must be 8 bytes");
    constexpr int NUM_PRODUCERS = 3;
    constexpr int NUM_CONSUMERS = 2;
    constexpr int MESSAGES_PER_PRODUCER = 10;
    constexpr int QUEUE_CAPACITY = 128; // 必须是 2 的幂

    // 2. 创建 pointer_mpmc_queue
    //    存储 unique_ptr<Message>
    //    底层使用 mpmc_queue<void*, 0, uint32_t, false, false>
    es::lockfree::pointer_mpmc_queue<
        es::lockfree::mpmc_queue, // 底层队列模板
        Message,                  // 数据类型
        0,                        // 运行时指定大小
        uint32_t,                 // 索引类型
        std::unique_ptr           // 智能指针类型
        >
        queue(QUEUE_CAPACITY);

    std::atomic<uint64_t> next_id{1};
    std::atomic<int> consumed_count{0};

    std::vector<std::thread> producers;
    std::vector<std::thread> consumers;

    auto start_time = std::chrono::high_resolution_clock::now();

    // 3. 启动生产者线程
    for (int p = 0; p < NUM_PRODUCERS; ++p)
    {
        producers.emplace_back([&queue, &next_id]()
                               {
                                   for (int i = 0; i < MESSAGES_PER_PRODUCER; ++i)
                                   {
                                       uint64_t id = next_id.fetch_add(1);
                                       auto msg = std::make_unique<Message>(id, std::clock() * 1.0 / CLOCKS_PER_SEC);

                                       msg->print(true); // 可选:打印消息

                                       // 尝试 push,失败则重试(模拟忙等)
                                       while (!queue.push(std::move(msg)))
                                       {
                                           // 可以加 std::this_thread::yield() 降低 CPU 占用
                                           std::this_thread::yield();
                                       }
                                   }
                               });
    }

    // 4. 启动消费者线程
    for (int c = 0; c < NUM_CONSUMERS; ++c)
    {
        consumers.emplace_back([&queue, &consumed_count]()
                               {
                                   std::unique_ptr<Message> msg;
                                   while (consumed_count.load() < NUM_PRODUCERS * MESSAGES_PER_PRODUCER)
                                   {
                                       if (queue.pop(msg))
                                       {
                                           // 处理消息(这里只是打印)
                                           msg->print(false); // 可选:打印消息

                                           // 模拟处理时间
                                           // std::this_thread::sleep_for(std::chrono::microseconds(10));

                                           msg.reset(); // 显式释放(可选,离开作用域也会自动释放)
                                           consumed_count.fetch_add(1);
                                       }
                                       else
                                       {
                                           // 队列空,短暂让出 CPU
                                           std::this_thread::yield();
                                       }
                                   }
                               });
    }

    // 5. 等待所有线程结束
    for (auto &t : producers)
    {
        t.join();
    }

    for (auto &t : consumers)
    {
        t.join();
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    // 6. 输出结果
    std::cout << "✅ Test completed!\n";
    std::cout << "Producers: " << NUM_PRODUCERS << "\n";
    std::cout << "Consumers: " << NUM_CONSUMERS << "\n";
    std::cout << "Messages sent: " << (NUM_PRODUCERS * MESSAGES_PER_PRODUCER) << "\n";
    std::cout << "Messages consumed: " << consumed_count.load() << "\n";
    std::cout << "Queue capacity: " << queue.capacity() << "\n";
    std::cout << "Test duration: " << duration.count() << " ms\n";

    // 7. 析构:pointer_mpmc_queue 会自动清理所有未消费的指针(如果有)
    //      本例中应该已经全部消费完
    return 0;
}