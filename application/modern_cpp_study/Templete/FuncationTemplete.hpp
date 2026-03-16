#pragma once

#include <iostream>

using namespace std;
using namespace std::string_literals;

// 函数模板

// 函数模板自身并不是类型,函数或任何其他实体.不会从只包含模板定义的源文件生成任何代码.

// 模板只有实例化才会有代码出现(模板不能分文件)
// 你单纯的放在一个 .cpp 文件中, 它不会生成任何实际的代码, 自然也没有函数定义, 也谈不上链接器找符号了.
// 所以模板通常是直接放在 .h 文件中, 而不会分文件.或者说用 .hpp 这种后缀, 这种约定俗成的, 代表这个文件里放的是模板.

// 同一个函数模板生成的不同类型的函数, 彼此之间没有任何关系

// 实例化, 指代的是编译器确定各模板实参(可以是根据传入的参数推导, 又或者是自己显式指明模板的实参)后从模板生成实际的代码(如从函数模板生成函数, 类模板生成类等), 这是在编译期就完成的, 没有运行时开销.
// 实例化还分为隐式实例化和显式实例化

// 普通的函数模板
template <typename T> // typename 也可用 class
T GetMax(T a, T b)
{
    return a > b ? a : b;
}

// T 可能只是类型的"一部分", 若 T 为int, 则模板形参类型为const int &
// T 可以有有默认实参
// N 非类型模板参数
// 函数模板和非模板函数支持重载; 通常优先选择非模板的函数;
template <typename T = int, std::size_t N = 100>
T GetMax2(const T &a, const T &b)
{
    return a > b ? a : b;
}

// 当使用函数模板(如 GetMax())时, 模板参数可以由传入的参数推导, 但有不少情况是没有办法进行推导的
// GetMax(a, b);          // 编译器推导函数模板的 T 类型 为 int 函数模板 GetMax 被推导为 GetMax<int>
// GetMax(1, 1.2);        // Error 无法确定你的 T 到底是要 int 还是 double
// GetMax("luse"s, "乐"); // Error 无法确定你的 T 到底是要 std::string 还是 const char[N]

// GetMax<double>(a, b);                // 显式指定函数模板的 T 类型, 函数模板 GetMax 为 GetMax<double>
// GetMax(static_cast<double>(1), 1.2); // 显式类型转换

// 默认模板实参 RT
template <typename T1, typename T2, typename RT = decltype(true ? T1{} : T2{})>
RT GetMax3(const T1 &a, const T2 &b)
{
    return a > b ? a : b;
}
// 如果函数模板的形参是类型相同 最终返回值类型为 T1 or T2

// decltype(true ? T1{} : T2{})
// 三目表达式要求第二项和第三项之间能够隐式转换, 整个表达式的类型是二者的"公共"类型
// 则通过decltype即可推导出T1和T2的公共类型(即二者都可以转换的类型)

// C++11 后置返回类型(即指定返回类型, 推导由decltype实现)
template <typename T1, typename T2>
auto GetMax4(const T1 &a, const T2 &b) -> decltype(true ? a : b)
{
    return a > b ? a : b;
}
// 如果函数模板的形参是类型相同 最终返回值类型为 true ? a : b 表达式的类型, 即 const T&

// C++20 返回类型推导
/*
decltype(auto) GetMax5(const auto &a, const auto &b)
{
    return a > b ? a : b;
}
// 如果函数模板的形参是类型相同 最终返回值类型为 const T&
*/

// 可变参数函数模板
/*
一个函数, 可以接受任意类型的任意个数的参数调用
模板中需要 typename 后跟三个点 Args, 函数形参中需要用模板类型形参包后跟着三个点 再 args.
Args 类型形参包, 存储了传入的全部参数的类型;
args 函数形参包, 存储了传入的全部的参数;
sizeof...(args) 全部的参数

// 形参包
// 模板形参包是接受零个或更多个模板实参(非类型,类型或模板)的模板形参.函数形参包是接受零个或更多个函数实参的函数形参.至少有一个形参包的模板被称作变参模板

// 形参包展开
// 后随省略号且其中至少有一个形参包的名字的模式会被展开成零个或更多个逗号分隔的模式实例, 其中形参包的名字按顺序被替换成包中的各个元素.对齐说明符实例以空格分隔, 其他实例以逗号分

// C++17 折叠表达式 - 新的形参包展开方式

// 模式
如下:
&args... 中 &args 就是模式, 在展开的时候, 模式, 也就是省略号前面的一整个表达式, 会被不停的填入对象并添加 &, 然后逗号分隔.直至形参包的元素被消耗完
*/

void f(const char *, int, double)
{
    puts("值");
}

void f(const char **, int *, double *)
{
    puts("&");
}

template <typename... Args>
void sum(Args... args)
{                // const char * args0, int args1, double args2
    f(args...);  // 相当于 f(args0, args1, args2)
    f(&args...); // 相当于 f(&args0, &args1, &args2)
}

template <typename... Args>
void print(const Args &...args)
{
    int _[]{0, (std::cout << args << ' ', 0)...};
}
// {0, }; 花括号初始化器列表一直都允许有一个尾随的逗号
// 目的就是为了print(); 不报错; 当然也不会有人会这么用

// (std::cout << args << ' ' , 0)... 是一个包展开, 那么它的模式是: (std::cout << args << ' ' , 0)
// 这是一个逗号表达式: 起两个作用, 打印 和 初始化数组为0

template <typename... Args>
void print1(const Args &...args)
{
    // 让这个数组对象直到函数结束生存期才结束, 实在是太晚了, 我们可以创造一个临时的数组对象, 这样它的生存期也就是那一行罢了
    using Arr = int[];                              // 创建临时数组, 需要使用别名
    (void)Arr{0, (std::cout << args << ' ', 0)...}; // Arr只定义了没使用, 则用void将其变为一个弃值表达式
}

template <typename T, std::size_t N, typename... Args>
void f(const T (&array)[N], Args... index)
{
    print(array[index]...);
}

// 求一个数组的和
template <typename... Args, typename RT = std::common_type_t<Args...>>
RT sum(const Args &...args)
{
    RT _[]{args...};
    RT n{};
    for (int i = 0; i < sizeof...(args); ++i)
    {
        n += _[i];
    }
    return n;
}

// 成员函数模板
// 成员函数模板基本上和普通函数模板没多大区别, 唯一需要注意的是, 它大致有两类:

// 1. 类模板中的成员函数模板
// f 就是成员函数模板, 通常写起来和普通函数模板没多大区别, 大部分也都支持, 比如形参包
template <typename T>
struct Class_template
{
    template <typename... Args>
    void f(Args &&...args)
    {
        (void)std::initializer_list<int>{((void)args, 0)...};
    }
};

// 2. 普通类中的成员函数模板
// f 就是成员函数模板, 没什么问题.
struct Test
{
    template <typename... Args>
    void f(Args &&...args)
    {
        (void)std::initializer_list<int>{((void)args, 0)...};
    }
};
