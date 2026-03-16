#include <inttypes.h>
#include "utility/utils.h"
#include <linux/limits.h>

static void print_bits_32(uint32_t val, int high, int low)
{
    if (high < low || high >= 32)
    {
        printf("Invalid range for 32-bit value.\n");
        return;
    }

    for (int i = high; i >= low; i--)
    {
        printf("%d", !!(val & (1U << (i - low))));
        if ((i - low) % 4 == 0 && i != low)
        {
            printf(" ");
        }
    }
    printf("\n");
}

static void print_bits_64(uint64_t val, int high, int low)
{
    if (high < low || high >= 64)
    {
        printf("Invalid range for 64-bit value.\n");
        return;
    }

    for (int i = high; i >= low; i--)
    {
        printf("%d", !!(val & (1ULL << (i - low))));
        if ((i - low) % 4 == 0 && i != low)
        {
            printf(" ");
        }
    }
    printf("\n");
}

#define AAABBB    "hello world"
#define TEST_STR1 AAA
#define TEST_STR2 BBB

static void macro_test(uint32_t input_array[])
{
    printf("=== macro tests ===\n");

    // MACRO_BOOL_VALUE
    long a = 1;
    long b = 2;
    printf("a[%ld], b[%ld]\n", a, b);
    printf("MACRO_BOOL_VALUE[%" UTIL_PRI_BOOL "]\n", UTIL_MACRO_BOOL_VALUE(((a > b) ? true : false)));

    // UTIL_TOSTR
    printf("UTIL_TOSTR[%s]\n", UTIL_TOSTR(TEST_STR1));

    // UTIL_CONN
    printf("UTIL_CONN[%s]\n", UTIL_CONN(TEST_STR1, TEST_STR2));

    // UTIL_ARRAY_SIZE
    uint32_t tmp_array[] = {1, 2, 3};
    (void)input_array;
    // When an array name is used as a function parameter, it degenerates into a pointer
    // printf("input_array size=[%zd]\n", UTIL_ARRAY_SIZE(input_array));
    printf("UTIL_ARRAY_SIZE[%zd]\n", UTIL_ARRAY_SIZE(tmp_array));

    // UTIL_MIN/UTIL_MAX/UTIL_SWAP
    printf("UTIL_MIN[%ld]-UTIL_MAX[%ld]\n", UTIL_MIN(a, b), UTIL_MAX(a, b));
    UTIL_SWAP(a, b);
    printf("UTIL_SWAP[%ld, %ld]\n", a, b);
}

static void bit_operation_test(void)
{
    uint32_t value = 0x12345678;
    uint64_t value64 = 0x123456789ABCDEF0ULL;

    printf("=== Bit Operation Tests ===\n");

    // UTIL_BITGET 测试
    printf("BITGET_32(0x%08" PRIx32 ", 4) = %d\n", value, !!UTIL_BITGET_32(value, 4));
    printf("BITGET_64(0x%016" PRIx64 ", 60) = %d\n", value64, !!UTIL_BITGET_64(value64, 60));

    // UTIL_BITSET 测试
    uint32_t set_val = value;
    UTIL_BITSET_32(&set_val, 0);
    printf("BITSET_32(0x%08" PRIx32 ", 0) = 0x%08" PRIx32 "\n", value, set_val);

    uint64_t set_val64 = value64;
    UTIL_BITSET_64(&set_val64, 0);
    printf("BITSET_64(0x%016" PRIx64 ", 0) = 0x%016" PRIx64 "\n", value64, set_val64);

    // UTIL_BITCLR 测试
    uint32_t clr_val = value;
    UTIL_BITCLR_32(&clr_val, 4);
    printf("BITCLR_32(0x%08" PRIx32 ", 4) = 0x%08" PRIx32 "\n", value, clr_val);

    uint64_t clr_val64 = value64;
    UTIL_BITCLR_64(&clr_val64, 60);
    printf("BITCLR_64(0x%016" PRIx64 ", 60) = 0x%016" PRIx64 "\n", value64, clr_val64);

    // UTIL_BITFLIP 测试
    uint32_t flip_val = value;
    UTIL_BITFLIP_32(&flip_val, 4);
    printf("BITFLIP_32(0x%08" PRIx32 ", 4) = 0x%08" PRIx32 "\n", value, flip_val);

    uint64_t flip_val64 = value64;
    UTIL_BITFLIP_64(&flip_val64, 60);
    printf("BITFLIP_64(0x%016" PRIx64 ", 60) = 0x%016" PRIx64 "\n", value64, flip_val64);

    // UTIL_BITTEST 测试
    printf("BITTEST_32(0x%08" PRIx32 ", 4) = %d\n", value, UTIL_BITTEST_32(value, 4));
    printf("BITTEST_64(0x%016" PRIx64 ", 60) = %d\n", value64, UTIL_BITTEST_64(value64, 60));

    // 提取 [7:4] → high=7, low=4
    uint32_t field = UTIL_BITGETS_M_TO_N_32(value, 7, 4);

    printf("UTIL_BITGETS_M_TO_N_32(0x%08" PRIx32 ", 7, 4) = 0x%x (%u)\n", value, field, field);
    print_bits_32(field, 7, 4);

    // [4:7] → low=7, high=4
    value = 0x12345678;
    field = UTIL_BITGETS_N_TO_M_32(value, 7, 4);

    printf("UTIL_BITGETS_N_TO_M_32(0x%08" PRIx32 ", 7, 4) = 0x%x (%u)\n", value, field, field);
    print_bits_32(field, 7, 4);
}

