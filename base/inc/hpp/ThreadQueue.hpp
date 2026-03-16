#pragma once

#include <deque>
#include <mutex>
#include <condition_variable>
#include <atomic>

// use thirdparty/concurrentqueue to replace the IMPL;

template <typename T>
class UnBoundedQueue
{
public:
    ~UnBoundedQueue()
    {
        Quit();
    }

    void Quit()
    {
        quit_.store(true);
        cond_notEmpty_.notify_all();
    }

    bool Empty() const
    {
        std::lock_guard<std::mutex> guard{mutex_};
        return deque_.empty();
    }

    void Clear()
    {
        std::lock_guard<std::mutex> guard{mutex_};
        deque_.clear();
    }

    std::size_t Size() const noexcept
    {
        std::lock_guard<std::mutex> guard{mutex_};
        return deque_.size();
    }

    bool Push(const T &val)
    {
        return PushImpl(val);
    }

    bool Push(T &&val)
    {
        return PushImpl(std::forward<T>(val));
    }

    bool Pop(T &val)
    {
        std::unique_lock lock{mutex_};
        cond_notEmpty_.wait(lock, [this]
                            {
                                return quit_.load() || !deque_.empty();
                            });
        if (quit_.load())
        {
            return false;
        }

        val = std::move(deque_.front());
        deque_.pop_front();

        return true;
    }

    bool PopTimeout(T &val, std::chrono::milliseconds wait_duration)
    {
        std::unique_lock lock{mutex_};
        if (!cond_notEmpty_.wait_for(lock, wait_duration, [this]
                                     {
                                         return quit_.load() || !deque_.empty();
                                     }))
        {
            return false;
        }
        if (quit_.load())
        {
            return false;
        }

        val = std::move(deque_.front());
        deque_.pop_front();

        return true;
    }

    bool tryPop(T &val)
    {
        std::lock_guard<std::mutex> guard{mutex_};
        if (quit_.load() || deque_.empty())
        {
            return false;
        }

        val = std::move(deque_.front());
        deque_.pop_front();

        return true;
    }

    bool PopAll(std::deque<T> &val)
    {
        std::unique_lock lock{mutex_};
        cond_notEmpty_.wait(lock, [this]
                            {
                                return quit_.load() || !deque_.empty();
                            });
        if (quit_.load())
        {
            return false;
        }

        deque_.swap(val);

        return true;
    }

    bool PopAllTimeout(std::deque<T> &val, std::chrono::milliseconds wait_duration)
    {
        std::unique_lock lock{mutex_};
        if (!cond_notEmpty_.wait_for(lock, wait_duration, [this]
                                     {
                                         return quit_.load() || !deque_.empty();
                                     }))
        {
            return false;
        }
        if (quit_.load())
        {
            return false;
        }

        deque_.swap(val);

        return true;
    }

    bool tryPopAll(std::deque<T> &val)
    {
        std::lock_guard<std::mutex> guard{mutex_};
        if (quit_.load() || deque_.empty())
        {
            return false;
        }

        deque_.swap(val);

        return true;
    }

private:
    mutable std::mutex mutex_;
    std::deque<T> deque_;
    std::condition_variable cond_notEmpty_;
    std::atomic_bool quit_ = false;

    template <typename F>
    bool PushImpl(F &&f)
    {
        std::lock_guard<std::mutex> guard{mutex_};
        if (quit_.load())
        {
            return false;
        }
        deque_.emplace_back(std::forward<F>(f));
        cond_notEmpty_.notify_one();

        return true;
    }
};

template <typename T, std::size_t capacity>
class BoundedQueue
{
    static_assert(capacity > 0, "BoundedQueue capacity must be greater than 0");

public:
    ~BoundedQueue()
    {
        Quit();
    }

    void Quit()
    {
        quit_.store(true);
        cond_notFull_.notify_all();
        cond_notEmpty_.notify_all();
    }

    void Clear()
    {
        std::lock_guard<std::mutex> guard{mutex_};
        deque_.clear();
        cond_notFull_.notify_one();
    }

