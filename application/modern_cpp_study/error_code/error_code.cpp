#include <iostream>

// 错误处理[错误值 - 错误描述]

// C
// errno - strerror(errno)
#include <cstring>
#include <cerrno>

// C++
#include <optional>
#include <variant>
#include <system_error>

// int : [INT_MIN, INT_MAX]
// 1. errno - std::errc::xxx
// 2. std::optional<int> : 正常范围 [INT_MIN, INT_MAX] | 单个可选/缺省值/无 {nullopt}
// 3. std::errcode重写 + 传出参数的形式
// 4. std::errcode重写 + std::variant<int, error_code> : 正常范围 [INT_MIN, INT_MAX] | 多个可选/缺省值 {error_code}
// 5. C++23 std::expected<int, error_code> : 正常范围 [INT_MIN, INT_MAX] | 多个可选/缺省值 {error_code}

enum class logic_errc
{
    success = 0,
    param_fail,
    logic_fail,
};

// 标准库参数可直接使用make_error_code
// 自定义参数则需要重载实现后方可使用, 实现如下:
/*
class error_code
{
    int _M_value;                 // 错误码
    const error_category* _M_cat; // 错误类型(其包含错误概述 和 错误码描述)
};
*/
std::error_code make_error_code(logic_errc ec)
{
    class logic_error_category : public std::error_category
    {
        virtual std::string message(int ec) const override
        {
            switch ((logic_errc)ec)
            {
                case logic_errc::success:
                    return "success";
                case logic_errc::param_fail:
                    return "param_fail";
                case logic_errc::logic_fail:
                    return "logic_fail";
                default:
                    return "unknown!";
            }
        }

        virtual const char *name() const noexcept override
        {
            return "logic_errc";
        }
    };

    static const logic_error_category instance;

    return {(int)ec, instance};
}

std::optional<int> sqrt1(int x)
{
    if (x < 0)
    {
#if 0
        errno = EDOM; // std::errc::argument_out_of_domain;
        std::cout << strerror(errno) << std::endl;
        return -errno;
#endif
        return std::nullopt;
    }

    for (int i = 0;; ++i)
    {
        if (i * i >= x)
        {
            return i;
        }
    }
}

int sqrt2(int x, std::error_code &ec)
{
    if (x < 0)
    {
        ec = make_error_code(std::errc::argument_out_of_domain);
        return -1;
    }

    if (x == 1)
    {
        ec = make_error_code(logic_errc::param_fail);
        return -1;
    }

    for (int i = 0;; ++i)
    {
        if (i * i >= x)
        {
            return i;
        }
    }
}

std::variant<int, std::error_code> sqrt3(int x)
{
    if (x < 0)
    {
        return make_error_code(std::errc::argument_out_of_domain);
    }

    if (x == 1)
    {
        return make_error_code(logic_errc::param_fail);
    }

    for (int i = 0;; ++i)
    {
        if (i * i >= x)
        {
            return i;
        }
    }
}

int test_err_code()
{
#if 0
    auto opt = sqrt1(-4);
    if (!opt) // !opt.has_value()
    {
        return -1;
    }

    // 确保在opt.has_value()分支内使用 *ret
    std::cout << opt.value() << std::endl;
#endif

#if 0
    std::error_code ec;
    int ret = sqrt2(-4, ec);
    if (!ec) // 0 != ec.value()
    {
        return ret;
    }

    std::cout << ec.value() << ec.message() << std::endl;
#endif

    auto ret = sqrt3(-4);
    if (0 == ret.index())
    {
        std::cout << std::get<0>(ret) << std::endl;
    }
    else
    {
        std::cout << std::get<1>(ret) << std::endl;
    }

    return 0;
}

int main()
{
    return test_err_code();
}
