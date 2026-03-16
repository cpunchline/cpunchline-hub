#pragma once

#include <iostream>
#include <functional>
#include <thread>
#include <iomanip>
#include <chrono>

/*
并行: 多核机器的真正并行.
并发: 单核机器的任务切换.
*/

// 在标准 C++ 中, 线程就是 std::thread (C++20 std::jthread)

#define DEFAULT_THREAD_POOL_MAX_THREAD_NUM std::thread::hardware_concurrency()
// 当前硬件支持的并发线程数量,它是 std::thread 的静态成员函数
/*
英特尔® 超线程技术是一项硬件创新,允许在每个内核上运行多个线程.更多的线程意味着可以并行完成更多的工作.
AMD 超线程技术被称为 SMT(Simultaneous Multi-Threading),它与英特尔的技术实现有所不同,不过使用类似.

举个例子:一款 4 核心 8 线程的 CPU,这里的 8 线程其实是指所谓的逻辑处理器,也意味着这颗 CPU 最多可并行执行 8 个任务.我们的 hardware_concurrency() 获取的值自然也会是 8.

当然了,都 2024 年了,我们还得考虑一个问题:“ 英特尔从 12 代酷睿开始,为其处理器引入了全新的“大小核”混合设计架构”.
比如我的 CPU i7 13700H 它是 14 核心,20 线程,有 8 个能效核,6 个性能核.不过我们说了,物理核心这个通常不看重,hardware_concurrency() 输出的值会为 20.
在进行多线程编程时,我们可以参考此值来确定创建的线程数量,以更好地利用当前硬件,从而提升程序性能.
*/

// “线程管理”,其实也就是指管理 std::thread 对象.

class Task
{
public:
    void operator()() const
    {
        std::cout << "operator()()const\n";
    }
};

void hello()
{ // 定义一个函数用作打印任务
    std::cout << "Hello World" << std::endl;
}

struct func
{
    int &m_i;
    func(int &i) :
        m_i{i}
    {
    }
    void operator()(int n) const
    {
        for (int i = 0; i <= n; ++i)
        {
            m_i += i; // 可能悬空引用
        }
    }
};

void test_thread_class()
{
    // std::thread
    int m = 0;
    int n = 0;

    std::thread default_t;  // std::thread 线程对象没有关联线程的,自然也不会启动线程执行任务
    std::thread t1{hello};  // 普通函数
    std::thread t2{Task{}}; // 重载了()的类
    std::thread t3{[]
                   {
                       std::this_thread::sleep_for(std::chrono::seconds(3));
                       std::cout << std::this_thread::get_id() << '\n';
                       std::cout << "Hello World!\n";
                   }}; // lambda表达式
    std::thread t4{func{n}, 100};         // 向可调用对象传递参数很简单; 这些参数会复制到新线程的内存空间中,即使函数中的参数是引用,依然实际是按值复制;
    std::thread t5{func{n}, std::ref(m)}; // 如果需要使用引用, 应使用std::ref

    struct X
    {
        void task_run(int &a) const
        {
            std::cout << &a << '\n';
        }
    };

    X x;
    std::thread t6{&X::task_run, &x, n};                      // 成员函数
    std::thread t7{std::bind(&X::task_run, &x, std::ref(n))}; // std::bind 也是默认按值复制的,即使我们的成员函数形参类型为引用

#if 0
    std::reference_wrapper<int> r1 = std::ref(m);
    int &p1 = r1; // r1 隐式转换为 m 的引用 此时 p1 引用的就是 m

    std::reference_wrapper<const int> r2 = std::cref(m);
    const int &p2 = r2; // r2 隐式转换为 m 的 const 的引用 此时 p2 引用的就是 m
#endif

    // 创建了一个线程对象 t,将 hello 作为它的可调用(Callable)对象,在新线程中执行.
    // 线程对象关联了一个线程资源,我们无需手动控制,在线程对象构造成功,就自动在新线程开始执行函数 hello

    // todo.. 不能写 抛出异常的代码; 避免程序被抛出的异常所终止,在异常处理过程中调用 join(),从而避免线程对象析构产生问题.
    t1.join();
    t2.join();
    t3.join();
    t4.detach(); // joinable() 为 false.线程分离,线程对象不再持有线程资源,线程独立的运行
    t5.join();
    t6.join();
    t7.join();

    // 启动线程后(也就是构造 std::thread 对象)我们必须在线程对象的生存期结束之前,即 std::thread::~thread 调用之前,决定它的执行策略,是 join()(合并)还是 detach()(分离)
    // 必须是 std::thread 的 joinable() 为 true 即线程对象有活跃线程,才能调用 join() 和 detach().

    // join
    // 等待线程对象 t 关联的线程执行完毕,否则将一直阻塞.
    // 这里的调用是必须的,否则 std::thread 的析构函数将调用 std::terminate() 无法正确析构.
    // 这是因为我们创建线程对象 t 的时候就关联了一个活跃的线程,调用 join() 就是确保线程对象关联的线程已经执行完毕,然后会修改对象的状态,让 std::thread::joinable() 返回 false,表示线程对象目前没有关联活跃线程.
    // std::thread 的析构函数,正是通过 joinable() 判断线程对象目前是否有关联活跃线程,如果为 true,那么就当做有关联活跃线程,会调用 std::terminate().

    // detach
    // 当 std::thread 线程对象调用了 detach(),那么就是线程对象放弃了对线程资源的所有权,不再管理此线程,允许此线程独立的运行,在线程退出时释放所有分配的资源
    // 放弃了对线程资源的所有权,也就是线程对象没有关联活跃线程了,此时 joinable 为 false
    // 在单线程的代码中,对象销毁之后再去访问,会产生未定义行为,多线程增加了这个问题发生的几率.

    // 通常非常不推荐使用 detach(),因为程序员必须确保所有创建的线程正常退出,释放所有获取的资源并执行其它必要的清理操作.
    // 这意味着通过调用 detach() 放弃线程的所有权不是一种选择,因此 join 应该在所有场景中使用.
} // 分离的线程可能还在运行
// 主线程不等待,此时分离的子线程可能没有执行完毕,但是主线程(main)已经结束,局部对象 n 生存期结束,被销毁,而此时子线程还持有它的引用,访问悬空引用,造成未定义行为.
// t4 已经没有关联线程资源,正常析构,没有问题.

