#pragma once

#include <iostream>
#include <array>

using namespace std;
using namespace std::string_literals;

template <typename T>
struct X
{
};

template <typename T>
struct X1
{
};

template <typename T>
struct X2
{
};

template <std::size_t N>
struct X4
{
};

// 必须显式的指明类模板的类型实参, 并且没有办法推导, 事实上这个空类在这里本身没什么意义
// 类模板能使用类模板形参, 声明自己的成员
// 类模板一样可以有默认实参
template <typename T = int>
struct ClassTest
{
    ClassTest(T v) :
        t{v}
    {
    }

private:
    T t;
};

// 在类中声明一个有默认实参的类模板类型的数据成员(静态或非静态, 是否类内定义都无所谓), 不要类内声明中省略 <>
struct Test2
{
    // ClassTest test;                // Error
    // static inline ClassTest test3; // Error
    ClassTest<> test2;    // OK
    ClassTest<int> test3; // OK
};

// 推导指引
// 如果只是设计具体类型, 那么只需要: 模板名称(类型a)->模板名称<想要让类型a被推导为的类型>
// 如果涉及的是一类类型, 那么就需要加上 template, 然后使用它的模板形参

// 如果推导为 int, 就让它实际成为 size_t
ClassTest(int) -> ClassTest<std::size_t>;

//  如果推导为 非常量类型, 就让它实际成为 常量类型
template <typename T>
ClassTest(T) -> ClassTest<const T>;

/*
我们要给出 TArray 的模板类型, 那么就让模板形参单独写一个 T 占位, 放到形参列表中,
并且写一个模板类型形参包用来处理任意个参数; 获取 TArray 的 size 也很简单,
直接使用 sizeof... 获取形参包的元素个数, 然后再 +1 , 因为先前我们用了一个模板形参占位
*/

template <class Ty, std::size_t size>
struct TArray
{
    Ty arr[size];
};

template <typename T, typename... Args>
TArray(T t, Args...) -> TArray<T, sizeof...(Args) + 1>;

// 类模板的模板类型形参可以接受一个类模板作为参数, 我们将它称为: 模板模板形参
template <template <typename T> typename C>
struct ClassTest2
{
};

// template<typename T> typename C 我们分两部分看就好
// 前面的 template<typename T> 就是我们要接受的类模板它的模板列表, 是需要一模一样的, 比如类模板 X 就是.
// 后面的 typename 是语法要求, 需要声明这个模板模板形参的名字, 可以自定义, 这样就引入了一个模板模板形参.
// template < 形参列表 > typename(C++17)|class 名字(可选)              (1)
// template < 形参列表 > typename(C++17)|class 名字(可选) = default    (2)
// template < 形参列表 > typename(C++17)|class ... 名字(可选)          (3) (C++11 起)

// 1.可以有名字的模板模板形参.
template <typename T>
struct named_array
{
    T arr[10];
};

template <typename Ty, template <typename T> typename C>
struct Named_Array
{
    C<Ty> array;
};

// 2. 有默认模板且可以有名字的模板模板形参.
template <typename Ty, template <typename T> typename C = named_array>
struct Default_Named_Array
{
    C<Ty> array;
};

// 3. 可以有名字的模板模板形参包.(其实就是形参包的一种, 能接受任意个数的类模板)
template <template <typename T> typename... Ts>
struct ClassTest3
{
};

// 模板模板形参也可以和非类型模板形参一起使用
// 注意到了吗?我们省略了其中 template<std::size_t> 非类型模板形参的名字,
// 可能通常会写成 template<std::size_t N> , 我们只是为了表达这是可以省略了, 看自己的需求
template <template <std::size_t> typename C>
struct ClassTest4
{
};

// 普通的有形参包的类模板
template <typename... T>
struct arg_array
{
    int arr[sizeof...(T)]; // 保有的数组大小根据模板类型形参的元素个数
};

template <typename Ty, template <typename... T> typename C = arg_array>
struct Arg_Array
{
    C<Ty> array;
};

// 可变参数类模板
template <typename... Args>
struct ArgTest
{
    ArgTest(Args... args) :
        value{args...} // 构造函数中使用成员初始化列表来初始化成员 value, 没什么问题, 正常展开.
    {
    } // 参数展开
    std::tuple<Args...> value; // 类型形参包展开
    // std::tuple 是一个模板类, 我们用来存储任意类型任意个数的参数,
    // 我们指明它的模板实参是使用的模板的类型形参包展开,
    // std::tuple<Args...> 展开后成为 std::tuple<int, const char*, char, double>
};

// 需要注意的是字符串字面量的类型是 const char[N] , 之所以被推导为 const char* 在于数组之间不能"拷贝"
// 它隐式转换为了指向数组首地址的指针, 类型自然也被推导为 const char*
