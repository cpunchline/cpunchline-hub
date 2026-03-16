#pragma once

#include <atomic>
#include <iostream>
#include <set>
#include <queue>
#include <unordered_map>
#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>
#include "ThreadPool.hpp"

class TimerCluster
{
public:
    using TimerId = uint32_t;
    using TimerUserData = void *; // WARNING: cb and usedata should keep valid forever
    using TimerCallback = std::function<void(TimerId, TimerUserData)>;

    enum class TimerType
    {
        SINGLE = 1,
        CYCLE = 2,
    };

    struct TimerInfo
    {
        TimerId timer_id;
        TimerType timer_type;
        uint32_t timer_interval;
        std::chrono::steady_clock::time_point timer_expire;
        TimerCallback timer_cb;
        TimerUserData timer_userdata;
    };

    TimerCluster(bool support_threadpool = false) :
        timer_support_threadpool(support_threadpool),
        timer_run(true)
    {
        timer_thread = std::make_unique<std::thread>(&TimerCluster::Processer, this);
        if (timer_support_threadpool)
        {
            timer_threadpool = std::make_unique<ThreadPool>(DEFAULT_THREAD_POOL_MIN_THREAD_NUM, DEFAULT_THREAD_POOL_MAX_THREAD_NUM, DEFAULT_THREAD_POOL_MAX_IDLE_TIME);
            timer_threadpool->start(DEFAULT_THREAD_POOL_MIN_THREAD_NUM);
        }
        std::cout << "TimerCluster inited" << std::endl;
    }

    ~TimerCluster()
    {
        timer_run.store(false);
        timer_cv.notify_all();

        if (timer_thread->joinable())
        {
            timer_thread->join();
        }
        std::cout << "TimerCluster destroyed" << std::endl;
    }

    bool TimerExist(TimerId id)
    {
        std::lock_guard<std::mutex> lock(timer_mutex);
        return timer_map.find(id) != timer_map.end();
    }

    bool AddTimer(TimerId id, TimerType type, uint32_t timer_interval, const TimerCallback &cb, TimerUserData userdata = nullptr)
    {
        std::lock_guard<std::mutex> lock(timer_mutex);
        if (timer_map.find(id) != timer_map.end())
        {
            std::cout << "timer_id[" << id << "] is existed" << std::endl;
            return false;
        }

        auto now = std::chrono::steady_clock::now();
        auto expire = now + std::chrono::milliseconds(timer_interval);
        auto timer_info = std::make_shared<TimerInfo>(TimerInfo{id, type, timer_interval, expire, cb, userdata});
        timer_set.insert(timer_info);
        timer_map[id] = timer_info;

        timer_cv.notify_one();

        std::cout << "timer_id[" << timer_info->timer_id << "] add success" << std::endl;

        return true;
    }

    bool DelTimer(TimerId id)
    {
        std::lock_guard<std::mutex> lock(timer_mutex);
        auto it = timer_map.find(id);
        if (it == timer_map.end())
        {
            std::cout << "timer_id[" << id << "] is not existed or already deleted" << std::endl;
            return false;
        }

        auto timer_info = it->second;
        timer_set.erase(timer_info);
        timer_map.erase(it);

        timer_cv.notify_one();

        std::cout << "timer_id[" << timer_info->timer_id << "] del success" << std::endl;

        return true;
    }

    bool ResetTimer(TimerId id, uint32_t new_interval)
    {
        std::lock_guard<std::mutex> lock(timer_mutex);
        auto it = timer_map.find(id);
        if (it == timer_map.end())
        {
            std::cout << "timer_id[" << id << "] is not existed or already deleted" << std::endl;
            return false;
        }

        auto timer_info = it->second;
#if 0
        if (timer_info->timer_interval == new_interval)
        {
            return false;
        }
#endif

        timer_set.erase(timer_info);
        auto now = std::chrono::steady_clock::now();
        timer_info->timer_interval = new_interval;
        timer_info->timer_expire = now + std::chrono::milliseconds(new_interval);
        timer_set.insert(timer_info);

        timer_cv.notify_one();
        std::cout << "timer_id[" << timer_info->timer_id << "] reset success" << std::endl;

        return true;
    }

private:
    struct TimerInfoCompare
    {
        bool operator()(const std::shared_ptr<TimerInfo> &lhs, const std::shared_ptr<TimerInfo> &rhs) const
        {
            if (lhs->timer_expire != rhs->timer_expire)
                return lhs->timer_expire < rhs->timer_expire;
            return lhs->timer_id < rhs->timer_id;
        }
    };

    bool timer_support_threadpool;
    std::atomic_bool timer_run;
    std::set<std::shared_ptr<TimerInfo>, TimerInfoCompare> timer_set;
    std::unordered_map<TimerId, std::shared_ptr<TimerInfo>> timer_map;
    std::mutex timer_mutex;
    std::condition_variable timer_cv;
    std::unique_ptr<std::thread> timer_thread;
    std::unique_ptr<ThreadPool> timer_threadpool;
    void Processer()
    {
        std::cout << "TimerCluster run" << std::endl;
        while (true)
        {
            std::unique_lock<std::mutex> lock(timer_mutex);
            timer_cv.wait(lock,
                          [this]
                          {
                              return !timer_run.load() || !timer_set.empty();
                          });
            if (!timer_run.load())
            {
                break;
            }

            auto timer_info = *timer_set.begin();
            TimerId timer_id = timer_info->timer_id;
            TimerUserData timer_userdata = timer_info->timer_userdata;
            auto now = std::chrono::steady_clock::now();

            if (timer_info->timer_expire <= now)
            {
                timer_set.erase(timer_set.begin());
                std::cout << "timer_id[" << timer_id << "] timeout" << std::endl;

                if (timer_info->timer_type == TimerType::CYCLE)
                {
                    timer_info->timer_expire = now + std::chrono::milliseconds(timer_info->timer_interval);
                    timer_set.insert(timer_info);
                }
                else
                {
                    timer_map.erase(timer_id);
                    std::cout << "timer_id[" << timer_id << "] auto del" << std::endl;
                }

                if (timer_info->timer_cb)
                {
                    lock.unlock();
                    if (timer_support_threadpool)
                    {
                        timer_threadpool->commit(timer_info->timer_cb, timer_id, timer_userdata);
                    }
                    else
                    {
                        timer_info->timer_cb(timer_id, timer_userdata);
                    }
                    lock.lock();
                }
            }
            else
            {
                timer_cv.wait_until(lock, timer_info->timer_expire); // bug: https://en.cppreference.com/w/cpp/thread/condition_variable/wait_until
            }
        }
        std::cout << "TimerCluster stoped" << std::endl;
    }
};
