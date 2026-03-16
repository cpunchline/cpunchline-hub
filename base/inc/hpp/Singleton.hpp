#pragma once

#include <mutex>

// 单例模式

#define DISABLE_COPY(Class)        \
    Class(const Class &) = delete; \
    Class &operator=(const Class &) = delete;

#define SINGLETON_DECL(Class)         \
public:                               \
    static Class *instance();         \
    static void exitInstance();       \
                                      \
private:                              \
    DISABLE_COPY(Class)               \
    static Class *s_pInstance;        \
    static std::once_flag s_initFlag; \
    static std::mutex s_mutex;

#define SINGLETON_IMPL(Class)                      \
    Class *Class::s_pInstance = nullptr;           \
    std::once_flag Class::s_initFlag;              \
    std::mutex Class::s_mutex;                     \
                                                   \
    Class *Class::instance()                       \
    {                                              \
        std::call_once(s_initFlag, []() {          \
            s_pInstance = new Class;               \
        });                                        \
        return s_pInstance;                        \
    }                                              \
                                                   \
    void Class::exitInstance()                     \
    {                                              \
        std::lock_guard<std::mutex> lock(s_mutex); \
        if (s_pInstance)                           \
        {                                          \
            delete s_pInstance;                    \
            s_pInstance = nullptr;                 \
        }                                          \
    }

/*
class Test
{
    SINGLETON_DECL(Test)
protected:

    Test() // 构造函数保护或者私有化
    {
    }
    ~Test()
    {
    }
};

SINGLETON_IMPL(Test)
*/

#if 0
// TODO DCLP + std::atomic<T*> + acquire-release
class Singleton1
{
private:
    static Singleton1 *instance;
    static std::mutex mutex;
    Singleton1() = default;
    ~Singleton1() = default;

public:
    Singleton1(const Singleton1 &) = delete;
    Singleton1 &operator=(const Singleton1 &) = delete;

    static Singleton1 *getInstance()
    {
        if (nullptr == instance)
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (nullptr == instance)
            {
                instance = new Singleton1();
            }
        }
        return instance;
    }

    static void exitInstance()
    {
        std::lock_guard<std::mutex> lock(mutex);
        delete instance;
        instance = nullptr;
    }
};

// cpp define
// std::mutex Singleton1::mutex;
// Singleton1 *Singleton1::instance = nullptr;

class Singleton2
{
private:
    static Singleton2 *instance; // TODO std::shared_ptr
    static std::once_flag initInstanceFlag;
    Singleton2() = default;
    ~Singleton2() = default;

public:
    Singleton2(const Singleton2 &) = delete;
    Singleton2 &operator=(const Singleton2 &) = delete;

    static Singleton2 *getInstance()
    {
        if (nullptr == instance)
        {
            std::call_once(initInstanceFlag, []()
                           {
                               instance = new Singleton2();
                           });
        }
        return instance;
    }
};

// cpp define
// Singleton2 *Singleton2::instance = nullptr;
// std::once_flag Singleton2::initInstanceFlag;

class Singleton3
{
private:
    Singleton3() = default;
    ~Singleton3() = default;

public:
    Singleton3(const Singleton3 &) = delete;
    Singleton3 &operator=(const Singleton3 &) = delete;

    static Singleton3 &getInstance()
    {
        static Singleton3 instance;
        return instance;
    }
};
#endif