    bool Empty() const
    {
        auto lock = std::lock_guard{mutex_};
        return deque_.empty();
    }

    bool Full() const
    {
        std::lock_guard<std::mutex> guard{mutex_};
        return deque_.size() >= capacity;
    }

    std::size_t Size() const noexcept
    {
        std::lock_guard<std::mutex> guard{mutex_};
        return deque_.size();
    }

    bool Push(const T &val)
    {
        return PushImpl(val);
    }

    bool Push(T &&val)
    {
        return PushImpl(std::forward<T>(val));
    }

    bool tryPush(const T &val)
    {
        return tryPushImpl(val);
    }

    bool tryPush(T &&val)
    {
        return tryPushImpl(std::forward<T>(val));
    }

    bool Pop(T &val)
    {
        std::unique_lock lock{mutex_};
        cond_notEmpty_.wait(lock, [this]
                            {
                                return quit_.load() || !deque_.empty();
                            });
        if (quit_.load())
        {
            return false;
        }

        val = std::move(deque_.front());
        deque_.pop_front();
        cond_notFull_.notify_one();

        return true;
    }

    bool PopTimeout(T &val, std::chrono::milliseconds wait_duration)
    {
        std::unique_lock lock{mutex_};
        if (!cond_notEmpty_.wait_for(lock, wait_duration, [this]
                                     {
                                         return quit_.load() || !deque_.empty();
                                     }))
        {
            return false;
        }
        if (quit_.load())
        {
            return false;
        }

        val = std::move(deque_.front());
        deque_.pop_front();
        cond_notFull_.notify_one();

        return true;
    }

    bool tryPop(T &val)
    {
        std::lock_guard<std::mutex> guard{mutex_};
        if (quit_.load() || deque_.empty())
        {
            return false;
        }

        val = std::move(deque_.front());
        deque_.pop_front();
        cond_notFull_.notify_one();

        return true;
    }

    bool PopAll(std::deque<T> &val)
    {
        std::unique_lock lock{mutex_};
        cond_notEmpty_.wait(lock, [this]
                            {
                                return quit_.load() || !deque_.empty();
                            });
        if (quit_.load())
        {
            return false;
        }

        deque_.swap(val);
        cond_notFull_.notify_one();

        return true;
    }

    bool PopAllTimeout(std::deque<T> &val, std::chrono::milliseconds wait_duration)
    {
        std::unique_lock lock{mutex_};
        if (!cond_notEmpty_.wait_for(lock, wait_duration, [this]
                                     {
                                         return quit_.load() || !deque_.empty();
                                     }))
        {
            return false;
        }
        if (quit_.load())
        {
            return false;
        }

        deque_.swap(val);
        cond_notFull_.notify_one();

        return true;
    }

    bool tryPopAll(std::deque<T> &val)
    {
        std::lock_guard<std::mutex> guard{mutex_};
        if (quit_.load() || deque_.empty())
        {
            return false;
        }

        deque_.swap(val);
        cond_notFull_.notify_one();

        return true;
    }

private:
    mutable std::mutex mutex_;
    std::deque<T> deque_;
    std::condition_variable cond_notFull_;
    std::condition_variable cond_notEmpty_;
    std::atomic_bool quit_ = false;

    template <typename F>
    bool PushImpl(F &&f)
    {
        std::unique_lock lock{mutex_};
        cond_notFull_.wait(lock, [this]
                           {
                               return quit_.load() || deque_.size() < capacity;
                           });
        if (quit_.load())
        {
            return false;
        }

        deque_.emplace_back(std::forward<F>(f));
        cond_notEmpty_.notify_one();

        return true;
    }

    template <typename F>
    bool tryPushImpl(F &&f)
    {
        std::lock_guard<std::mutex> guard{mutex_};
        if (quit_.load() || deque_.size() >= capacity)
        {
            return false;
        }

        deque_.emplace_back(std::forward<F>(f));
        cond_notEmpty_.notify_one();

        return true;
    }
};
