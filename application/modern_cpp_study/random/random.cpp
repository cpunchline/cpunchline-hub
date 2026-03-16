#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include <random>
#include <thread>
#include <numeric>
#include <threads.h>

long c_get_random_num(long min, long max, int choice = 0)
{
    /*
    C语言标准库中, srand和rand并不是线程安全的
    int rand(void); // 返回一个随机整数
    rand() % x;     // 范围为(0, x - 1)
    */

    long res = 0;

    switch (choice)
    {
        case 0: // [min, max]
            res = random() % (max + 1 - min) + min;
            break;
        case 1: // (min, max)
            res = random() % (max - 1 - min) + min + 1;
            break;
        case 2: // [min, max)
            res = random() % (max - min) + min;
            break;
        case 3: // (min, max]
            res = random() % (max - min) + min + 1;
            break;
        default:
            return -1;
    }

    return res;
}

double c_get_random_double()
{
    // return rand() / (double)RAND_MAX;
    return drand48(); // 返回0 ~ 1之间double类型的随机数
}

int cpp_get_random_num(int min, int max, int choice = 0)
{
    std::random_device rnd;
    thread_local std::mt19937 rng(rnd());

    int res = 0;

    switch (choice)
    {
        case 0: // [min, max]
            {
                std::uniform_int_distribution<int> uni(min, max);
                res = uni(rng);
                break;
            }
        case 1: // (min, max)
            {
                std::uniform_int_distribution<int> uni(min + 1, max - 1);
                res = uni(rng);
            }
            break;
        case 2: // [min, max)
            {
                std::uniform_int_distribution<int> uni(min, max - 1);
                res = uni(rng);
            }
            break;
        case 3: // (min, max]
            {
                std::uniform_int_distribution<int> uni(min + 1, max);
                res = uni(rng);
                break;
            }
        default:
            return -1;
    }

    return res;
}

std::vector<int> cpp_fill_in_array_by_random(size_t size)
{
    std::vector<int> v;
    v.reserve(size);

    std::mt19937 rng{std::random_device()()};
    std::iota(v.begin(), v.end(), 0);      // 填充0~100
    std::shuffle(v.begin(), v.end(), rng); // 洗牌(唯一且随机)
    // rng更改, 则洗牌规则更改
    // shuffle调用一次, 则重新洗牌一次

#if 0
    // 等概率分布, 重复
    std::generate(v.begin(), v.end(), [rng = std::random_device()(), uni = std::uniform_int_distribution(0, 100)]() mutable
                  {
                      return uni(rng);
                  });
#endif

#if 0
    // 等概率分布, 重复
    std::generate_n(std::back_inserter(v), 100, [rng = std::random_device()(), uni = std::uniform_int_distribution(0, 100)]() mutable
                    {
                        return uni(rng);
                    });
#endif

    return v;
}

double cpp_get_random_double()
{
    // 等概率分布, 重复
    std::mt19937 rng{std::random_device()()};
    std::uniform_real_distribution<double> unf(0.0f, 1.0f);
    return unf(rng);
}

std::string cpp_get_random_string()
{
    // 等概率分布, 重复
    std::mt19937 rng{std::random_device()()};
    std::vector<std::string> choices = {"apple", "peach", "cherry"};
    std::uniform_int_distribution<size_t> uni(0, choices.size() - 1);

    return choices[uni(rng)];
}

std::string cpp_get_probability_string()
{
    // 按指定概率分布
    std::mt19937 rng{std::random_device()()};
    std::vector<float> probability = {0.5f, 0.25f, 0.25f};
    std::vector<float> probability_scanned;
    std::inclusive_scan(probability.begin(), probability.end(), std::back_inserter(probability_scanned));
    std::vector<std::string> choices = {"apple", "peach", "cherry"};
    std::uniform_real_distribution<float> unf(0.0f, 1.0f);

    auto get_string_res = [&]() -> std::string
    {
        float f = unf(rng);
        auto it = std::lower_bound(probability_scanned.begin(), probability_scanned.end(), f);
        if (it == probability_scanned.end())
        {
            return "";
        }
        return choices[(size_t)(it - probability_scanned.begin())];
    };

    return get_string_res();
}

int main()
{
    srand((unsigned)time(NULL)); // 随机数生成器(thread_local线程局部变量)

    return EXIT_SUCCESS;
}
