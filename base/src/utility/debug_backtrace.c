#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "platform.h"

#if defined(PLATFORM_OS_LINUX)

#include <signal.h>
#include <fcntl.h>
#include <linux/limits.h>
#include <ucontext.h>
#include <link.h>
#include <execinfo.h>

#include "utility/utils.h"
#include "utility/debug_backtrace.h"

#define BT_SIZE     (100)
#define CMD_SIZE    (1024)
#define SKIP_FRAMES (2) // Skip backtrace_handler and signal handler frames

char crash_file_abs_path[PATH_MAX] = {};
char abs_process_path[PATH_MAX] = {};
char print_type = 0; // 0 write to terminal; 1 write to file;

// Helper function for async-signal-safe string output to file descriptor
static void safe_write_str(int fd, const char *str)
{
    if (str)
    {
        size_t len = strlen(str);
        ssize_t r = write(fd, str, len);
        (void)r; // Ignore return value in signal context
    }
}

// Helper function for async-signal-safe int64_t output to file descriptor
static void safe_write_int64(int fd, int64_t num)
{
    char buf[32]; // 2^63 is approx 9.2e18 (19 digits) + sign + null. 32 is safe.
    char *p = buf + sizeof(buf) - 1;
    int negative = 0;

    // 特殊处理 INT64_MIN (-9223372036854775808)
    // 因为 -INT64_MIN 会溢出，所以不能直接取反
    if (num == INT64_MIN)
    {
        ssize_t r = write(fd, "-9223372036854775808", 20);
        (void)r; // Ignore return value in signal context
        return;
    }

    if (num < 0)
    {
        negative = 1;
        num = -num;
    }

    // 处理 0 的情况 (如果上面没返回，且 num 为 0)
    if (num == 0)
    {
        ssize_t r = write(fd, "0", 1);
        (void)r; // Ignore return value in signal context
        return;
    }

    // 转换为字符串 (倒序)
    do
    {
        *--p = (char)('0' + (num % 10));
        num /= 10;
    } while (num > 0);

    // 添加负号
    if (negative)
    {
        *--p = '-';
    }

    size_t len = (size_t)(buf + sizeof(buf) - 1 - p);
    ssize_t r = write(fd, p, len);
    (void)r; // Ignore return value in signal context
}

static void safe_write_uint64(int fd, uint64_t num)
{
    char buf[32]; // 2^64 is approx 1.8e19, which needs 20 digits + null terminator. 32 is safe.
    char *p = buf + sizeof(buf) - 1;

    // Handle 0 explicitly to avoid empty output from the do-while loop if logic changes
    if (num == 0)
    {
        ssize_t r = write(fd, "0", 1);
        (void)r; // Ignore return value in signal context
        return;
    }

    // Convert number to string in reverse order
    do
    {
        *--p = (char)('0' + (num % 10));
        num /= 10;
    } while (num > 0);

    size_t len = (size_t)(buf + sizeof(buf) - 1 - p);
    ssize_t r = write(fd, p, len);
    (void)r; // Ignore return value in signal context
}

static void safe_write_hex64_var(int fd, uint64_t num)
{
    if (num == 0)
    {
        ssize_t r = write(fd, "0x0", 3);
        (void)r; // Ignore return value in signal context
        return;
    }

    char buf[16]; // Max 16 hex digits for uint64_t
    char *p = buf + sizeof(buf);

    const char hex_chars[] = "0123456789abcdef";

    do
    {
        *--p = hex_chars[num & 0xF];
        num >>= 4;
    } while (num > 0);

    // Write prefix
    ssize_t r = write(fd, "0x", 2);
    (void)r; // Ignore return value in signal context

    // Write digits
    size_t len = (size_t)(buf + sizeof(buf) - p);
    r = write(fd, p, len);
    (void)r; // Ignore return value in signal context
}

// Helper function for async-signal-safe hex address output to file descriptor
static void safe_write_hex(int fd, void *addr)
{
    if (!addr)
    {
        safe_write_str(fd, "0x0");
        return;
    }

    uintptr_t num = (uintptr_t)addr;
    char buf[32];
    char *p = buf + sizeof(buf) - 1;

    const char hex_chars[] = "0123456789abcdef";

    do
    {
        *--p = hex_chars[num & 0xf];
        num >>= 4;
    } while (num > 0);

    // Write "0x" prefix
    ssize_t r = write(fd, "0x", 2);
    (void)r; // Ignore return value in signal context

    size_t len = (size_t)(buf + sizeof(buf) - 1 - p);
    r = write(fd, p, len);
    (void)r; // Ignore return value in signal context
}

static int signals_trace[] = {
    SIGILL,  /* Illegal instruction (ANSI).  */
    SIGABRT, /* Abort (ANSI).  */
    SIGBUS,  /* BUS error (4.2 BSD). (unaligned access) */
    SIGFPE,  /* Floating-point exception (ANSI).  */
    SIGSEGV, /* Segmentation violation (ANSI).  */
};

