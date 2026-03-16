#include "FuncationTemplete.hpp"     // 函数模板
#include "ClassTemplete.hpp"         // 类模板
#include "VariableTemplete.hpp"      // 变量模板
#include "FullSpecialization.hpp"    // 全特化
#include "PartialSpecialization.hpp" // 偏特化
#include "SplitFiles.hpp"            // 模板分文件
#include "CRTP.hpp"                  // CRTP 奇特重现模板模式

void test_Funcation_Templete()
{
    int a{1};
    int b{2};

    // 当使用函数模板(如 GetMax())时, 模板参数可以由传入的参数推导, 但有不少情况是没有办法进行推导的
    GetMax(a, b); // 编译器推导函数模板的 T 类型 为 int 函数模板 GetMax 被推导为 GetMax<int>
#if 0
    GetMax(1, 1.2);        // Error 无法确定你的 T 到底是要 int 还是 double
    GetMax("luse"s, "乐"); // Error 无法确定你的 T 到底是要 std::string 还是 const char[N]
#endif
    GetMax<double>(a, b);                // 显式指定函数模板的 T 类型, 函数模板 GetMax 为 GetMax<double>
    GetMax(static_cast<double>(1), 1.2); // 显式类型转换
    GetMax("luse"s, std::string("乐"));
}

void test_Class_Templete()
{
#if 0
    ClassTest<int> ct1{1};
    ClassTest<> ct2{1}; // 类模板有默认实参
    ClassTest ct3{1};   // C++17 CTAD 类模板实参推导

    int x = 1;
    ClassTest ct4(x); // 等价于 ClassTest<const int> t2(x);

    TArray arr{1, 2, 3, 4, 5};          // array<int>[6]{1, 2, 3, 4, 5};
    Named_Array<int, named_array> arr1; // arr 保有的成员是 named_array<int> 而它保有了 int arr[10]
    Default_Named_Array<int> arr2;      // arr 的类型同(1), 模板模板形参一样可以有默认值
    Arg_Array<int> arr3;

    ClassTest2<X> ct2_1;

    ClassTest3<X, X2, X, X> ct3_1; // 我们可以传递任意个数的模板实参

    ClassTest4<X4> ct4_1;

    ArgTest argtest{1, "2", '3', 4.};                // x 的类型是 X<int, const char*, char, double>
    std::cout << std::get<1>(argtest.value) << '\n'; // 2
#endif
}

void test_Variable_Templete()
{
    // v<int>;                                                           // 相当于 constexpr int v = 0;
    std::cout << std::is_same_v<decltype(v<int>), const int> << '\n'; // 会打印 1, 也就是 v<int> 的类型其实就是 const int
    // int b = v<>;                                                      // v 就是 v<int> 也就是 const int v = 0

    std::cout << v1<10> << '\n';
    std::cout << v1<> << '\n';

    for (const auto &i : variable_arg_array<1, 2, 3, 4, 5>)
    {
        std::cout << i << ' ';
    }
    // variable_arg_array 是一个数组, 我们传入的模板实参用来推导出这个数组的大小以及初始化.
    // {values...} 展开就是{1, 2, 3, 4, 5}.
    std::cout << std::is_same_v<decltype(::variable_arg_array<1, 2, 3, 4, 5>), const std::size_t[5]>; // 1
    // 在 msvc 会输出 1, 但是 gcc,clang, 却会输出 0.msvc 是正确的.
    // gcc 与 clang 不认为 variable_arg_array<1, 2, 3, 4, 5> 与 const std::size_t[5] 类型相同;
    // 它们认为 variable_arg_array<1, 2, 3, 4, 5> 与 const std::size_t[] 类型相同, 这显然是个 bug.
}

