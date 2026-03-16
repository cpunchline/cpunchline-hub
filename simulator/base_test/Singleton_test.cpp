#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <memory>

#include "Singleton.hpp"

// 使用宏定义一个测试类
class Test
{
    SINGLETON_DECL(Test)

private:
    Test()
    {
        std::cout << "[Constructor] Test object created at " << this << std::endl;
    }

    ~Test()
    {
        std::cout << "[Destructor] Test object destroyed at " << this << std::endl;
    }

    friend class std::default_delete<Test>; // 允许 delete 调用私有析构

public:
    void doSomething()
    {
        std::cout << "Test instance working at " << this << std::endl;
    }
};

// 显式实例化静态成员
SINGLETON_IMPL(Test)

// 测试函数:模拟多线程获取单例
void worker(int id)
{
    // 模拟竞争时机
    std::this_thread::sleep_for(std::chrono::milliseconds(id * 100));
    Test *t = Test::instance();
    std::cout << "Thread #" << id << " got instance: " << t << std::endl;
    t->doSomething();
}

int main()
{
    std::cout << "=== Starting Singleton Test ===" << std::endl;

    // --- 1. 单线程测试 ---
    std::cout << "\n--- Single-thread Test ---" << std::endl;
    Test *t1 = Test::instance();
    Test *t2 = Test::instance();
    std::cout << "t1: " << t1 << std::endl;
    std::cout << "t2: " << t2 << std::endl;
    if (t1 == t2)
        std::cout << "✅ Single instance in single thread." << std::endl;
    else
        std::cout << "❌ Different instances!" << std::endl;

    t1->doSomething();

    // --- 2. 多线程并发测试 ---
    std::cout << "\n--- Multi-thread Test (10 threads) ---" << std::endl;
    std::vector<std::thread> threads;
    for (int i = 0; i < 10; ++i)
    {
        threads.emplace_back(worker, i + 1);
    }

    for (auto &th : threads)
    {
        th.join();
    }

    // --- 3. 销毁测试 ---
    std::cout << "\n--- Destroying Instance ---" << std::endl;
    Test::exitInstance();

    // --- 4. 再次创建测试(重新初始化)---
    std::cout << "\n--- Re-creating Instance ---" << std::endl;
    Test *t3 = Test::instance();
    std::cout << "Re-created instance at: " << t3 << std::endl;
    t3->doSomething();

    // 最后再次销毁
    std::cout << "\n--- Final Cleanup ---" << std::endl;
    Test::exitInstance();

    std::cout << "\n=== Test Complete ===" << std::endl;
    return 0;
}
