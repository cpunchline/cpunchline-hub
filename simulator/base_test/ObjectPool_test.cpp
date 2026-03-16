#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <chrono>
#include <atomic>
#include <functional>

#include "ObjectPool.hpp"

class MyTestObject
{
public:
    MyTestObject()
    {
        static int id_gen = 0;
        id_ = ++id_gen;
        std::cout << "MyTestObject created, ID: " << id_ << std::endl;
    }

    ~MyTestObject()
    {
        std::cout << "MyTestObject destroyed, ID: " << id_ << std::endl;
    }

    void sayHello() const
    {
        std::cout << "Hello from MyTestObject ID: " << id_ << std::endl;
    }

    int getId() const
    {
        return id_;
    }

private:
    int id_;
};

class MyTestObjectFactory
{
public:
    static MyTestObject *create()
    {
        std::cout << "[Factory] Creating new MyTestObject instance." << std::endl;
        return new MyTestObject();
    }
};

using MyPool = ObjectPool<MyTestObject, MyTestObjectFactory>;
using MyPoolObject = PoolObject<MyTestObject, MyTestObjectFactory>;

void test_basic_usage()
{
    std::cout << "\n=== Test: Basic Usage ===" << std::endl;
    MyPool pool(2, 4); // 初始2个,最多4个,超时默认3000ms

    std::cout << "Initial object count: " << pool.ObjectNum() << ", Idle: " << pool.IdleNum() << std::endl;

    auto obj1 = pool.Borrow();
    if (obj1)
    {
        obj1->sayHello();
    }

    auto obj2 = pool.Borrow();
    if (obj2)
    {
        obj2->sayHello();
    }

    std::cout << "After borrowing 2 objects -> Idle: " << pool.IdleNum() << ", Borrowed: " << pool.BorrowNum() << std::endl;

    // 归还
    pool.Return(obj1);
    std::cout << "After returning obj1 -> Idle: " << pool.IdleNum() << std::endl;

    pool.Return(obj2);
    std::cout << "After returning obj2 -> Idle: " << pool.IdleNum() << std::endl;
}

void test_try_borrow_and_timeout()
{
    std::cout << "\n=== Test: TryBorrow and Timeout ===" << std::endl;
    MyPool pool(0, 2); // 初始0,最多2个

    auto obj1 = pool.Borrow();
    auto obj2 = pool.Borrow();

    std::cout << "Borrowed 2 objects. Idle: " << pool.IdleNum() << std::endl;

    auto obj3 = pool.TryBorrow(); // 非阻塞
    if (!obj3)
    {
        std::cout << "TryBorrow returned NULL as expected (no idle)." << std::endl;
    }

    auto obj4 = pool.Borrow(); // 超时等待,但已达最大数,且无空闲
    if (!obj4)
    {
        std::cout << "Borrow() timed out or failed as expected." << std::endl;
    }

    // 归还一个,让 Borrow() 成功
    pool.Return(obj1);
    auto obj5 = pool.Borrow();
    if (obj5)
    {
        std::cout << "Successfully borrowed after return. ID: " << obj5->getId() << std::endl;
    }
}

void test_pool_object_raii()
{
    std::cout << "\n=== Test: PoolObject (RAII wrapper) ===" << std::endl;
    MyPool pool(1, 3);

    {
        MyPoolObject pobj(pool);
        if (pobj)
        {
            pobj->sayHello();
        }
        else
        {
            std::cout << "Failed to borrow via PoolObject!" << std::endl;
        }
        // 自动归还发生在析构时
    }

    std::cout << "After PoolObject destroyed, Idle count: " << pool.IdleNum() << std::endl;
}

void worker_thread(MyPool &pool, int thread_id, std::atomic<int> &success_count)
{
    // 模拟借用对象做工作
    auto obj = pool.Borrow();
    if (obj)
    {
        std::cout << "Thread " << thread_id << " got object ID: " << obj->getId() << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(500)); // 模拟使用
        pool.Return(obj);                                            // 显式归还
        success_count++;
    }
    else
    {
        std::cout << "Thread " << thread_id << " failed to borrow object (timeout)." << std::endl;
    }
}

void test_multithreading()
{
    std::cout << "\n=== Test: Multithreading ===" << std::endl;
    MyPool pool(2, 3); // 限制最大3个对象
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};

    auto start = std::chrono::steady_clock::now();

    // 创建5个线程,同时请求对象
    for (int i = 0; i < 5; ++i)
    {
        threads.emplace_back(worker_thread, std::ref(pool), i, std::ref(success_count));
    }

    for (auto &t : threads)
    {
        t.join();
    }

    auto end = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "Multithread test done. Elapsed: " << elapsed.count() << " ms" << std::endl;
    std::cout << "Successfully borrowed objects: " << success_count << "/5" << std::endl;
    std::cout << "Final pool stats -> Total: " << pool.ObjectNum() << ", Idle: " << pool.IdleNum() << std::endl;
}

void test_add_remove_clear()
{
    std::cout << "\n=== Test: Add, Remove, Clear ===" << std::endl;
    MyPool pool(0, 3);

    auto obj = std::make_shared<MyTestObject>();
    bool added = pool.Add(obj);
    std::cout << "Add object: " << (added ? "Success" : "Failed") << std::endl;
    std::cout << "After Add -> Total: " << pool.ObjectNum() << ", Idle: " << pool.IdleNum() << std::endl;

    // 再添加一个
    auto obj2 = std::make_shared<MyTestObject>();
    bool added2 = pool.Add(obj2);
    std::cout << "Add second object: " << (added2 ? "Success" : "Failed") << std::endl;

    // 尝试移除
    bool removed = pool.Remove(obj);
    std::cout << "Remove first object: " << (removed ? "Success" : "Failed") << std::endl;
    std::cout << "After Remove -> Total: " << pool.ObjectNum() << std::endl;

    pool.Clear();
    std::cout << "After Clear -> Total: " << pool.ObjectNum() << ", Idle: " << pool.IdleNum() << std::endl;
}

int main()
{
    std::cout << "Starting ObjectPool Tests...\n";

    test_basic_usage();
    test_try_borrow_and_timeout();
    test_pool_object_raii();
    test_multithreading();
    test_add_remove_clear();

    std::cout << "\nAll tests completed." << std::endl;
    return 0;
}
