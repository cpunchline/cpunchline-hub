#pragma once

#include <iostream>

// 模板偏特化 一类类型
// 函数模板没有偏特化, 只有类模板和变量模板可以.

// 模板偏特化这个语法让模板实参具有一些相同特征可以自定义, 而不是像全特化那样, 必须是具体的什么类型, 什么值
// 模板偏特化使我们可以对具有相同的一类特征的类模板,变量模板进行定制行为.

// 语法就是正常写主模板那样, 然后再定义这个 ps 的时候, 指明模板实参.
// 或者你也可以定义非类型的模板形参的模板, 偏特化, 都是一样的写法.
// 不过与全特化不同, 全特化不会写 template<typename T>, 它是直接 template<>, 然后指明具体的模板实参.
// 它与全特化最大的不同在于, 全特化基本必写 template<>, 而且定义的时候(如 ps)是指明具体的类型, 而不是一类类型(T*,T[]).

// 主模板
template <typename T>
const char *ps = "?";

// 偏特化, 对指针这一类类型
template <typename T>
const char *ps<T *> = "pointer";

// 偏特化, 但是只是对 T[] 这一类类型, 而不是数组类型, 因为 int[] 和 int[N] 不是一个类型
template <typename T>
const char *ps<T[]> = "array";

// 主模板
template <typename T, typename T2>
const char *psx = "?";

// 这种偏特化也是可以的, 多个模板实参的情况下, 对第一个模板实参为 int 的情况进行偏特化.
// 其他的各种形式无非都是我们提到的这两个示例的变种, 类模板也不例外.
template <typename T2>
const char *psx<int, T2> = "T == int";

// 类模板偏特化

template <typename T, typename T2> // 主模板
struct Part1
{
    void f_T_T2(); // 类内声明
};

template <typename T, typename T2>
void Part1<T, T2>::f_T_T2()
{
} // 类外定义

// 偏特化
template <typename T>
struct Part1<void, T>
{
    void f_void_T(); // 类内声明
};

template <typename T>
void Part1<void, T>::f_void_T()
{
} // 类外定义

// 偏特化类模板中的类模板, 全特化和偏特化一起使用的示例:
template <typename T, std::size_t N>
struct XCLASS
{
    template <typename T_, typename T2>
    struct YCLASS
    {
    };
};

#if 0
// 此示例无法在 gcc 通过编译, 这是编译器 BUG 需要注意.
template <>
template <typename T2>
struct XCLASS<int, 10>::YCLASS<int, T2>
{ // 对 XCLASS<int, 10> 的情况下的 YCLASS<int> 进行偏特化
    void f_X_Y() const
    {
    }
};
#endif

// 实现 std::is_same_v
// 对变量模板的偏特化, 逻辑也很简单, 如果两个模板类型参数的类型是一样的, 就匹配到下面的偏特化, 那么初始化就是 true, 不然就是 false
// 因为没有用到模板类型形参, 所以我们只是写了 class 进行占位; 这就和你声明函数的时候, 如果形参没有用到, 那么就不声明名字一样合理, 比如 void f(int).
// 声明为 inline 的是因为 内联变量 (C++17 起)可以在被多个源文件包含的头文件中定义.也就是允许多次定义.

template <class, class> // 主模板
inline constexpr bool my_is_same_v = false;

template <class Ty> // 偏特化
inline constexpr bool my_is_same_v<Ty, Ty> = true;

// 实现 std::enable_if
// 当条件为 true 时, std::enable_if 的 type 成员将被定义为第二个模板参数指定的类型;
// 当条件为 false 时, type 成员将被定义为 void.
template <bool Cond, typename T = void>
struct my_enable_if
{
    using type = T;
};

template <typename T>
struct my_enable_if<true, T>
{
    using type = T;
};

// std::enable_if_t 是 std::enable_if 的一个简写形式, 它只提供了 type 成员的类型.
// 它等价于 std::enable_if<Cond, T>::type
template <bool Cond, typename T = void>
using my_enable_if_t = typename my_enable_if<Cond, T>::type;
