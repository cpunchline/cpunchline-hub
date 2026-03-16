#pragma once

#include <cstdio>

// 模板全特化

// 给出这样一个函数模板 f, 你可以看到, 它的逻辑是返回两个对象相加的结果,
// 那么如果我有一个需求: "如果我传的是一个 double 一个 int 类型, 那么就让它们返回相减的结果"
template <typename T1, typename T2>
auto Full1(const T1 &a, const T2 &b) // C++14 允许函数返回声明的 auto 占位符自行推导类型.
{
    return a + b;
}

// 函数模板全特化
// 当特化函数模板时, 如果模板实参推导能通过函数实参提供, 那么就可以忽略它的模板实参 auto f(const double &a, const int &b)
template <>
auto Full1<double, int>(const double &a, const int &b)
{
    return a - b;
}

// 类模板全特化

// 使用全特化, 实现了一个 is_void 判断模板类型实参是不是 void 类型
// 虽然很简单, 但我们还是稍微强调一下: 同一个类模板实例化的不同的类, 彼此之间毫无关系,
// 而静态数据成员是属于类的, 而不是模板类; 模板类实例化的不同的类, 他们的静态数据成员不是同一个, 请注意.

template <typename T> // 主模板
struct my_is_void
{
    static constexpr bool value = false;
};

template <> // 对 T = void 的显式特化
struct my_is_void<void>
{
    static constexpr bool value = true;
};

template <typename T>
constexpr bool my_is_void_v = my_is_void<T>::value;

template <typename T>
struct Full2
{
    void full2() const
    {
        puts("f");
    }
};

template <>
struct Full2<int>
{
    void full2() const
    {
        puts("X<int>");
    }
    void full22() const
    {
    }

    int n;
};

// 我们要明白, 写一个类的全特化, 就相当于写一个新的类一样, 你可以自己定义任何东西, 不管是函数,数据成员,静态数据成员, 等等;
// 根据自己的需求.

// 变量模板全特化

template <typename T>
constexpr const char *s = "??";

template <>
constexpr const char *s<void> = "void";

template <>
constexpr const char *s<int> = "int";

template <typename T>
constexpr bool my_var_is_void_v = false;

template <>
constexpr bool my_var_is_void_v<void> = true;

// 上面的变量模板, 模板是类型形参, 我们根据类型进行全特化.

// 细节
// 1. 特化必须在导致隐式实例化的首次使用之前, 在每个发生这种使用的翻译单元中声明
template <typename T> // 主模板
void Full3(const T &)
{
}

void Full3_test()
{
    Full3(1); // 使用模板 Full3() 隐式实例化 Full3<int>
}

/*
template <> // 错误 Full3<int> 的显式特化在隐式实例化之后出现
void Full3<int>(const int &)
{
}
*/

// 2. 只有声明没有定义的模板特化可以像其他不完整类型一样使用(例如可以使用到它的指针和引用)
template <class T> // 主模板
class Full4;

template <> // 特化(声明, 不定义)
class Full4<int>;

// 3. 函数模板和变量模板的显式特化是否为 inline/constexpr/constinit/consteval 只与显式特化自身有关,
// 主模板的声明是否带有对应说明符对它没有影响.模板声明中出现的属性在它的显式特化中也没有效果:
template <typename T>
int Full5(T) // 主模板
{
    return 6;
}

template <>
constexpr int Full5<int>(int)
{
    return 6;
} // OK, Full5<int> 是以 constexpr 修饰的

template <class T>
constexpr T Full6(T) // 主模板
{
    return 6;
} // 这里声明的 constexpr 修饰函数模板是无效的

template <>
int Full6<int>(int)
{
    return 6;
} // OK, Full6<int> 不是以 constexpr 修饰的
// 如果主模板有 constexpr 属性, 那么模板实例化的, 如 Full6<double> 自然也是附带了 constexpr, 但是如果其特化没有, 那么以特化为准(如 Full6<int>)

// 特化的成员
template <typename T> // 主模板
struct A
{
    // 成员类
    struct B
    {
    };

    // 成员类模板
    template <class U>
    struct C
    {
    };
};

// 特化 模板类 A<void>
template <>
struct A<void>
{
    void Afunc(); // 类内声明
};

void A<void>::Afunc()
{ // 类外定义
  // todo..
}

// 特化 成员类.设置 A<char> 的情况下 B 类的定义.
template <>
struct A<char>::B
{                 // 特化 A<char>::B
    void Bfunc(); // 类内声明
};

void A<char>::B::Bfunc()
{ // 类外定义
  // todo..
}

// 特化 成员类模板.设置 A<int> 情况下模板类 C 的定义.
template <>
template <class U>
struct A<int>::C
{
    void Cfunc(); // 类内声明
};

// template<> 会用于定义被特化为类模板的显式特化的成员类模板的成员
template <>
template <class U>
void A<int>::C<U>::Cfunc()
{ // 类外定义
  // todo..
}

// 特化 类的成员函数模板
// 其实语法和普通特化函数模板没什么区别, 类外的话那就指明函数模板是在 D 类中
// 对于非类型模板参数的完全特化, 必须在类的外部进行声明和定义
struct D
{
    template <typename T>
    void Dfunc(T)
    {
    }

#if 0
    // 类内对成员函数 Dfunc 的特化, 在 gcc 无法通过编译, 根据考察, 这是一个很多年前就有的 BUG, 使用 gcc 的开发者自行注意.
    template <> // 类内特化
    void Dfunc<int>(int)
    {
        std::puts("int");
    }
#endif
};

template <>
void D::Dfunc<int>(int)
{
    std::puts("int");
}

template <>
void D::Dfunc<double>(double)
{
    std::puts("double");
}

// 特化 类模板的成员函数模板

// 成员或成员模板可以在多个外围类模板内嵌套.在这种成员的显式特化中, 对每个显式特化的外围类模板都有一个 template<>.
// 其实就是注意有几层那就多套几个 template<>, 并且指明模板类的模板实参.
// 下面这样: 就是自定义了 E<void> 且 Efunc<double> 的情况下的函数.
template <typename T>
struct E
{
    template <typename T2>
    void Efunc(T2)
    {
    }

#if 0
    // 类内对成员函数 Efunc 的特化, 在 gcc 无法通过编译, 根据考察, 这是一个很多年前就有的 BUG, 使用 gcc 的开发者自行注意.
    template <>
    void Efunc<int>(int)
    {
        // 类内特化, 对于 Efunc<int>(int) 的情况
        std::puts("Efunc<int>(int)");
    }
#endif
};

#if 0 // 编译不过
template <typename T>
template <>
void E<T>::Efunc<int>(int)
{
    // 类外特化, 对于 Efunc<int> 的情况
    std::puts("Efunc<int>(int)");
}
#endif

template <>
template <>
void E<void>::Efunc<double>(double)
{ // 类外特化, 对于 E<void>::Efunc<double> 的情况
    std::puts("E<void>::Efunc<double>");
}
