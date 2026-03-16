#include "LRUCache.hpp"

#include <cassert>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <memory>

using namespace std;

// 示例1:基本的 put/get/容量驱逐测试
void example_basic_usage()
{
    cout << "\n=== Example 1: Basic Usage and Eviction ===" << endl;

    LRUCache<string, int> cache(3); // 容量为3

    cache.put("a", 1);
    cache.put("b", 2);
    cache.put("c", 3);

    cout << "After adding a=1, b=2, c=3:" << endl;
    cout << "Size: " << cache.size() << endl; // 输出 3

    // 访问 "a",使其变为最近使用
    int val;
    if (cache.get("a", val))
    {
        cout << "Got a = " << val << endl; // 输出 a=1
    }

    // 插入新元素 d=4,此时缓存满,最久未使用的 "b" 将被驱逐
    cache.put("d", 4);

    cout << "After putting d=4 (should evict 'b'):" << endl;
    cout << "Contains 'b'? " << (cache.contains("b") ? "yes" : "no") << endl; // no
    cout << "Contains 'd'? " << (cache.contains("d") ? "yes" : "no") << endl; // yes

    // 验证 a,b,c,d 是否存在
    vector<string> keys = {"a", "b", "c", "d"};
    for (const auto &k : keys)
    {
        if (cache.get(k, val))
        {
            cout << k << " = " << val << endl;
        }
        else
        {
            cout << k << " not found" << endl;
        }
    }
}

// 示例2:驱逐回调(Eviction Callback)
void example_eviction_callback()
{
    cout << "\n=== Example 2: Eviction Callback ===" << endl;

    LRUCache<string, shared_ptr<int>> cache(2);

    // 设置驱逐回调:当元素被移除时打印并释放资源
    cache.set_eviction_callback([](const string &key, const shared_ptr<int> &ptr)
                                {
                                    cout << "[Eviction] Key '" << key << "' with value=" << *ptr << " is being removed." << endl;
                                });

    auto a = make_shared<int>(100);
    auto b = make_shared<int>(200);
    auto c = make_shared<int>(300);

    cout << "Initial ref_count of a: " << a.use_count() << endl; // 1

    cache.put("a", a);
    cache.put("b", b);

    cout << "After put(a), put(b): ref_count of a: " << a.use_count() << endl; // 2

    // 插入 c -> 驱逐 "a"
    cache.put("c", c); // 触发回调,a 被移出缓存

    cout << "After put(c): ref_count of a: " << a.use_count() << endl; // 应为1(缓存不再持有)

    // 此时只有 b 和 c 在缓存中
    auto ptr = cache.get("b");
    if (ptr)
        cout << "Found b = " << *ptr->get() << endl;
}

// 示例3:remove 和 clear 操作
void example_remove_and_clear()
{
    cout << "\n=== Example 3: Remove and Clear ===" << endl;

    LRUCache<string, string> cache(3);
    cache.put("name", "Alice");
    cache.put("city", "Beijing");
    cache.put("job", "Engineer");

    cout << "Before remove: size = " << cache.size() << endl;

    bool removed = cache.remove("city");
    cout << "Removed 'city'? " << (removed ? "yes" : "no") << endl;
    cout << "After remove: size = " << cache.size() << endl;

    cache.clear();
    cout << "After clear: size = " << cache.size() << endl;
    cout << "Is empty? " << (cache.empty() ? "yes" : "no") << endl;
}

// 示例4:for_each 和 remove_if 高级操作
void example_advanced_operations()
{
    cout << "\n=== Example 4: for_each and remove_if ===" << endl;

    LRUCache<int, string> cache(5);
    cache.put(1, "apple");
    cache.put(2, "banana");
    cache.put(3, "cherry");
    cache.put(4, "date");

    cout << "All items:" << endl;
    cache.for_each([](int k, const string &v)
                   {
                       cout << "  " << k << " -> " << v << endl;
                   });

    // 删除所有长度 > 5 的值
    size_t erased = cache.remove_if([](int k, const string &v)
                                    {
                                        (void)k;
                                        return v.length() > 5;
                                    });

    cout << "Removed " << erased << " items (length > 5)" << endl;
    cout << "Remaining items:" << endl;
    cache.for_each([](int k, const string &v)
                   {
                       cout << "  " << k << " -> " << v << endl;
                   });
}
void example_thread_safety_demo()
{
    std::cout << "\n=== Example 5: Thread Safety Demo (with std::thread) ===" << std::endl;

    LRUCache<int, int> cache(100);

    constexpr int num_threads = 4;
    constexpr int ops_per_thread = 50;

    // 启动多个线程并发访问
    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t)
    {
        threads.emplace_back([&cache, t]()
                             {
                                 for (int i = 0; i < ops_per_thread; ++i)
                                 {
                                     int key = t * ops_per_thread + i;
                                     cache.put(key, key * key);

                                     int val;
                                     bool found = cache.get(key, val);
                                     (void)found;
                                     assert(found && "Inserted key should be retrievable");
                                     assert(val == key * key && "Value mismatch");
                                 }
                             });
    }

    // 等待所有线程完成
    for (auto &th : threads)
    {
        th.join();
    }

    std::cout << "Concurrent put/get completed successfully.\n";
    std::cout << "Final cache size: " << cache.size() << " (expected: "
              << num_threads * ops_per_thread << ")" << std::endl;

    // 验证最终状态
    assert(cache.size() == num_threads * ops_per_thread);
}

int main()
{
    cout << "LRUCache Template Test Program" << endl;

    example_basic_usage();
    example_eviction_callback();
    example_remove_and_clear();
    example_advanced_operations();
    example_thread_safety_demo();

    cout << "\nAll tests completed." << endl;
    return 0;
}