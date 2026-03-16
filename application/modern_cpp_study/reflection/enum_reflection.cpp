#include <iostream>
#include <string>

enum class DataType
{
    USB,
    PCI,
    HD
};

enum DType
{
    USB,
    PCI,
    HD
};

template <auto T>
constexpr auto TypeInfo()
{
    // it is runtime impl
    // if you want a real enum reflection, please use the thirdparty 'Magic Enum';
    std::string_view type{__PRETTY_FUNCTION__};
    constexpr std::string_view begin_str{"T = "};
    constexpr std::string_view end_str{"]"};
    auto begin = type.find(begin_str) + begin_str.size();
    auto end = type.find_last_of(end_str);
    return std::string_view{type.data() + begin, end - begin};
}

int main()
{
    std::cout << TypeInfo<DataType::HD>() << std::endl;
    std::cout << TypeInfo<DType::HD>() << std::endl;
}