void test_FullSpecialization()
{
#if 0
    std::cout << Full1(2, 1) << '\n';   // 3
    std::cout << Full1(2.1, 1) << '\n'; // 1.1

    std::cout << std::boolalpha << my_is_void<char>::value << '\n'; // false
    std::cout << std::boolalpha << my_is_void<void>::value << '\n'; // true

    // 标准库在 C++17 引入了 is_xxx 的 _v 的版本, 就不需要再写 ::value 了.所以我们也可以这么做, 这会使用到变量模板
    std::cout << std::boolalpha << my_is_void_v<char> << '\n'; // false
    std::cout << std::boolalpha << my_is_void_v<void> << '\n'; // true

    Full2<void> f2_1;
    Full2<int> f2_2;
    f2_1.full2(); // 打印 f
    // f2_1.full2(); // Error!
    f2_2.full2(); // 打印 X<int>
    f2_2.full22();

    // 特化了变量模板 s 的模板实参为 void 与 int 的情况, 修改 s 的初始化器, 让它的值不同
    std::cout << s<void> << '\n'; // void
    std::cout << s<int> << '\n';  // int
    std::cout << s<char> << '\n'; // ??

    Full4<int> *p; // OK: 指向不完整类型的指针
    // Full4<int> x;  // Error: 不完整类型的对象

    constexpr auto n = Full5<int>(0); // OK, Full5<int> 是以 constexpr 修饰的, 可以编译期求值
    // constexpr auto n2 = Full5<double>(0); // Error! Full5<double> 不可编译期求值
    // constexpr auto n3 = Full6<int>(0);    // Error! 函数模板 Full6<int> 不可编译期求值
    constexpr auto n4 = Full6<double>(0); // OK! 函数模板 Full6<double> 可编译期求值

    D d;
    d.Dfunc(1);   // int
    d.Dfunc(1.2); // double
    d.Dfunc("");

    E<void> e;
    e.Efunc(1);   // E<void>(int)
    e.Efunc(1.2); // E<void>::Efunc<double>
    e.Efunc("");
#endif
}

void test_PartialSpecialization()
{
#if 0
    std::cout << ps<int> << '\n';           // ?
    std::cout << ps<int *> << '\n';         // pointer
    std::cout << ps<std::string *> << '\n'; // pointer
    std::cout << ps<int[]> << '\n';         // array
    std::cout << ps<double[]> << '\n';      // array
    std::cout << ps<int[1]> << '\n';        // ?

    std::cout << psx<char, double> << '\n';     // ?
    std::cout << psx<int, double> << '\n';      // T == int
    std::cout << psx<int, std::string> << '\n'; // T == int

    // 稍微提一下类外的写法, 不过其实通常不推荐写到类外, 目前还好; 很多情况涉及大量模板的时候, 类内声明写到类外非常的麻烦.
    Part1<int, int> p1;
    p1.f_T_T2(); // OK!
    Part1<void, int> p2;
    p2.f_void_T(); // OK!


    XCLASS<int, 10>::YCLASS<int, void> y;
    // y.f_X_Y(); // OK XCLASS<int,10> 和 YCLASS<int>
    XCLASS<int, 1>::YCLASS<int, void> y2;
    // y2.f(); // Error! 主模板模板实参不对
    XCLASS<int, 10>::YCLASS<void, int> y3;
    // y3.f(); // Error!成员函数模板模板实参不对
#endif
}

void test_CRTP()
{
    Dervied d;
    // 当你调用 d.addWater() 时, 实际上是 Dervied 对象调用了父类 Base<Dervied> 的成员函数.
    // 这个成员函数内部使用 static_cast<Dervied*>(this), 将 this 从 Base<Dervied>* 转换为 Dervied*, 然后调用 Dervied 中的 impl() 函数.
    // 这种转换是合法且安全的, 且 Dervied 确实实现了 impl() 函数.
    d.addWater();

    Dervied_Friend dfriend;
    dfriend.addWater();

    TXFriend tx_friend;
    TYFriend ty_friend;
    processWaterAddition(tx_friend, 50);
    processWaterAddition(ty_friend, 100);
    processWaterAddition(tx_friend, -10);
}

int main()
{
    return EXIT_SUCCESS;
}
