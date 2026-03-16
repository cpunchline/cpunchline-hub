#include <iomanip>
#include <iostream>
#include <chrono>
#include <thread>
#include <string>

#include "Thread.hpp"

// 具体任务类:继承 Thread 并实现 doTask
class MyTask : public Thread
{
public:
    MyTask(const std::string &name) :
        thread_name(name)
    {
    }

    virtual bool doPrepare() override
    {
        std::cout << "[" << thread_name << "] Preparing...\n";
        return true; // 返回 false 会直接退出线程
    }

    virtual void doTask() override
    {
        std::cout << "[" << thread_name << "] Doing task #" << dotask_cnt
                  << " (Status: " << getStatusStr() << ")\n";

        // 模拟一点工作量
        volatile int sum = 0;
        for (int i = 0; i < 10000; ++i)
            sum += i;
    }

    virtual bool doFinish() override
    {
        std::cout << "[" << thread_name << "] Cleaning up...\n";
        return true;
    }

    std::string thread_name;

private:
    std::string getStatusStr()
    {
        switch (status.load())
        {
            case STOP:
                return "STOP";
            case RUNNING:
                return "RUNNING";
            case PAUSE:
                return "PAUSE";
            default:
                return "UNKNOWN";
        }
    }
};

// 辅助函数:打印当前时间(用于 sleep_until 测试)
void print_time(const std::string &label)
{
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::cout << label << ": " << std::put_time(std::localtime(&t), "%H:%M:%S") << std::endl;
}

int main()
{
    std::cout << "=== Thread Class Test Program ===" << std::endl;

    // --- 测试1: 基本启动与停止 ---
    std::cout << "\n--- Test 1: Start and Stop ---" << std::endl;
    MyTask task1("Task1");
    task1.setSleepPolicy(Thread::SLEEP_FOR, 500); // 每次任务后 sleep 500ms

    task1.start();
    std::this_thread::sleep_for(std::chrono::seconds(2));
    task1.stop();

    std::cout << "[" << task1.thread_name << "] Total tasks executed: " << task1.dotask_cnt << std::endl;

    // --- 测试2: 暂停与恢复 ---
    std::cout << "\n--- Test 2: Pause and Resume ---" << std::endl;
    MyTask task2("Task2");
    task2.setSleepPolicy(Thread::SLEEP_FOR, 300);

    task2.start();
    std::this_thread::sleep_for(std::chrono::seconds(1));

    std::cout << "Pausing Task2...\n";
    task2.pause();
    std::this_thread::sleep_for(std::chrono::seconds(2));

    std::cout << "Resuming Task2...\n";
    task2.resume();
    std::this_thread::sleep_for(std::chrono::seconds(1));
    task2.stop();

    std::cout << "[" << task2.thread_name << "] Total tasks executed: " << task2.dotask_cnt << std::endl;

    // --- 测试3: 测试 sleep_until 策略(固定周期)---
    std::cout << "\n--- Test 3: SLEEP_UNTIL (Fixed Rate Execution) ---" << std::endl;
    print_time("Start");

    MyTask task3("Task3");
    task3.setSleepPolicy(Thread::SLEEP_UNTIL, 1000); // 每秒执行一次

    task3.start();
    std::this_thread::sleep_for(std::chrono::seconds(3));
    task3.stop();

    print_time("End");
    std::cout << "[" << task3.thread_name << "] Executed " << task3.dotask_cnt << " times (~3 times expected)\n";

    // --- 测试4: 多次启停 ---
    std::cout << "\n--- Test 4: Restart Multiple Times ---" << std::endl;
    MyTask task4("Task4");
    task4.setSleepPolicy(Thread::YIELD); // 高频任务

    for (int i = 0; i < 3; ++i)
    {
        std::cout << "=== Round " << (i + 1) << " ===" << std::endl;
        task4.start();
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        task4.stop();
        std::cout << "[" << task4.thread_name << "] Round " << (i + 1) << " - Tasks: " << task4.dotask_cnt << std::endl;
    }

    // --- 测试5: NO_SLEEP 高频运行 ---
    std::cout << "\n--- Test 5: NO_SLEEP (High Frequency) ---" << std::endl;
    MyTask task5("Task5");
    task5.setSleepPolicy(Thread::NO_SLEEP);

    auto start = std::chrono::steady_clock::now();
    task5.start();
    std::this_thread::sleep_for(std::chrono::seconds(1));
    task5.stop();
    auto end = std::chrono::steady_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "[" << task5.thread_name << "] NO_SLEEP: "
              << task5.dotask_cnt << " tasks in " << duration.count() << " ms\n";

    std::cout << "\n=== All Tests Completed ===" << std::endl;
    return 0;
}