void test_this_thread_class()
{
    /*
    std::this_thread
    这个命名空间包含了管理当前线程的函数.
        yield 建议实现重新调度各执行线程.
        get_id 返回当前线程 id.
        sleep_for 使当前线程停止执行指定时间.
        sleep_until 使当前线程执行停止到指定的时间点.sleep_until 本身设置使用很简单,是打印时间格式,设置时区麻烦
    */

    // 获取当前时间点
    auto now = std::chrono::system_clock::now();

    // 设置要等待的时间点为当前时间点之后的5秒
    auto wakeup_time = now + std::chrono::seconds(5);

    // 输出当前时间
    auto now_time = std::chrono::system_clock::to_time_t(now);
    std::cout << "Current time:\t\t" << std::put_time(std::localtime(&now_time), "%H:%M:%S") << std::endl;

    // 输出等待的时间点
    auto wakeup_time_time = std::chrono::system_clock::to_time_t(wakeup_time);
    std::cout << "Waiting until:\t\t" << std::put_time(std::localtime(&wakeup_time_time), "%H:%M:%S") << std::endl;

    // 等待到指定的时间点
    std::this_thread::sleep_until(wakeup_time);

    // 输出等待结束后的时间
    now = std::chrono::system_clock::now();
    now_time = std::chrono::system_clock::to_time_t(now);
    std::cout << "Time after waiting:\t" << std::put_time(std::localtime(&now_time), "%H:%M:%S") << std::endl;
}

void f(std::thread t)
{
    t.join();
}

void test_thread_move()
{
    std::thread t{[]
                  {
                      std::cout << std::this_thread::get_id() << '\n';
                  }};
    std::cout << t.joinable() << '\n'; // 线程对象 t 当前关联了活跃线程 打印 1
    std::thread t2{std::move(t)};      // 将 t 的线程资源的所有权移交给 t2
    std::cout << t.joinable() << '\n'; // 线程对象 t 当前没有关联活跃线程 打印 0
    // t.join(); // Error! t 没有线程资源
    t2.join(); // t2 当前持有线程资源
    // 这段代码通过移动构造转移了线程对象 t 的线程资源所有权到 t2,这里虽然有两个 std::thread 对象,但是从始至终只有一个线程资源,让持有线程资源的 t2 对象最后调用 join() 阻塞让其线程执行完毕.
    // t 与 t2 都能正常析构.

    t2 = std::thread([]
                     {
                     }); // 临时对象是右值表达式,不用调用 std::move,这里相当于是将临时的 std::thread 对象所持有的线程资源转移给 t2,t2 再调用 join() 正常析构
    t2.join();

    std::thread t3{[]
                   {
                   }};
    f(std::move(t3)); // std::move 将 t 转换为了一个右值表达式,初始化函数f 形参 t,选择到了移动构造转移线程资源的所有权,在函数中调用 t.join() 后正常析构.
    f(std::thread{[]
                  {
                  }}); // std::thread{ [] {} } 构造了一个临时对象,本身就是右值表达式,初始化函数f 形参 t,移动构造转移线程资源的所有权到 t,t.join() 后正常析构.
}

void test_cpp_20_jthread()
{
    /*
    std::jthread 相比于 C++11 引入的 std::thread,只是多了两个功能:
        RAII 管理:在析构时自动调用 join().
        线程停止功能:线程的取消/停止.
    */
}