static void *get_uc_mcontext_pc(ucontext_t *uc)
{
#if defined(__APPLE__) && !defined(MAC_OS_X_VERSION_10_6)
/* OSX < 10.6 */
#if defined(__x86_64__)
    return (void *)uc->uc_mcontext->__ss.__rip;
#elif defined(__i386__)
    return (void *)uc->uc_mcontext->__ss.__eip;
#else
    return (void *)uc->uc_mcontext->__ss.__srr0;
#endif
#elif defined(__APPLE__) && defined(MAC_OS_X_VERSION_10_6)
/* OSX >= 10.6 */
#if defined(_STRUCT_X86_THREAD_STATE64) && !defined(__i386__)
    return (void *)uc->uc_mcontext->__ss.__rip;
#else
    return (void *)uc->uc_mcontext->__ss.__eip;
#endif
#elif defined(__linux__)
/* Linux */
#if defined(__i386__)
    return (void *)uc->uc_mcontext.gregs[REG_EIP]; /* Linux 32 */
#elif defined(__X86_64__) || defined(__x86_64__)
    return (void *)uc->uc_mcontext.gregs[REG_RIP]; /* Linux 64 */
#elif defined(__ia64__) /* Linux IA64 */
    return (void *)uc->uc_mcontext.sc_ip;
#elif defined(__arm__)
    return (void *)uc->uc_mcontext.arm_pc;
#elif defined(__aarch64__)
    return (void *)uc->uc_mcontext.pc;
#else
    return NULL;
#endif
#else
    return NULL;
#endif
}

#if 0
#include <stacktrace>
#include <iostream>
#include <print>

void print_stack_trace()
{
    auto st = std::stracktrace::current();
    for (const auto & entry : st)
    {
        std::println("Func: {}\t File: {}\t Line: {}\n", entry.description(), entry.source_file(), entry.source_line());
    }
}
#endif

static void backtrace_handler(int sig_num, siginfo_t *info, void *ucontext)
{
    (void)info;
    int fd = -1;
    if (print_type == 1)
    {
        fd = open(crash_file_abs_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0)
        {
            fd = STDERR_FILENO;
        }
    }
    else
    {
        fd = STDERR_FILENO;
    }

    void *array[BT_SIZE] = {};
    void *caller_addr = get_uc_mcontext_pc((ucontext_t *)ucontext);

    safe_write_str(fd, "received Signal[");
    safe_write_int64(fd, sig_num);
    safe_write_str(fd, "]");
    safe_write_str(fd, " at [");
    safe_write_hex(fd, caller_addr);
    safe_write_str(fd, "]\n");

    int size = backtrace(array, BT_SIZE);
    if (size <= 0)
    {
        safe_write_str(fd, "backtrace failed\n");
    }
    else if (size > SKIP_FRAMES)
    {
        // Skip signal handler frames
        void **p_array = array + SKIP_FRAMES;
        int new_size = size - SKIP_FRAMES;

        // Output raw addresses only - async-signal-safe
        safe_write_str(fd, "Backtrace addresses (");
        safe_write_int64(fd, new_size);
        safe_write_str(fd, " frames):\n");
        safe_write_str(fd, "#Frame Address\n");

        Dl_info dl_info = {};
        uintptr_t offset = 0;
        for (int i = 0; i < new_size; ++i)
        {
            safe_write_str(fd, "#");
            safe_write_int64(fd, i);
            safe_write_str(fd, " ");
            safe_write_hex(fd, p_array[i]);

            memset(&dl_info, 0x00, sizeof(dl_info));
            offset = 0;
            if (dladdr(array[i], &dl_info) && dl_info.dli_fbase) // not signal-safe but it is very important
            {
                offset = (uintptr_t)array[i] - (uintptr_t)dl_info.dli_fbase;
                safe_write_str(fd, "-");
                safe_write_hex64_var(fd, offset);
            }

            safe_write_str(fd, "\n");
        }

        safe_write_str(fd, "Note: To resolve addresses to source locations, run:\n");
        safe_write_str(fd, "  addr2line -C -f -e crash_point(lib/process) <address>\n");
        safe_write_str(fd, "Example:\n");
        safe_write_str(fd, "  addr2line -C -f -e ");
        safe_write_str(fd, abs_process_path);
        safe_write_str(fd, " ");
        safe_write_hex64_var(fd, offset);
        safe_write_str(fd, "\n");
    }
    else
    {
        safe_write_str(fd, "backtrace too shallow (size=");
        safe_write_int64(fd, size);
        safe_write_str(fd, ")\n");
    }

    // Close the file descriptor
    if (fd != STDERR_FILENO)
    {
        close(fd);
    }

    // Restore default signal handler
    struct sigaction sa = {};
    sa.sa_handler = SIG_DFL;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(sig_num, &sa, NULL) != 0)
    {
        return;
    }

    // Unblock the signal
    sigset_t mask = {};
    sigemptyset(&mask);
    sigaddset(&mask, sig_num);
    if (sigprocmask(SIG_UNBLOCK, &mask, NULL) != 0)
    {
        return;
    }

    raise(sig_num); // Re-raise signal for core dump
}

int debug_backtrace_init(char *crash_file_path)
{
    pid_t pid = getpid();
    char process_name[NAME_MAX] = {};
    util_get_exec_name(pid, process_name, sizeof(process_name));
    util_get_abs_exec_name(pid, abs_process_path, sizeof(abs_process_path));

    if (NULL == crash_file_path)
    {
        print_type = 0;
    }
    else
    {
        print_type = 1;
        snprintf(crash_file_abs_path, sizeof(crash_file_abs_path), "%s/%s.%d.crash", crash_file_path, process_name, pid);
    }

    struct sigaction sa = {};
    sa.sa_sigaction = backtrace_handler;
    sa.sa_flags = SA_RESTART | SA_SIGINFO;
    sigemptyset(&sa.sa_mask);

    size_t num_signals = sizeof(signals_trace) / sizeof(signals_trace[0]);
    int ret = 0;

    for (size_t i = 0; i < num_signals; ++i)
    {
        if (sigaction(signals_trace[i], &sa, NULL) != 0)
        {
            LOG_PRINT_ERROR("backtrace failed to set signal handler for signal ");
            ret = -1;
            break;
        }
    }

    return ret;
}

#else
int debug_backtrace_init(char *crash_file_path)
{
    (void)crash_file_path;
    return 0;
}
#endif
