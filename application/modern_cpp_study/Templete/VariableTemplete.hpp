#pragma once

#include <iostream>

using namespace std;
using namespace std::string_literals;

// C++14 变量模板

// std::is_same_v 其实也是个变量模板, 在 C++17 引入.
// 这里用来比较两个类型是否相同, 如果相同返回 1, 不相同返回 0.

// 变量模板其实很常见, 在 C++17, 所有元编程库的类型特征均添加了 _v 的版本, 就是使用的变量模板,
// 比如 std::is_same_v ,std::is_pointer_v 等

// 比如 cv 限定, 比如 constexpr
// 当然也可以有初始化器, 比如 {} ,= xxx
template <typename T = int>
constexpr T v{};

template <std::size_t N = 66>
constexpr int v1 = N;

// 可变参数变量模板
template <std::size_t... values>
constexpr std::size_t variable_arg_array[]{values...};

// 类静态数据成员模板
struct limits
{
    template <typename T>
    static const T min; // 静态数据成员模板的声明
};

template <typename T>
const T limits::min = {}; // 静态数据成员模板的定义

// 当然, 如果支持 C++17 你也可以选择直接以 inline 修饰
struct limits2
{
    template <typename T>
    inline static const T min{};
};
