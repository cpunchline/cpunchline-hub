#include <iostream>
#include <type_traits>
#include <memory>
#include <string>

// 局部变量初始化 - auto大法

// auto变量
// auto变量必须初始化
// auto变量不要new, 建议用make_unique或者make_shared

using namespace std::string_literals;

struct Test
{
    int a = 1;
    std::string b = "hello"s;
};

int main()
{
#if 0
    // 字面量
    auto a = size_t(0);
    auto b = static_cast<size_t>(0);
    auto c = char(0);

    // string
    auto s1 = "hello"; // const char *
    auto s2 = "hello"s;
    auto s3 = L"hello"s; // std::string("hello", 5);

    // 结构体 or 类
    auto d = Test{1, "hello"};
    auto e = Test{
        .a = 1,
        .b = "hello"s,
    };
    auto f = std::make_unique<Test>();
    auto g = std::make_shared<Test>();
#endif

    return EXIT_SUCCESS;
}
