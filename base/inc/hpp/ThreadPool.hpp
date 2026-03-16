#pragma once

#include <cstring>
#include <iostream>
#include <atomic>
#include <thread>
#include <functional>
#include <future>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <list>
#include <queue>
#include <chrono>
#include <sstream>

#include "Singleton.hpp"

// use thirdparty/thread-pool to replace the IMPL;

#define DEFAULT_THREAD_POOL_MIN_THREAD_NUM 1
#define DEFAULT_THREAD_POOL_MAX_THREAD_NUM std::thread::hardware_concurrency()
#define DEFAULT_THREAD_POOL_MAX_IDLE_TIME  60000 // ms

class FixThreadPool
{
public:
    using Task = std::function<void()>;

    FixThreadPool(size_t num = DEFAULT_THREAD_POOL_MIN_THREAD_NUM) :
        stop_(false), thread_num_(num)
    {
        if (0 == thread_num_)
        {
            thread_num_ = DEFAULT_THREAD_POOL_MIN_THREAD_NUM;
        }
    }

    ~FixThreadPool()
    {
        stop();
    }

    template <class F, class... Args>
    auto commit(F &&f, Args &&...args) -> std::future<decltype(std::forward<F>(f)(std::forward<Args>(args)...))>
    {
        using RetType = decltype(std::forward<F>(f)(std::forward<Args>(args)...));
        if (this->stop_.load())
            // return std::future<RetType>{};
            throw std::runtime_error("enqueue on stopped ThreadPool");

        auto task = std::make_shared<std::packaged_task<RetType()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...));

        std::future<RetType> ret = task->get_future();
        {
            std::lock_guard<std::mutex> cv_mt(cv_mt_);
            if (this->pool_.empty())
            {
                start();
            }
            this->tasks_.emplace([task]
                                 {
                                     (*task)();
                                 });
        }
        this->cv_lock_.notify_one();
        return ret;
    }

private:
    void start()
    {
        for (size_t i = 0; i < thread_num_; ++i)
        {
            pool_.emplace_back([this]()
                               {
                                   while (true)
                                   {
                                       Task task;
                                       {
                                           std::unique_lock<std::mutex> cv_mt(cv_mt_);
                                           this->cv_lock_.wait(cv_mt, [this]
                                                               {
                                                                   return this->stop_.load() || !this->tasks_.empty();
                                                               });
                                           if (this->stop_.load())
                                               return;

                                           task = std::move(this->tasks_.front());
                                           this->tasks_.pop();
                                       }
                                       task();
                                   }
                               });
        }
    }

    void stop()
    {
        stop_.store(true);
        cv_lock_.notify_all();
        for (auto &td : pool_)
        {
            if (td.joinable())
            {
                td.join();
            }
        }
    }

private:
    std::mutex cv_mt_;
    std::condition_variable cv_lock_;
    std::atomic_bool stop_;
    std::size_t thread_num_;
    std::queue<Task> tasks_;
    std::vector<std::thread> pool_;
};

class ThreadPool
{
public:
    using Task = std::function<void()>;

    ThreadPool(size_t min_threads = DEFAULT_THREAD_POOL_MIN_THREAD_NUM,
               size_t max_threads = DEFAULT_THREAD_POOL_MAX_THREAD_NUM,
               size_t max_idle_ms = DEFAULT_THREAD_POOL_MAX_IDLE_TIME) :
        min_thread_num(min_threads), max_thread_num(max_threads), max_idle_time(max_idle_ms), status(STOP), cur_thread_num(0), idle_thread_num(0)
    {
    }

    virtual ~ThreadPool()
    {
        stop();
    }

    void setMinThreadNum(size_t min_threads)
    {
        min_thread_num = min_threads;
    }
    void setMaxThreadNum(size_t max_threads)
    {
        max_thread_num = max_threads;
    }
    void setMaxIdleTime(size_t ms)
    {
        max_idle_time = ms;
    }
    size_t currentThreadNum()
    {
        return cur_thread_num;
    }
    size_t idleThreadNum()
    {
        return idle_thread_num;
    }
    size_t taskNum()
    {
        std::lock_guard<std::mutex> locker(task_mutex);
        return tasks.size();
    }
    bool isStarted()
    {
        return status != STOP;
    }
    bool isStopped()
    {
        return status == STOP;
    }

    int start(size_t start_threads = 0)
    {
        if (status != STOP)
            return -1;
        status = RUNNING;
        if (start_threads < min_thread_num)
            start_threads = min_thread_num;
        if (start_threads > max_thread_num)
            start_threads = max_thread_num;
        for (size_t i = 0; i < start_threads; ++i)
        {
            createThread();
        }
        return 0;
    }

    int stop()
    {
        if (status == STOP)
        {
            return -1;
        }
        status = STOP;
        task_cond.notify_all();
        for (auto &i : threads)
        {
            if (i.thread->joinable())
            {
                i.thread->join();
            }
        }
        threads.clear();
        cur_thread_num = 0;
        idle_thread_num = 0;
        return 0;
    }

    int pause()
    {
        if (status == RUNNING)
        {
            status = PAUSE;
        }
        return 0;
    }

    int resume()
    {
        if (status == PAUSE)
        {
            status = RUNNING;
        }
        return 0;
    }

    int wait()
    {
        while (status != STOP)
        {
            if (tasks.empty() && idle_thread_num == cur_thread_num)
            {
                break;
            }
            std::this_thread::yield();
        }
        return 0;
    }

