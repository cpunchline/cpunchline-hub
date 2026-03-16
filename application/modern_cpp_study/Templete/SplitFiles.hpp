#pragma once

// 模板显式实例化解决模板分文件问题
// 模板必须实例化才能使用, 实例化就会生成实际代码;
// 有隐式实例化和显式实例化, 我们平时粗略的说的"模板只有使用了才会生成实际代码", 其实是指使用模板的时候, 就会隐式实例化, 生成实际代码.
// 分文件自然没有隐式实例化了, 那我们就得显式实例化, 让模板生成我们想要的代码.

// 函数模板显式实例化
/*
template 返回类型 名字 < 实参列表 > ( 形参列表 ) ;          (1)
template 返回类型 名字 ( 形参列表 ) ;                      (2)
extern template 返回类型 名字 < 实参列表 > ( 形参列表 ) ;   (3) (C++11 起)
extern template 返回类型 名字 ( 形参列表 ) ;               (4) (C++11 起)

1. 显式实例化定义(显式指定所有无默认值模板形参时不会推导模板实参)
2. 显式实例化定义, 对所有形参进行模板实参推导
3. 显式实例化声明(显式指定所有无默认值模板形参时不会推导模板实参)
4. 显式实例化声明, 对所有形参进行模板实参推导
在模板分文件问题中, 几乎不会使用到显式实例化声明.
因为我们引用 .h 文件本身就有声明, 除非你准备直接两个 .cpp.

显式实例化定义强制实例化它所指代的函数或成员函数.它可以出现在程序中模板定义后的任何位置, 而对于给定的实参列表, 它在整个程序中只能出现一次, 不要求诊断.
显式实例化声明(extern 模板)阻止隐式实例化: 本来会导致隐式实例化的代码必须改为使用已在程序的别处所提供的显式实例化.(C++11 起)

在函数模板特化或成员函数模板特化的显式实例化中, 尾部的各模板实参在能从函数参数推导时不需要指定.


// 类模板显式实例化

template 类关键词 模板名 < 实参列表 > ;           (1)
extern template 类关键词 模板名 < 实参列表 > ;    (2)    (C++11 起)
类关键词 - class, struct 或 union
1. 显式实例化定义
2. 显式实例化声明


使用示例

// test_function_template.h
#pragma once

#include <iostream>
#include <typeinfo>

template<typename T>
void f_t(T);

// test_function_template.cpp
#include"test_function_template.h"

template<typename T>
void f_t(T) { std::cout << typeid(T).name() << '\n'; }

template void f_t<int>(int); // 显式实例化定义 实例化 f_t<int>(int)
template void f_t<>(char);   // 显式实例化定义 实例化 f_t<char>(char), 推导出模板实参
template void f_t(double);   // 显式实例化定义 实例化 f_t<double>(double), 推导出模板实参

// test_class_template.h
#pragma once

#include <iostream>
#include <typeinfo>

namespace N {

    template<typename T>
    struct X {
        int a{};
        void f();
        void f2();
    };

    template<typename T>
    struct X2 {
        int a{};
        void f();
        void f2();
    };
};

// test_class_template.cpp
#include "test_class_template.h"

template<typename T>
void N::X<T>::f(){
    std::cout << "f: " << typeid(T).name() << "a: " << this->a << '\n';
}

template<typename T>
void N::X<T>::f2() {
    std::cout << "f2: " << typeid(T).name() << "a: " << this->a << '\n';
}

template void N::X<int>::f();    // 显式实例化定义 成员函数, 这不是显式实例化类模板

template<typename T>
void N::X2<T>::f() {
    std::cout << "X2 f: " << typeid(T).name() << "a: " << this->a << '\n';
}

template<typename T>
void N::X2<T>::f2() {
    std::cout << "X2 f2: " << typeid(T).name() << "a: " << this->a << '\n';
}

template struct N::X2<int>;      // 类模板显式实例化定义

// main.cpp
#include "test_function_template.h"
#include "test_class_template.h"

int main()
{
    f_t(1);
    f_t(1.2);
    f_t('c');
    //f_t("1");   // 没有显式实例化 f_t<const char*> 版本, 会有链接错误

    N::X<int>x;
    x.f();
    //x.f2();     // 链接错误, 没有显式实例化 X<int>::f2() 成员函数
    N::X<double>x2{};
    //x2.f();     // 链接错误, 没有显式实例化 X<double>::f() 成员函数

    N::X2<int>x3; // 我们显式实例化了类模板 X2<int> 也就自然而然实例化它所有的成员, f, f2 函数
    x3.f();
    x3.f2();

    // 类模板分文件 我们写了两个类模板 X X2, 他们一个使用了成员函数显式实例化, 一个类模板显式实例化, 进行对比
    // 这主要在于我们所谓的类模板分文件, 其实类模板定义还是在头文件中, 只不过成员函数定义在 cpp 罢了.
}

值得一提的是, 我们前面讲类模板的时候说了类模板的成员函数不是函数模板, 但是这个语法形式很像前面的"函数模板显式实例化"对不对?的确看起来差不多, 不过这是显式实例化类模板成员函数, 而不是函数模板.

上面的 f f2 是定义, 但是别把它当成函数模板了, 那个 template<typename T> 是属于类模板的.

类型链接的时候都不存, 只需要保证当前文件有类的完整定义, 就能使用模板类.

类的完整定义不包括成员函数定义, 理论上只要数据成员定义都有就行了.

所以我们只需要显式实例化这个成员函数也能完成类模板分文件, 如果有其他成员函数, 那么我们就得都显式实例化它们才能使用, 或者使用显式实例化类模板, 它会实例化自己的所有成员.


// 显式实例化解决模板导出静态库动态库
生成动态库和静态库用的代码几乎是一模一样的, 只是去掉了 __declspec(dllexport).
测试动态库和静态库使用的主要区别在于项目配置, 代码上的区别是去掉 __declspec(dllexport).

// 这很简单, 和之前分文件写法的区别只是用了__declspec(dllexport) .

// export_template.h
#pragma once

#include <iostream>
#include <string>

template<typename T>
void f(T);

template<typename T>
struct __declspec(dllexport) X {
    void f();
};

// export_template.cpp
#include "export_template.h"

template<typename T>
void f(T) {                                 // 函数模板定义
    std::cout << typeid(T).name() << '\n';
}

template <typename T>
void X<T>::f(){                             // 类模板中的成员函数模板定义
    std::cout << typeid(T).name() << '\n';
}

template __declspec(dllexport)  void f<int>(int);
template __declspec(dllexport)  void f<std::string>(std::string);

template struct X<int>;     // 类模板显式实例化


可以使用 __declspec(dllexport) 关键字从 DLL 中导出数据,函数,类或类成员函数.

以上示例中的函数模板,类模板显式实例化, 不要放在 .h 文件中, 因为 一个显式实例化定义在程序中最多只能出现一次; 如果放在 .h 文件中, 被多个翻译单元使用, 就会产生问题.
当显式实例化函数模板,变量模板 (C++14 起),类模板的成员函数或静态数据成员, 或成员函数模板时, 只需要它的声明可见.
类模板,类模板的成员类或成员类模板在显式实例化之前必须出现完整定义, 除非之前已经出现了拥有相同模板实参的显式特化
我们将生成的 .dll 与 .lib 文件放在了指定目录下, 配置了项目的查找路径以供使用.

// run_test.cpp.cpp
#include "export_template.h"
#include <string>

int main(){
    std::string s;
    f(1);
    //f(1.2); // Error!链接错误, 没有这个符号
    f(s);
    X<int>x;
    x.f();
}

*/
