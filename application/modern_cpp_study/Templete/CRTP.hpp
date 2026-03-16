#pragma once

#include <iostream>

// CRTP 奇特重现模板模式
// CRTP 可用于在父类暴露接口, 而子类实现该接口, 以此实现"编译期多态", 或称"静态多态"

// 好处
// 1. 静态多态 CRTP 实现静态多态, 无需使用虚函数, 静态绑定, 无运行时开销.
// 2. 类型安全 CRTP 提供了类型安全的多态性.通过模板参数传递具体的子类类型, 编译器能够确保类型匹配, 避免了传统向下转换可能引发的类型错误.
// 3. 灵活的接口设计 CRTP 允许父类定义公共接口, 并要求子类实现具体的操作.这使得基类能够提供通用的接口, 而具体的实现细节留给派生类.其实也就是说多态了.

// 父类是模板, 再定义一个类类型继承它.
// 因为类模板不是类, 只有实例化后的类模板才是实际的类类型, 所以我们需要实例化它, 显式指明模板类型参数, 而这个类型参数就是我们定义的类型, 也就是子类了.
// 这种范式是完全合法合理的, 并无问题, 首先不要对这种东西感到害怕或觉得非常神奇, 也只不过是基本的语法规则所允许的.
// 即使不使用 CRTP, 这些写法也是完全合理的, 并无问题.

template <class Son>
class Farther
{
};

class Son : public Farther<Son>
{
};

template <class Dervied>
class Base
{
public:
    // 公开的接口函数 供外部调用
    void addWater()
    {
        // 调用子类的实现函数.
        // 要求子类必须实现名为 impl() 的函数, 这个函数会被调用来执行具体的操作
        // static_cast<Dervied*>(this) 是进行了一个类型转换, 将 this 指针(也就是父类的指针), 转换为通过模板参数传递的类型, 也就是子类的指针
        // 这个转换是安全合法的.因为 this 指针实际上指向一个 X 类型的对象, X 类型对象继承了 Base<X> 的部分, X 对象也就包含了 Base<X> 的部分, 所以这个转换在编译期是有效的, 并且是合法的.
        static_cast<Dervied *>(this)->impl();
    }
};

class Dervied : public Base<Dervied>
{
public:
    // 子类实现了父类接口
    void impl() const
    {
        std::cout << "Dervied 设备加了 50 毫升水\n";
    }
};

// 不让子类的接口暴露出来
class Dervied_Friend : public Base<Dervied_Friend>
{
    // 设置友元, 让父类得以访问
    friend Base<Dervied_Friend>;

private:
    // 私有接口, 禁止外部访问
    void impl() const
    {
        std::cout << "Dervied_Friend 设备加了 50 毫升水\n";
    }
};

// 使用 CRTP 模式实现静态多态性并复用代码
// 虚函数的价值在于, 作为一个参数传入其他函数时, 可以复用那个函数里的代码, 而不需要在需求频繁变动与增加的时候一直修改

template <typename Derived>
class TBase
{
public:
    void addWater(int amount)
    {
        static_cast<Derived *>(this)->impl_addWater(amount);
    }

#if 0
    // C++23 的改动-显式对象形参
    // 就是将 C++23 之前, 隐式的, 由编译器自动将 this 指针传递给成员函数使用的, 改成允许用户显式写明了;
    // 不再需要使用 static_cast 进行转换, 直接调用即可
    // auto&& 被推导为 Derived&, 顾名思义"显式"对象形参, 非常的简单直观
    void addWater(this auto &&self, int amount)
    {
        self.impl_addWater(amount);
    }
#endif
};

class TXFriend : public TBase<TXFriend>
{
    friend TBase<TXFriend>;
    void impl_addWater(int amount)
    {
        std::cout << "TXFriend 设备加了 " << amount << " 毫升水\n";
    }
};

class TYFriend : public TBase<TYFriend>
{
    friend TBase<TYFriend>;
    void impl_addWater(int amount)
    {
        std::cout << "TYFriend 设备加了 " << amount << " 毫升水\n";
    }
};

template <typename T>
void processWaterAddition(TBase<T> &r, int amount)
{
    if (amount > 0)
    {
        r.addWater(amount);
    }
    else
    {
        std::cerr << "无效数量: " << amount << '\n';
    }
}