    /*
     * return a future, calling future.get() will wait task done and return RetType.
     * commit(fn, args...)
     * commit(std::bind(&Class::mem_fn, &obj))
     * commit(std::mem_fn(&Class::mem_fn, &obj))
     *
     */
    template <class Fn, class... Args>
    auto commit(Fn &&fn, Args &&...args) -> std::future<decltype(std::forward<Fn>(fn)(std::forward<Args>(args)...))>
    {
        if (status == STOP)
            start();
        if (idle_thread_num <= tasks.size() && cur_thread_num < max_thread_num)
        {
            createThread();
        }
        using RetType = decltype(std::forward<Fn>(fn)(std::forward<Args>(args)...));
        auto task = std::make_shared<std::packaged_task<RetType()>>(
            std::bind(std::forward<Fn>(fn), std::forward<Args>(args)...));
        std::future<RetType> future = task->get_future();
        {
            std::lock_guard<std::mutex> locker(task_mutex);
            tasks.emplace([task]
                          {
                              (*task)();
                          });
        }

        task_cond.notify_one();
        return future;
    }

protected:
    bool createThread()
    {
        if (cur_thread_num >= max_thread_num)
            return false;
        std::thread *thread = new std::thread([this]
                                              {
                                                  while (status != STOP)
                                                  {
                                                      while (status == PAUSE)
                                                      {
                                                          std::this_thread::yield();
                                                      }

                                                      Task task;
                                                      {
                                                          std::unique_lock<std::mutex> locker(task_mutex);
                                                          task_cond.wait_for(locker, std::chrono::milliseconds(max_idle_time), [this]()
                                                                             {
                                                                                 return status == STOP || !tasks.empty();
                                                                             });
                                                          if (status == STOP)
                                                              return;
                                                          if (tasks.empty())
                                                          {
                                                              if (cur_thread_num > min_thread_num)
                                                              {
                                                                  delThread(std::this_thread::get_id());
                                                                  return;
                                                              }
                                                              continue;
                                                          }
                                                          --idle_thread_num;
                                                          task = std::move(tasks.front());
                                                          tasks.pop();
                                                      }
                                                      if (task)
                                                      {
                                                          task();
                                                          ++idle_thread_num;
                                                      }
                                                  }
                                              });
        addThread(thread);
        return true;
    }

    void addThread(std::thread *thread)
    {
        thread_mutex.lock();
        ++cur_thread_num;
        ++idle_thread_num;
        ThreadData data;
        data.thread = std::shared_ptr<std::thread>(thread);
        data.id = thread->get_id();
        data.status = RUNNING;
        data.start_time = std::chrono::steady_clock::now();
        data.stop_time = std::chrono::steady_clock::time_point();
        threads.emplace_back(data);
        thread_mutex.unlock();
    }

    void delThread(std::thread::id id)
    {
        auto now = std::chrono::steady_clock::now();
        thread_mutex.lock();
        --cur_thread_num;
        --idle_thread_num;
        auto iter = threads.begin();
        while (iter != threads.end())
        {
            if (iter->status == STOP && now > iter->stop_time)
            {
                // join other STOPED's thread
                if (iter->thread->joinable())
                {
                    iter->thread->join();
                    iter = threads.erase(iter);
                    continue;
                }
            }
            else if (iter->id == id)
            {
                // mark STOP;
                iter->status = STOP;
                iter->stop_time = std::chrono::steady_clock::now();
            }
            ++iter;
        }
        thread_mutex.unlock();
    }

public:
    size_t min_thread_num;
    size_t max_thread_num;
    size_t max_idle_time;

protected:
    enum Status
    {
        STOP,
        RUNNING,
        PAUSE,
    };
    struct ThreadData
    {
        std::shared_ptr<std::thread> thread;
        std::thread::id id;
        Status status;
        std::chrono::steady_clock::time_point start_time;
        std::chrono::steady_clock::time_point stop_time;
    };
    std::atomic<Status> status;
    std::atomic<size_t> cur_thread_num;
    std::atomic<size_t> idle_thread_num;
    std::list<ThreadData> threads;
    std::mutex thread_mutex;
    std::queue<Task> tasks;
    std::mutex task_mutex;
    std::condition_variable task_cond;
};

class GlobalThreadPool : public ThreadPool
{
    SINGLETON_DECL(GlobalThreadPool)
protected:
    GlobalThreadPool() :
        ThreadPool()
    {
    }

    ~GlobalThreadPool()
    {
    }
};

/*
 * return a future, calling future.get() will wait task done and return RetType.
 * async(fn, args...)
 * async(std::bind(&Class::mem_fn, &obj))
 * async(std::mem_fn(&Class::mem_fn, &obj))
 *
 */
template <class Fn, class... Args>
auto async(Fn &&fn, Args &&...args) -> std::future<decltype(std::forward<Fn>(fn)(std::forward<Args>(args)...))>
{
    return GlobalThreadPool::instance()->commit(std::forward<Fn>(fn), std::forward<Args>(args)...);
}

class async
{
public:
    static void startup(size_t min_threads = DEFAULT_THREAD_POOL_MIN_THREAD_NUM,
                        size_t max_threads = DEFAULT_THREAD_POOL_MAX_THREAD_NUM,
                        size_t max_idle_ms = DEFAULT_THREAD_POOL_MAX_IDLE_TIME)
    {
        GlobalThreadPool *gtp = GlobalThreadPool::instance();
        if (gtp->isStarted())
            return;
        gtp->setMinThreadNum(min_threads);
        gtp->setMaxThreadNum(max_threads);
        gtp->setMaxIdleTime(max_idle_ms);
        gtp->start();
    }

    static void cleanup()
    {
        GlobalThreadPool::exitInstance();
    }
};