static void bit_reverse_test(void)
{
    uint32_t val32 = 0x12345678;
    uint64_t val64 = 0x123456789ABCDEF0ULL;

    printf("=== Bit Reverse Tests ===\n");
    printf("bitreverse_n_32(0x%08" PRIx32 ", 8) = 0x%08" PRIx32 "\n", val32, util_bitreverse_n_32(val32, 8));
    printf("bitreverse_n_64(0x%016" PRIx64 ", 8) = 0x%016" PRIx64 "\n", val64, util_bitreverse_n_64(val64, 8));
}

static void power_test(void)
{
    unsigned long nums[] = {1, 2, 3, 7, 8, 15, 16, 17, 1023, 1024, 1025};
    size_t count = sizeof(nums) / sizeof(nums[0]);

    printf("=== Power of Two Tests ===\n");
    for (size_t i = 0; i < count; i++)
    {
        printf("floor2e(%lu) = %lu, ceil2e(%lu) = %lu\n",
               nums[i], util_floor2e(nums[i]), nums[i], util_ceil2e(nums[i]));
    }
}

static void time_test(void)
{
    printf("=== Time Tests ===\n");
    printf("clock_now: %" PRIu64 "\n", util_clock_now());
    printf("clock_mono: %" PRIu64 "\n", util_clock_mono());

    struct timespec now = util_time_now();
    struct timespec mono = util_time_mono();
    struct timespec boot = util_time_boot();

    printf("time_now: %ld.%09ld\n", now.tv_sec, now.tv_nsec);
    printf("time_mono: %ld.%09ld\n", mono.tv_sec, mono.tv_nsec);
    printf("time_boot: %ld.%09ld\n", boot.tv_sec, boot.tv_nsec);

    struct timespec after = util_time_after(1000);
    struct timespec mono_after = util_time_mono_after(1000);
    struct timespec boot_after = util_time_boot_after(1000);

    printf("time_after(1000ms): %ld.%09ld\n", after.tv_sec, after.tv_nsec);
    printf("time_mono_after(1000ms): %ld.%09ld\n", mono_after.tv_sec, mono_after.tv_nsec);
    printf("time_boot_after(1000ms): %ld.%09ld\n", boot_after.tv_sec, boot_after.tv_nsec);

    long int tz;
    if (util_timezone(&tz))
    {
        printf("timezone: %ld\n", tz);
    }
    else
    {
        printf("timezone: failed to get\n");
    }
}

static void random_test(void)
{
    char random_string[16 + 1] = {};
    printf("=== Random Tests ===\n");
    printf("random uint32_t[%" PRIu32 "]\n", util_random());
    printf("random uint32_t[%" PRIu32 "]\n", util_random_v2());
    printf("random uint32_t[%" PRIu32 "]\n", util_random_v3());
    printf("random uint32_t[%" PRIu32 "]\n", util_random_v4());

    // 增强的随机数范围测试
    printf("random_range_number(0, 10): %" PRIu32 "\n", util_random_range_number(0, 10));
    printf("random_range_number(100, 200): %" PRIu32 "\n", util_random_range_number(100, 200));
    printf("random_range_number(0, 0): %" PRIu32 "\n", util_random_range_number(0, 0));
    printf("random_range_number(10, 5): %" PRIu32 "\n", util_random_range_number(10, 5)); // min > max case
    printf("random_range_number(UINT32_MAX-10, UINT32_MAX): %" PRIu32 "\n",
           util_random_range_number(UINT32_MAX - 10, UINT32_MAX));

    printf("random string[%s]\n", util_random_string(random_string, sizeof(random_string)));
}

static void file_test(void)
{
    char random_string[16 + 1] = {};
    char filename[PATH_MAX] = "/tmp/test_file";
    uint8_t read_data[16 + 1] = {};
    size_t read_datalen = sizeof(read_data);
    util_random_string(random_string, sizeof(random_string));
    printf("write file data[%s] ret[%d]\n", random_string, util_file_write(filename, (uint8_t *)random_string, strlen(random_string)));
    util_file_read(filename, read_data, &read_datalen);
    printf("read file data[%s] len[%zu]\n", read_data, read_datalen);
}

int main(void)
{
    uint32_t tmp_array[] = {1, 2, 3};

    LOG_PRINT_INFO("");
    LOG_PRINT_INFO("\n");
    LOG_PRINT_INFO("\r\n");
    LOG_PRINT_INFO("\n\n");
    LOG_PRINT_INFO("\r\n\n\n");
    LOG_PRINT_INFO("1234\r\n\n");
    LOG_PRINT_INFO("1\n2\n3\n4\r\n\n");

    macro_test(tmp_array);
    bit_operation_test();
    bit_reverse_test();
    power_test();
    time_test();
    random_test();
    file_test();

    return 0;
}