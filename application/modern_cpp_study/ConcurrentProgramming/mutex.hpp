#pragma once

#include <iostream>
#include <map>
#include <thread>
#include <vector>
#include <mutex>
#include <shared_mutex>

std::mutex m;
std::recursive_mutex n;
/*
当某个表达式的求值写入某个内存位置, 而另一求值读或修改同一内存位置时, 称这些表达式冲突.拥有两个冲突的求值的程序就有数据竞争, 除非
    两个求值都在同一线程上, 或者在同一信号处理函数中执行, 或
    两个冲突的求值都是原子操作(见 std::atomic), 或
    一个冲突的求值发生早于 另一个(见 std::memory_order)

如果出现数据竞争, 那么程序的行为未定义.
*/

void f1()
{
    m.lock();
    // m.try_lock(); // 在需要保护临界区的同时, 又不想线程因为等待锁而阻塞的情况下
    // 被 lock() 和 unlock() 包含在其中的代码是线程安全的, 同一时间只有一个线程执行, 不会被其它线程的执行所打断
    std::cout << std::this_thread::get_id() << '\n';
    m.unlock();
}

void f2()
{
    // 不推荐这样显式的 lock() 与 unlock(), 我们可以使用 C++11 标准库引入的"管理类" std::lock_guard:
    std::lock_guard<std::mutex> lc{m}; // std::unique_lock<std::mutex> lc{m};
    // std::lock_guard lc{m};
    // std::scoped_lock lc{m};
    // std::scoped_lock<std::mutex> lc{m};
    std::cout << std::this_thread::get_id() << '\n';
}

void f3()
{
    // code..
    {
        // 使用 {} 创建了一个块作用域, 限制了对象 lc 的生存期, 进入作用域构造 lock_guard 的时候上锁(lock), 离开作用域析构的时候解锁(unlock).
        // 我们要尽可能的让互斥量上锁的粒度小, 只用来确保必须的共享资源的线程安全.
        // "粒度"通常用于描述锁定的范围大小, 较小的粒度意味着锁定的范围更小, 因此有更好的性能和更少的竞争.
        std::lock_guard<std::mutex> lc{m};
        // 涉及共享资源的修改的代码...
    }
    // code..
}

void test_mutex()
{
    std::vector<std::thread> threads;
    for (std::size_t i = 0; i < 10; ++i)
        threads.emplace_back(f1);

    for (auto &thread : threads)
        thread.join();
}

// 多个互斥量才可能遇到死锁问题.
/*
两个线程需要对它们所有的互斥量做一些操作, 其中每个线程都有一个互斥量, 且等待另一个线程的互斥量解锁.
因为它们都在等待对方释放互斥量, 没有线程工作. 这种情况就是死锁

避免死锁的一般建议是让两个互斥量以相同的顺序上锁, 总在互斥量 B 之前锁住互斥量 A, 就通常不会死锁.

, 可以使用 std::lock or std::scoped_lock, 它能一次性锁住多个互斥量, 并且没有死锁风险
*/

/*
使用 std::defer_lock 构造函数不上锁, 要求构造之后上锁; 没有所有权自然构造函数就不会上锁
使用 std::adopt_lock 构造函数不上锁, 要求在构造之前互斥量上锁; std::adopt_lock 只是不上锁, 但是有所有权
默认构造会上锁, 要求构造函数之前和构造函数之后都不能再次上锁
*/

// 读写锁
// std::shared_timed_mutex 具有 std::shared_mutex 的所有功能, 并且额外支持超时功能.所以以上代码可以随意更换这两个互斥量.
class Settings
{
private:
    std::map<std::string, std::string> data_;
    mutable std::shared_mutex mutex_;
    /*
    "M&M 规则": mutable 与 mutex 一起出现
    "M&M 规则" 是一种经验法则, 指出: 如果你在类中使用 mutable 成员变量, 通常也应该为该类引入一个互斥量(如 std::mutex 或 std::shared_mutex), 以确保在多线程环境下从常量方法(const 方法)中访问或修改这些 mutable 成员是线程安全的.
    */

public:
    void set(const std::string &key, const std::string &value)
    {
        std::lock_guard<std::shared_mutex> lock{mutex_}; // 获取独占锁
        data_[key] = value;
    }

    std::string get(const std::string &key) const
    {
        std::shared_lock<std::shared_mutex> lock(mutex_); // 获取共享锁
        auto it = data_.find(key);
        return (it != data_.end()) ? it->second : ""; // 如果没有找到键返回空字符串
    }
};

// std::recursive_mutex
// unlock 必须和 lock 的调用次数一样, 才会真正解锁互斥量
/*
std::recursive_mutex 是 C++ 标准库提供的一种互斥量类型, 它允许同一线程多次锁定同一个互斥量, 而不会造成死锁.
当同一线程多次对同一个 std::recursive_mutex 进行锁定时, 只有在解锁与锁定次数相匹配时, 互斥量才会真正释放.
但它并不影响不同线程对同一个互斥量进行锁定的情况.不同线程对同一个互斥量进行锁定时, 会按照互斥量的规则进行阻塞,
*/
void recursive_function(int count)
{
    std::lock_guard<std::recursive_mutex> lc{n};
    std::cout << "Locked by thread: " << std::this_thread::get_id() << ", count: " << count << std::endl;
    if (count > 0)
    {
        recursive_function(count - 1);
    }
}

// thread_local 线程局部存储
// 每一个线程都有独立的 thread_local_counter 对象, 它们不是同一个
// 在 MSVC 的实现中, 如果 std::async 策略为 launch::async , 但却并不是每次都创建一个新的线程, 而是从线程池获取线程.
// 这意味着无法保证线程局部变量在任务完成时会被销毁.
// 如果线程被回收并用于新的 std::async 调用, 则旧的线程局部变量仍然存在.因此, 建议不要将线程局部变量与 std::async 一起使用
int global_counter = 0;
thread_local int thread_local_counter = 0;

void print_counters()
{
    std::cout << "global:" << global_counter++ << '\n';
    std::cout << "thread_local:" << thread_local_counter++ << '\n';
}

void test()
{
    std::thread{print_counters}.join();
    std::thread{print_counters}.join();
}
