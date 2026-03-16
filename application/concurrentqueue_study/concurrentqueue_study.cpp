#include "moodycamel/concurrentqueue.h"
#include "moodycamel/blockingconcurrentqueue.h"
#include <cmath>

/*

多生产者-多消费者(MPMC)无锁线程间通信队列

1. 非阻塞版本 concurrentqueue.h

ConcurrentQueue(size_t initialSizeEstimate)            - 创建一个并发队列实例.可选地接受一个参数 initialSizeEstimate, 它是一个对队列将要持有的元素数量的估计值.这个估计可以帮助预先分配足够的内存来减少在运行时进行内存分配的需求.

// 入队操作
enqueue(item) : bool                                   - 尝试将单个元素 item 添加到队列中.自动扩容(动态分配).成功时返回 true.
enqueue(prod_token, item) : bool                       - 与上面类似, 但使用了一个生产者令牌 prod_token 来标识哪个生产者正在添加元素.
enqueue_bulk(item_first, count) : bool                 - 尝试将从 item_first 开始的连续 count 个元素添加到队列中.自动扩容(动态分配).
enqueue_bulk(prod_token, item_first, count) : bool     - 类似于 enqueue_bulk, 但是指定了生产者令牌.

try_enqueue(item) : bool                               - 尝试将单个元素 item 添加到队列中, 但如果内存不足则不会分配新的内存并失败.
try_enqueue(prod_token, item) : bool                   - 使用生产者令牌的尝试入队操作.
try_enqueue_bulk(item_first, count) : bool             - 尝试批量入队, 不分配额外内存.
try_enqueue_bulk(prod_token, item_first, count) : bool - 带有生产者令牌的批量尝试入队操作.

// 出队操作
try_dequeue(item&) : bool                              - 试图从队列中移除一个元素并将其值复制到 item 中.这个操作不会分配内存.成功时返回 true.
try_dequeue(cons_token, item&) : bool                  - 与上面类似, 但使用了消费者令牌 cons_token.
try_dequeue_bulk(item_first, max) : size_t             - 试图从队列中移除最多 max 个元素并将它们复制到以 item_first 开始的区域.返回实际移除的元素数量.
try_dequeue_bulk(cons_token, item_first, max) : size_t - 带有消费者令牌的批量尝试出队操作.

// 从指定生产者处出队
try_dequeue_from_producer(prod_token, item&) : bool                  - 试图从具有给定生产者令牌 prod_token 的队列部分移除一个元素.
try_dequeue_bulk_from_producer(prod_token, item_first, max) : size_t - 试图从具有给定生产者令牌 prod_token 的队列部分移除最多 max 个元素.

// 数量
size_approx() : size_t - 返回队列中元素数量的一个近似值.这个数字可能不是精确的, 特别是在并发环境下.

2. 阻塞版本 blockingconcurrentqueue.h

wait_dequeue(item&) : bool                  - 该方法会阻塞调用者直到有一个元素可以从队列中出队.一旦有元素可用, 它会被移除并赋值给 item, 然后返回 true.如果队列被销毁或发生其他异常情况, 可能不会返回.
wait_dequeue_bulk(item_first, max) : size_t - 类似于 wait_dequeue, 但是允许一次出队多个元素.它尝试从队列中取出最多 max 个元素并将它们复制到以 item_first 开始的区域.返回实际移除的元素数量.

wait_dequeue_timed(item&, timeout)                         - 允许指定一个超时时间(可以是微秒或者 std::chrono 对象).如果在超时时间内没有元素可出队, 则返回 false; 否则, 行为与 wait_dequeue 相同.
wait_dequeue_bulk_timed(item_first, max, timeout) : size_t - 与 wait_dequeue_bulk 类似, 但允许设置超时.如果在超时时间内无法获取任何元素, 那么返回0.

使用注意事项
1. 不要在等待过程中销毁队列: 这是使用阻塞队列的一个主要限制.你需要确保没有任何线程正在等待队列上的操作时才去销毁队列.这意味着你必须能够确定在调用任何阻塞方法之前, 至少会有一个元素即将入队.
2. 协调清理: 对于非阻塞队列来说, 协调清理也可能是一个挑战, 但在实践中通常更容易管理, 因为你可以通过检查队列是否为空来判断是否有线程还在使用它.

Tokens
moodycamel::ConcurrentQueue 提供了 ProducerToken 和 ConsumerToken 机制来提高多线程环境下的性能.
这些令牌为每个生产者或消费者提供了额外的存储空间, 从而减少了对共享资源的竞争, 加快了入队和出队操作的速度.

在生产或者消费大量元素时, 最高效的方式是:
1. 使用带令牌的批量方法: 这是最快的方式, 因为它们利用了令牌提供的额外存储空间.
2. 使用不带令牌的批量方法: 如果不能使用令牌, 那么使用批量方法仍然比单个元素的操作更高效.
3. 使用带令牌的单个元素方法: 如果必须逐个处理元素, 那么使用令牌会比不使用令牌更高效.
4. 使用不带令牌的单个元素方法: 这是最后的选择, 通常也是最慢的方式.

注意事项
1. 不要随意创建令牌: 理想情况下, 每个线程应该只有一个生产者令牌和一个消费者令牌.虽然队列可以在没有令牌的情况下工作, 但使用令牌可以显著提高性能.
2. 令牌不绑定到特定线程: 令牌并不强制要求与特定线程关联, 只要在同一时间只由一个生产者/消费者使用即可.
3. 通过合理地使用令牌, 可以最大化 moodycamel::ConcurrentQueue 和 moodycamel::BlockingConcurrentQueue 的性能, 特别是在多线程环境中.

预分配(正确使用 try_enqueue 方法的关键)
在于预分配足够的内存空间, 以确保在队列中始终有足够的空间来存放元素.
为了确保 moodycamel::ConcurrentQueue 能够在任何给定时间至少容纳 N 个元素, 我们需要考虑到队列内部的块(block)机制.
每个块默认包含 32 个元素(这个值可以通过自定义特性来改变), 并且一旦一个块中的某个槽位被使用过, 该槽位就不能再被重用, 直到整个块被完全填满并清空.

这里的 BLOCK_SIZE 是块大小, 默认为 32.你可以通过自定义特性来改变这个值.
这里的 ceil 函数是向上取整函数, 以确保我们总是向上取到最接近的整数倍数.

显式生产者(有令牌)的预分配:
公式为: preallocated_elements = (std::ceil(N / BLOCK_SIZE) + 1) * MAX_NUM_PRODUCERS * BLOCK_SIZE

隐式生产者(无令牌)的预分配:
公式为: preallocated_elements = (std::ceil(N / BLOCK_SIZE) - 1 + 2 * MAX_NUM_PRODUCERS) * BLOCK_SIZE

混合生产者的预分配:
公式为: preallocated_elements = ((std::ceil(N / BLOCK_SIZE) - 1) * (MAX_EXPLICIT_PRODUCERS + 1) + 2 * (MAX_IMPLICIT_PRODUCERS + MAX_EXPLICIT_PRODUCERS)) * BLOCK_SIZE

注意事项
块大小: 如果你改变了块大小(通过自定义特性), 请确保在上述公式中使用正确的块大小.
内存使用: 预分配更多的空间可以提高性能, 但也会占用更多的内存.根据你的具体应用场景权衡这一点.
实际应用: 在实际应用中, 你可能还需要考虑其他因素, 如消费者的行为,队列的最大容量等.

通过这种方式, 你可以更有效地使用 try_enqueue 方法, 确保在大多数情况下不需要动态分配内存, 从而提高程序的性能.

moodycamel::ConcurrentQueue<int> q(min_elements, max_explicit_producers, max_implicit_producers);
直接使用构造函数重载, 该重载接受最小元素数量(N),最大显式生产者数量和最大隐式生产者数量, 并为你计算所需的预分配空间

除了块之外, 还有一些内部数据结构可能需要在超过初始大小时重新分配内存.为了避免这种情况, 你需要适当调整以下特性的初始大小:

INITIAL_IMPLICIT_PRODUCER_HASH_SIZE: 限制同时活跃的隐式生产者的数量.
IMPLICIT_INITIAL_INDEX_SIZE:         限制每个隐式生产者在内部哈希表需要重新分配之前可以插入的未消费元素数量.
EXPLICIT_INITIAL_INDEX_SIZE:         限制每个显式生产者在内部哈希表需要重新分配之前可以插入的未消费元素数量.

处理 try_enqueue 的失败
即使你正确地进行了预分配, 由于队列利用了弱内存顺序来提高速度, 所以在竞争条件下 try_enqueue 仍有可能失败.
因此, 你应该总是准备好处理失败的情况, 例如通过循环尝试直到成功, 或者选择丢弃元素.

异常安全性保证
入队操作:
如果元素构造函数抛出异常, 入队操作将完全回滚.
对于批量入队操作, 这意味着元素会被复制而不是移动, 以避免在异常情况下只有部分对象被移动的情况.非批量入队操作总是尽可能使用移动构造函数.
出队操作:
如果在出队操作(单个或批量)期间赋值操作符抛出异常, 元素仍然被视为已经出队.
在这种情况下, 出队的元素会在异常传播之前被正确销毁, 但无法恢复这些元素本身.
异常传播:
任何抛出的异常都会沿着调用栈传播, 在此过程中队列保持一致状态.
注意事项:
队列的异常安全保证仅在元素类型的析构函数永不抛出异常, 并且传递给队列的迭代器(用于批量操作)也永不抛出异常的情况下有效.
特别需要注意的是, 使用 std::back_inserter 迭代器时要小心, 因为目标容器可能需要分配内存并在迭代器内部抛出 std::bad_alloc 异常.因此, 如果这样做, 请确保预先为目标容器预留足够的容量.
优化建议:
如果你的类型的拷贝构造函数,移动构造函数或赋值操作符不会抛出异常, 请务必用 noexcept 标注它们.这可以避免队列中不必要的异常检查开销, 即使在零成本异常的情况下, 代码大小的影响也需要考虑.

自定义特性 (Traits)
moodycamel::ConcurrentQueue 允许通过自定义特性来调整其行为.
你可以通过继承 moodycamel::ConcurrentQueueDefaultTraits 并覆盖相应的成员变量来自定义这些特性.以下是一些常见的可配置特性:

块大小 (BLOCK_SIZE): 默认为 32, 你可以根据需要调整这个值.
初始隐式生产者哈希表大小 (INITIAL_IMPLICIT_PRODUCER_HASH_SIZE): 限制同时活跃的隐式生产者的数量.
隐式生产者初始索引大小 (IMPLICIT_INITIAL_INDEX_SIZE): 限制每个隐式生产者在内部哈希表需要重新分配之前可以插入的未消费元素数量.
显式生产者初始索引大小 (EXPLICIT_INITIAL_INDEX_SIZE): 限制每个显式生产者在内部哈希表需要重新分配之前可以插入的未消费元素数量.
*/

void one_producer_one_consumer_func()
{
    moodycamel::BlockingConcurrentQueue<int> q(100, 1, 0);

    std::thread producer([&]()
                         {
                             moodycamel::ProducerToken ptok(q);
                             for (int i = 0; i != 100; ++i)
                             {
                                 std::this_thread::sleep_for(std::chrono::milliseconds(i % 10));
                                 q.try_enqueue(ptok, i);
                             }
                         });

    std::thread consumer([&]()
                         {
                             moodycamel::ConsumerToken ctok(q);
                             for (int i = 0; i != 100; ++i)
                             {
                                 int item;
                                 if (q.wait_dequeue_timed(ctok, item, std::chrono::milliseconds(5)))
                                 {
                                     ++i;
                                     assert(item == i);
                                 }
                             }
                         });
    producer.join();
    consumer.join();

    assert(q.size_approx() == 0);
}

int main()
{
    one_producer_one_consumer_func();
    return 0;
}
