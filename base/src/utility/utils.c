#ifndef _GUN_SOURCE
#define _GNU_SOURCE
#endif

#include <pthread.h>
#include <sys/random.h>
#include <sys/socket.h>
#include <time.h>
#include <sys/time.h>
#include <signal.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <linux/limits.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/poll.h>
#include <sys/eventfd.h>
#include <stdatomic.h>

#include "utility/utils.h"

#define TMP_FILE_SUFFIX            ".tmp"
#define TMP_FILE_SUFFIX_LEN        (sizeof(TMP_FILE_SUFFIX) - 1)
#define TMP_FILE_DEFAULT_FILE_MODE (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH) // 0644

// We pack the wfd and rfd into a uint64_t so that we can update the pair
// atomically and use util_atomic_cas64, to be lock free.
#define WFD(fds)          ((int)((fds) & 0xffffffffu))
#define RFD(fds)          ((int)(((fds) >> 32u) & 0xffffffffu))
#define FD_JOIN(wfd, rfd) ((uint64_t)(wfd) + ((uint64_t)(rfd) << 32u))

static int urandom_fd = -1;
static pthread_mutex_t urandom_lock = PTHREAD_MUTEX_INITIALIZER;

uint32_t util_bitreverse_n_32(uint32_t val, int n)
{
    uint32_t r = 0;
    while (n--)
    {
        r = (r << 1) | (val & 1);
        val >>= 1;
    }
    return r;
}

uint64_t util_bitreverse_n_64(uint64_t val, int n)
{
    uint64_t r = 0;
    while (n--)
    {
        r = (r << 1) | (val & 1);
        val >>= 1;
    }
    return r;
}

unsigned long util_floor2e(unsigned long num)
{
    unsigned long n = num;
    int e = 0;
    while (n >>= 1)
        ++e;
    unsigned long ret = 1;
    while (e--)
        ret <<= 1;
    return ret;
}

unsigned long util_ceil2e(unsigned long num)
{
    // 2**0 = 1
    if (num == 0 || num == 1)
        return 1;
    unsigned long n = num - 1;
    int e = 1;
    while (n >>= 1)
        ++e;
    unsigned long ret = 1;
    while (e--)
        ret <<= 1;
    return ret;
}

void util_msleep(uint64_t ms)
{
    struct timespec ts;

    ts.tv_sec = (time_t)(ms / 1000);
    ts.tv_nsec = (long)((ms % 1000) * 1000000);

    // Do this in a loop, so that interrupts don't actually wake us.
    while (ts.tv_sec || ts.tv_nsec)
    {
        if (nanosleep(&ts, &ts) == 0)
        {
            break;
        }
    }
}

void util_msleep_v2(uint64_t ms)
{
    // So probably there is no nanosleep.  We could in theory use
    // pthread condition variables, but that means doing memory
    // allocation, or forcing the use of pthreads where the
    // platform might be preferring the use of another threading
    // package. Additionally, use of pthreads means that we cannot
    // use relative times in a clock_settime safe manner. So we can
    // use poll() instead.
    struct pollfd pfd;
    uint64_t now;
    uint64_t expire;

    // Possibly we could pass NULL instead of pfd, but passing a
    // valid pointer ensures that if the system dereferences the
    // pointer it won't come back with EFAULT.
    pfd.fd = -1;
    pfd.events = 0;

    now = util_clock_mono();
    expire = now + ms;

    while (now < expire)
    {
        (void)poll(&pfd, 0, (int)(expire - now));
        now = util_clock_mono();
    }
}

uint64_t util_clock_now(void)
{
    uint64_t msec = 0;
    struct timespec now = util_time_now();
    msec = (uint64_t)now.tv_sec;
    msec *= 1000;
    msec += (uint64_t)(now.tv_nsec / 1000000);

    return (msec);
}

uint64_t util_clock_mono(void)
{
    uint64_t msec = 0;
    struct timespec now = util_time_mono();
    msec = (uint64_t)now.tv_sec;
    msec *= 1000;
    msec += (uint64_t)(now.tv_nsec / 1000000);

    return (msec);
}

uint64_t util_clock_boot(void)
{
    uint64_t msec = 0;
    struct timespec now = util_time_boot();
    msec = (uint64_t)now.tv_sec;
    msec *= 1000;
    msec += (uint64_t)(now.tv_nsec / 1000000);

    return (msec);
}

time_t util_time_s_now(void)
{
    struct timespec ret;
    if (0 != clock_gettime(CLOCK_REALTIME, &ret))
    {
        LOG_PRINT_ERROR("clock_gettime failed, errno[%d](%s)", errno, strerror(errno));
        ret.tv_sec = 0;
        ret.tv_nsec = 0;
    }
    return ret.tv_sec;
}

time_t util_time_s_mono(void)
{
    struct timespec ret;
    if (0 != clock_gettime(CLOCK_MONOTONIC, &ret))
    {
        LOG_PRINT_ERROR("clock_gettime failed, errno[%d](%s)", errno, strerror(errno));
        ret.tv_sec = 0;
        ret.tv_nsec = 0;
    }
    return ret.tv_sec;
}

time_t util_time_s_boot(void)
{
    struct timespec ret;
    if (0 != clock_gettime(CLOCK_BOOTTIME, &ret))
    {
        LOG_PRINT_ERROR("clock_gettime failed, errno[%d](%s)", errno, strerror(errno));
        ret.tv_sec = 0;
        ret.tv_nsec = 0;
    }
    return ret.tv_sec;
}

struct timespec util_time_now(void)
{
    struct timespec ret;
    if (0 != clock_gettime(CLOCK_REALTIME, &ret))
    {
        LOG_PRINT_ERROR("clock_gettime failed, errno[%d](%s)", errno, strerror(errno));
        ret.tv_sec = 0;
        ret.tv_nsec = 0;
    }
    return ret;
}

struct timespec util_time_mono(void)
{
    struct timespec ret;
    if (0 != clock_gettime(CLOCK_MONOTONIC, &ret))
    {
        LOG_PRINT_ERROR("clock_gettime failed, errno[%d](%s)", errno, strerror(errno));
        ret.tv_sec = 0;
        ret.tv_nsec = 0;
    }
    return ret;
}

struct timespec util_time_boot(void)
{
    struct timespec ret;
    if (0 != clock_gettime(CLOCK_BOOTTIME, &ret))
    {
        LOG_PRINT_ERROR("clock_gettime failed, errno[%d](%s)", errno, strerror(errno));
        ret.tv_sec = 0;
        ret.tv_nsec = 0;
    }
    return ret;
}

struct timespec util_time_after(uint32_t ms)
{
    struct timespec now = util_time_now();
    if (now.tv_sec == 0 && now.tv_nsec == 0)
    {
        return now;
    }
    now.tv_sec += ms / 1000;
    now.tv_nsec += (ms % 1000) * 1000000;
    if (now.tv_nsec >= 1000000000)
    {
        now.tv_sec += 1;
        now.tv_nsec -= 1000000000;
    }

    return now;
}

struct timespec util_time_mono_after(uint32_t ms)
{
    struct timespec now = util_time_mono();
    now.tv_sec += ms / 1000;
    now.tv_nsec += (ms % 1000) * 1000000;
    if (now.tv_nsec >= 1000000000)
    {
        now.tv_sec += 1;
        now.tv_nsec -= 1000000000;
    }

    return now;
}

struct timespec util_time_boot_after(uint32_t ms)
{
    struct timespec now = util_time_boot();
    now.tv_sec += ms / 1000;
    now.tv_nsec += (ms % 1000) * 1000000;
    if (now.tv_nsec >= 1000000000)
    {
        now.tv_sec += 1;
        now.tv_nsec -= 1000000000;
    }

    return now;
}

bool util_timezone(long int *tz)
{
    if (NULL == tz)
    {
        return false;
    }

    time_t t1 = time(NULL);
    time_t t2 = t1;
    struct tm curTime1 = {};
    struct tm curTime2 = {};
    struct tm *localTime = localtime_r(&t1, &curTime1);
    struct tm *gmTime = gmtime_r(&t2, &curTime2);
    if ((localTime == NULL) || (gmTime == NULL))
    {
        return false;
    }

    t1 = mktime(&curTime1);
    t2 = mktime(&curTime2);
    if ((t1 == -1) || (t2 == -1))
    {
        LOG_PRINT_ERROR("mktime current time fail, errno[%d](%s)", errno, strerror(errno));
        return false;
    }

    *tz = (t1 - t2) / 3600;

    return true;
}

uint32_t util_random(void)
{
    return (arc4random());
}

uint32_t util_random_v2(void)
{
    uint32_t val;
    if (0 != getentropy(&val, sizeof(val)))
    {
        LOG_PRINT_ERROR("getentropy fail, errno[%d](%s)", errno, strerror(errno));
        abort();
    }

    return (val);
}

uint32_t util_random_v3(void)
{
    uint32_t val;

    // Documentation claims that as long as we are not using
    // GRND_RANDOM and buflen < 256, this should never fail.
    // The exception here is that we could fail if for some
    // reason we got a signal while blocked at very early boot
    // (i.e. /dev/urandom was not yet seeded).
    if (getrandom(&val, sizeof(val), 0) != sizeof(val))
    {
        LOG_PRINT_ERROR("getrandom fail, errno[%d](%s)", errno, strerror(errno));
        abort();
    }
    return (val);
}

uint32_t util_random_v4(void)
{
    int fd;
    uint32_t val;

    (void)pthread_mutex_lock(&urandom_lock);
    if ((fd = urandom_fd) == -1)
    {
        if ((fd = open("/dev/urandom", O_RDONLY | O_CLOEXEC)) < 0)
        {
            (void)pthread_mutex_unlock(&urandom_lock);
            LOG_PRINT_ERROR("open fail, errno[%d](%s)", errno, strerror(errno));
            abort();
        }
        urandom_fd = fd;
    }
    (void)pthread_mutex_unlock(&urandom_lock);

    if (read(fd, &val, sizeof(val)) != sizeof(val))
    {
        LOG_PRINT_ERROR("read fail, errno[%d](%s)", errno, strerror(errno));
        abort();
    }

    return (val);
}

uint32_t util_random_range_number(uint32_t min, uint32_t max)
{
    if (min > max)
    {
        return min;
    }

    uint32_t range = max - min + 1U;
    if (range == 0)
    {
        return util_random();
    }

    uint32_t limit = UINT32_MAX - (UINT32_MAX % range);
    uint32_t r;
    do
    {
        r = util_random();
    } while (r >= limit);

    return min + (r % range);
}

char *util_random_string(char *buf, size_t len)
{
    if (NULL == buf)
    {
        return NULL;
    }
    // clang-format off
    static char s_characters[] = {
        'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U',
        'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p',
        'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
    };
    // clang-format on
    size_t i = 0;
    for (; i < len - 1; i++)
    {
        buf[i] = s_characters[util_random_range_number(0, sizeof(s_characters) - 1)];
    }
    buf[i] = '\0';
    return buf;
}

int32_t util_file_write(const char *abs_filename, const uint8_t *data, size_t len)
{
    int32_t ret = 0;
    int fd = -1;
    ssize_t n = 0;
    mode_t file_mode = TMP_FILE_DEFAULT_FILE_MODE;
    char tmp_abs_filename[PATH_MAX] = {};

    do
    {
        struct stat st = {};
        if (stat(abs_filename, &st) == 0)
        {
            file_mode = st.st_mode & 0777;
        }
        else if (errno == ENOENT)
        {
            file_mode = TMP_FILE_DEFAULT_FILE_MODE;
        }
        else
        {
            LOG_PRINT_ERROR("stat fail, errno[%d](%s)", errno, strerror(errno));
            ret = -1;
            break;
        }

        if (strlen(abs_filename) + TMP_FILE_SUFFIX_LEN >= sizeof(tmp_abs_filename))
        {
            LOG_PRINT_ERROR("filename len[%zu] is limit", strlen(abs_filename));
            ret = -1;
            break;
        }

        snprintf(tmp_abs_filename, sizeof(tmp_abs_filename), "%s%s", abs_filename, TMP_FILE_SUFFIX);
        if (unlink(tmp_abs_filename) && errno != ENOENT)
        {
            LOG_PRINT_ERROR("unlink fail, errno[%d](%s)", errno, strerror(errno));
            ret = -1;
            break;
        }

        fd = open(tmp_abs_filename, O_WRONLY | O_CREAT | O_EXCL, file_mode);
        if (fd < 0)
        {
            LOG_PRINT_ERROR("open fail, errno[%d](%s)", errno, strerror(errno));
            ret = -1;
            break;
        }

        while (len > 0)
        {
            n = write(fd, data, len);
            if (n < 0)
            {
                LOG_PRINT_ERROR("write fail, errno[%d](%s)", errno, strerror(errno));
                if (errno == EINTR)
                {
                    continue;
                }
                ret = -1;
                break;
            }

            data = data + n;
            len -= (size_t)n;
        }

        if (0 != fsync(fd))
        {
            if (errno != ENOSYS && errno != EINTR)
            {
                LOG_PRINT_ERROR("fsync fail, errno[%d](%s)", errno, strerror(errno));
                ret = -1;
                break;
            }
        }

        if (0 != close(fd))
        {
            LOG_PRINT_ERROR("close fail, errno[%d](%s)", errno, strerror(errno));
            ret = -1;
            break;
        }
        fd = -1;

        if (0 != rename(tmp_abs_filename, abs_filename))
        {
            // rename not support EXDEV(different partition)
            LOG_PRINT_ERROR("rename fail, errno[%d](%s)", errno, strerror(errno));
            ret = -1;
            break;
        }

        ret = 0;
    } while (0);

    if (fd >= 0)
    {
        close(fd);
        fd = -1;
    }
    unlink(tmp_abs_filename);

    return ret;
}

int32_t util_file_read(const char *abs_filename, uint8_t *data, size_t *len)
{
    int32_t ret = 0;
    int fd = -1;
    ssize_t n = 0;
    size_t remain = 0;
    uint8_t *p = NULL;

    // 128 KiB is from:
    // http://git.savannah.gnu.org/cgit/coreutils.git/tree/src/ioblksize.h
    // > As of May 2014, 128KiB is determined to be the minimium
    // > blksize to best minimize system call overhead.

    if (NULL == data || NULL == len || 0 == *len)
    {
        LOG_PRINT_ERROR("invalid param");
        return -1;
    }

    do
    {
        fd = open(abs_filename, O_RDONLY);
        if (fd < 0)
        {
            LOG_PRINT_ERROR("open fail, errno[%d](%s)", errno, strerror(errno));
            ret = -1;
            break;
        }

        remain = *len;
        p = data;
        while (remain > 0)
        {
            n = read(fd, (void *)p, remain);
            if (n < 0)
            {
                LOG_PRINT_ERROR("read fail, errno[%d](%s)", errno, strerror(errno));
                if (errno == EINTR)
                {
                    continue;
                }

                ret = -1; // not done
                break;
            }
            if (0 == n)
            {
                break;
            }
            p += n;
            remain -= (size_t)n;
        }

        *len -= remain;
        if (ret != 0)
        {
            break;
        }
    } while (0);

    if (fd >= 0)
    {
        close(fd);
        fd = -1;
    }

    return ret;
}

int32_t util_execute_command(const char *cmd)
{
    int32_t ret = 0;
    bool sigaction_flag = true;
    struct sigaction sa = {};
    struct sigaction old_sa = {};

    if (NULL == cmd)
    {
        LOG_PRINT_ERROR("invalid param");
        return -1;
    }

    LOG_PRINT_DEBUG("system command[%s]", cmd);

    sa.sa_handler = SIG_DFL;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_NOCLDSTOP | SA_RESTART;

    ret = sigaction(SIGCHLD, &sa, &old_sa);
    if (0 != ret)
    {
        sigaction_flag = false;
        LOG_PRINT_ERROR("sigaction set fail, ret[%d], errno[%d](%s)", ret, errno, strerror(errno));
        return -1;
    }

    do
    {
        int mr = system(cmd);
        if (-1 == mr)
        {
            LOG_PRINT_ERROR("system mr[%d], errno[%d](%s)", mr, errno, strerror(errno));
            ret = -1;
            break;
        }

        if (WIFEXITED(mr)) // exit success
        {
            ret = WEXITSTATUS(mr);
        }
        else // exit fail
        {
            LOG_PRINT_ERROR("system exit fail, WIFEXITED[%d]", WIFEXITED(mr));
            if (WIFSIGNALED(mr))
            {
                ret = WTERMSIG(mr);
            }
            else if (WIFSTOPPED(mr))
            {
                ret = WSTOPSIG(mr);
            }
            else
            {
                ret = -1;
                LOG_PRINT_ERROR("system exit fail");
            }
        }
    } while (0);

    if (sigaction_flag && 0 != sigaction(SIGCHLD, &old_sa, NULL))
    {
        LOG_PRINT_ERROR("sigaction recovery fail, errno[%d](%s)", errno, strerror(errno));
    }

    return ret;
}

/*
BITS 64
%define SYS_WRITE 1
%define SYS_EXIT 60
%define STDOUT 1

_start:
    ;; write
    mov rax, SYS_WRITE
    mov rdi, STDOUT
    lea rsi, [rel hello]
    mov rdx, hello_size
    syscall

    ret

hello: db "Hello, World", 10
hello_size: equ $-hello
*/

/*
// exec a program file(Machine code); like up⬆
    int fd = open(program_filepath, O_RDWR);
    assert(fd >= 0);

    struct stat statbuf;
    int err = fstat(fd, &statbuf);
    assert(err >= 0);

    // MAP_PRIVATE + PROT_EXEC mean not modify the ptr(SIGSEGV);
    void *ptr = mmap(NULL, statbuf.st_size,
                     PROT_EXEC,
                     MAP_PRIVATE,
                     fd, 0);
    assert(ptr != MAP_FAILED);
    close(fd);

    ((void (*)(void))ptr)(); // to function ptr to run! crazy!
*/

int32_t util_get_output_command(const char *cmd, uint8_t *output, size_t output_len)
{
    int32_t ret = 0;
    bool sigaction_flag = true;
    struct sigaction sa = {};
    struct sigaction old_sa = {};
    FILE *fp = NULL;

    if (NULL == cmd)
    {
        LOG_PRINT_ERROR("invalid param");
        return -1;
    }

    LOG_PRINT_DEBUG("get output command[%s]", cmd);

    sa.sa_handler = SIG_DFL;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_NOCLDSTOP | SA_RESTART;

    ret = sigaction(SIGCHLD, &sa, &old_sa);
    if (0 != ret)
    {
        sigaction_flag = false;
        LOG_PRINT_ERROR("sigaction set fail, ret[%d], errno[%d](%s)", ret, errno, strerror(errno));
        return -1;
    }

    do
    {
        fp = popen(cmd, "r");
        if (NULL == fp)
        {
            LOG_PRINT_ERROR("popen fail errno[%d](%s)", errno, strerror(errno));
            ret = -1;
            break;
        }

        if (output && output_len > 0)
        {
            size_t total_read = 0;
            size_t n = 0;
            while (total_read < output_len)
            {
                n = fread(output + total_read, 1, output_len - total_read, fp);
                if (n == 0)
                {
                    int file_errno = ferror(fp);
                    if (0 != file_errno)
                    {
                        LOG_PRINT_ERROR("fread error, file_errno[%d]", file_errno);
                        ret = -1;
                    }
                    break; // EOF or error
                }
                total_read += n;
            }

            if (ret < 0)
            {
                break;
            }
        }

        int mr = pclose(fp);
        if (-1 == mr)
        {
            LOG_PRINT_ERROR("pclose fail, mr[%d], errno[%d](%s)", mr, errno, strerror(errno));
            ret = -1;
            break;
        }

        if (WIFEXITED(mr)) // exit success
        {
            ret = WEXITSTATUS(mr);
        }
        else // exit fail
        {
            LOG_PRINT_ERROR("pclose exit fail, WIFEXITED[%d]", WIFEXITED(mr));
            if (WIFSIGNALED(mr))
            {
                ret = WTERMSIG(mr);
            }
            else if (WIFSTOPPED(mr))
            {
                ret = WSTOPSIG(mr);
            }
            else
            {
                ret = -1;
                LOG_PRINT_ERROR("pclose exit fail");
            }
        }
    } while (0);

    if (sigaction_flag && 0 != sigaction(SIGCHLD, &old_sa, NULL))
    {
        LOG_PRINT_ERROR("sigaction recovery fail, errno[%d](%s)", errno, strerror(errno));
    }

    return ret;
}

/** This code is based on Stevens' Advanced Programming in the UNIX
 * Environment.
 * @brief Turn the current process into a daemon.
 *
 * This function forks the current process and detaches it from the controlling
 * terminal to run as a background daemon. The parent process exits immediately,
 * and the child continues as a daemon.
 *
 * On failure (e.g., fork(), chdir(), or open() fails), it returns false and sets errno.
 * On success, it returns true.
 *
 * @note This function has the following side effects:
 *       - The process becomes a child of init (PID changes).
 *       - A new session is created via setsid().
 *       - The current working directory is changed to "/".
 *       - The umask is set to 0.
 *       - Standard file descriptors (stdin, stdout, stderr) are redirected to /dev/null.
 *
 * @return true if daemonization succeeded, false on error (with errno set).
 */
bool util_daemonize(void)
{
    pid_t pid;

    /* Separate from our parent via fork, so init inherits us. */
    pid = fork();
    /* use _exit() to avoid triggering atexit() processing */
    switch (pid)
    {
        case -1:
            return false;
        case 0:
            break;
        default:
            _exit(0);
    }

    /* Session leader so ^C doesn't whack us. */
    if (setsid() == (pid_t)-1)
        return false;

    /* Move off any mount points we might be in. */
    if (chdir("/") != 0)
        return false;

    /* Discard our parent's old-fashioned umask prejudices. */
    umask(0);

#if 0
    /* Don't hold files open. */
    /* Not necessary: dup2() automatically closes the target fd. */
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
#endif

    /* Many routines write to stderr; that can cause chaos if used
     * for something else, so set it here. */

    int fd = -1;
    do
    {
        fd = open("/dev/null", O_RDWR);
        if (-1 == fd)
        {
            //  LOG_PRINT_ERROR("open fail, errno[%d](%s)", errno, strerror(errno));
            break;
        }

        if (dup2(fd, STDIN_FILENO) == -1)
        {
            // LOG_PRINT_ERROR("dup2 fail, errno[%d](%s)", errno, strerror(errno));
            break;
        }

        if (dup2(fd, STDOUT_FILENO) == -1)
        {
            // LOG_PRINT_ERROR("dup2 fail, errno[%d](%s)", errno, strerror(errno));
            break;
        }

        if (dup2(fd, STDERR_FILENO) == -1)
        {
            // LOG_PRINT_ERROR("dup2 fail, errno[%d](%s)", errno, strerror(errno));
            break;
        }
    } while (0);

    if (fd > STDERR_FILENO)
    {
        if (-1 == close(fd))
        {
            // LOG_PRINT_ERROR("close fail, errno[%d](%s)", errno, strerror(errno));
            return false;
        }
    }

    return true;
}

/**
 * @brief Get the executable filename of a process (basename only).
 *
 * Reads the executable name of the process with given PID by resolving
 * the symbolic link at `/proc/<pid>/exe`. Only the basename (e.g., "sshd")
 * is returned, not the full path.
 *
 * @param[in]  p_pid             Process ID
 * @param[out] p_exec_name       Buffer to store the executable name
 * @param[in]  p_exec_name_maxsize Size of p_exec_name buffer in bytes
 *
 * @return 0 on success, -1 on failure (errno may be set)
 *
 * @note
 *   - Returns the basename of the executable (strips path).
 *   - Result is always null-terminated, even if truncated.
 *   - Depends on `/proc` filesystem (Linux-specific).
 *   - If the executable is deleted or inaccessible, the behavior is implementation-defined.
 */
int32_t util_get_exec_name(pid_t p_pid, char *p_exec_name, size_t p_exec_name_maxsize)
{
    char l_exe_link[PATH_MAX] = {0};
    char exec_name[PATH_MAX] = {0};
    char *l_name_ptr = NULL;

    memset(l_exe_link, 0, sizeof(l_exe_link));
    snprintf(l_exe_link, sizeof(l_exe_link) - 1, "/proc/%d/exe", p_pid);

    if (readlink(l_exe_link, exec_name, sizeof(exec_name) - 1) < 0)
    {
        return -1;
    }

    if ((l_name_ptr = strrchr(exec_name, '/')) == NULL)
    {
        return -1;
    }

    memset(p_exec_name, 0, p_exec_name_maxsize);
    strncpy(p_exec_name, l_name_ptr + 1, p_exec_name_maxsize - 1);

    return 0;
}

int32_t util_get_abs_exec_name(pid_t p_pid, char *p_exec_name, size_t p_exec_name_maxsize)
{
    char l_exe_link[PATH_MAX] = {0};
    char exec_name[PATH_MAX] = {0};

    snprintf(l_exe_link, sizeof(l_exe_link) - 1, "/proc/%d/exe", p_pid);

    if (readlink(l_exe_link, exec_name, sizeof(exec_name) - 1) < 0)
    {
        return -1;
    }

    memset(p_exec_name, 0, p_exec_name_maxsize);
    strncpy(p_exec_name, exec_name, p_exec_name_maxsize - 1);

    return 0;
}

struct util_spinlock
{
    atomic_int lock;
};

void util_spinlock_init(util_spinlock *spinlock)
{
    assert(spinlock != NULL);
    atomic_store_explicit(&spinlock->lock, 0, memory_order_relaxed);
}

void util_spinlock_lock(util_spinlock *spinlock)
{
    assert(spinlock != NULL);
    while (atomic_exchange_explicit(&spinlock->lock, 1, memory_order_acquire) != 0)
    {
    }
}

bool util_spinlock_trylock(util_spinlock *spinlock)
{
    assert(spinlock != NULL);
    return atomic_exchange_explicit(&spinlock->lock, 1, memory_order_acquire) == 0;
}

void util_spinlock_unlock(util_spinlock *spinlock)
{
    assert(spinlock != NULL);
    atomic_store_explicit(&spinlock->lock, 0, memory_order_release);
}

void util_spinlock_fini(util_spinlock *spinlock)
{
    assert(spinlock != NULL);
    (void)spinlock;
}

void util_mtx_init(util_mtx *mtx)
{
    // On most platforms, pthread_mutex_init cannot ever fail, when
    // given NULL attributes.  Linux and Solaris fall into this category.
    // BSD platforms (including OpenBSD, FreeBSD, and macOS) seem to
    // attempt to allocate memory during mutex initialization.

    // An earlier design worked around failures here by using a global
    // fallback lock, but this was potentially racy, complex, and led
    // to some circumstances where we were simply unable to provide
    // adequate debug.

    // If you find you're spending time in this function, consider
    // adding more memory, reducing consumption, or moving to an
    // operating system that doesn't need to do heap allocations
    // to create mutexes.

    // The symptom will be an apparently stuck application spinning
    // every 10 ms trying to allocate this lock.

    pthread_mutexattr_t util_mxattr;
    pthread_mutexattr_init(&util_mxattr);
    (void)pthread_mutexattr_settype(&util_mxattr, PTHREAD_MUTEX_ERRORCHECK);

    while ((pthread_mutex_init(&mtx->mtx, &util_mxattr) != 0) &&
           (pthread_mutex_init(&mtx->mtx, NULL) != 0))
    {
        // We must have memory exhaustion -- ENOMEM, or
        // in some cases EAGAIN.  Wait a bit before we try to
        // give things a chance to settle down.
        util_msleep(10);
    }
    pthread_mutexattr_destroy(&util_mxattr);
}

void util_mtx_fini(util_mtx *mtx)
{
    (void)pthread_mutex_destroy(&mtx->mtx);
}

static void util_pthread_mutex_lock(pthread_mutex_t *m)
{
    int rv;
    if ((rv = pthread_mutex_lock(m)) != 0)
    {
        LOG_PRINT_ERROR("pthread_mutex_lock: [%d](%s)", rv, strerror(rv));
        abort();
    }
}

static void util_pthread_mutex_unlock(pthread_mutex_t *m)
{
    int rv;
    if ((rv = pthread_mutex_unlock(m)) != 0)
    {
        LOG_PRINT_ERROR("pthread_mutex_unlock: [%d](%s)", rv, strerror(rv));
        abort();
    }
}

static void util_pthread_cond_broadcast(pthread_cond_t *c)
{
    int rv;
    if ((rv = pthread_cond_broadcast(c)) != 0)
    {
        LOG_PRINT_ERROR("pthread_cond_broadcast: [%d](%s)", rv, strerror(rv));
        abort();
    }
}

static void util_pthread_cond_signal(pthread_cond_t *c)
{
    int rv;
    if ((rv = pthread_cond_signal(c)) != 0)
    {
        LOG_PRINT_ERROR("pthread_cond_signal: [%d](%s)", rv, strerror(rv));
        abort();
    }
}

static void util_pthread_cond_wait(pthread_cond_t *c, pthread_mutex_t *m)
{
    int rv;
    if ((rv = pthread_cond_wait(c, m)) != 0)
    {
        LOG_PRINT_ERROR("pthread_cond_wait: [%d](%s)", rv, strerror(rv));
        abort();
    }
}

static int util_pthread_cond_timedwait(pthread_cond_t *c, pthread_mutex_t *m, struct timespec *ts)
{
    int rv;

    switch ((rv = pthread_cond_timedwait(c, m, ts)))
    {
        case 0:
            return (0);
        case ETIMEDOUT:
            return (ETIMEDOUT);
        case EAGAIN:
            return (EAGAIN);
        default:
            LOG_PRINT_ERROR("pthread_cond_timedwait: [%d](%s)", rv, strerror(rv));
            abort();
    }

    return (rv);
}

void util_mtx_lock(util_mtx *mtx)
{
    util_pthread_mutex_lock(&mtx->mtx);
}

void util_mtx_unlock(util_mtx *mtx)
{
    util_pthread_mutex_unlock(&mtx->mtx);
}

void util_cv_init(util_cv *cv, util_mtx *mtx)
{
    // See the comments in util_mtx_init.  Almost everywhere this simply does not/cannot fail.

    pthread_condattr_t util_cvattr;
    pthread_condattr_init(&util_cvattr);
    if (pthread_condattr_setclock(&util_cvattr, CLOCK_MONOTONIC) != 0)
    {
        pthread_condattr_destroy(&util_cvattr);
        return;
    }
    while (pthread_cond_init(&cv->cv, &util_cvattr) != 0)
    {
        util_msleep(10);
    }
    cv->mtx = &mtx->mtx;
    pthread_condattr_destroy(&util_cvattr);
}

void util_cv_fini(util_cv *cv)
{
    int rv;

    if ((rv = pthread_cond_destroy(&cv->cv)) != 0)
    {
        LOG_PRINT_ERROR("pthread_cond_destroy: [%d](%s)", rv, strerror(rv));
        abort();
    }
    cv->mtx = NULL;
}

void util_cv_wake(util_cv *cv)
{
    util_pthread_cond_broadcast(&cv->cv);
}

void util_cv_wake1(util_cv *cv)
{
    util_pthread_cond_signal(&cv->cv);
}

void util_cv_wait(util_cv *cv)
{
    util_pthread_cond_wait(&cv->cv, cv->mtx);
}

int util_cv_until(util_cv *cv, uint64_t when)
{
    struct timespec ts;
    ts.tv_sec = (time_t)(when);
    ts.tv_nsec = (long)((when % 1000) * 1000000);
    return util_pthread_cond_timedwait(&cv->cv, cv->mtx, &ts);
}

static void *util_thr_main(void *arg)
{
    util_thr *thr = arg;
    sigset_t set;

    // Suppress (block) SIGPIPE for this thread.
    sigemptyset(&set);
    sigaddset(&set, SIGPIPE);
    (void)pthread_sigmask(SIG_BLOCK, &set, NULL);

    thr->func(thr->arg);
    return (NULL);
}

int util_thr_init(util_thr *thr, void (*fn)(void *), void *arg)
{
    int rv;

    thr->func = fn;
    thr->arg = arg;

    pthread_attr_t util_thrattr;
    pthread_attr_init(&util_thrattr);
    rv = pthread_create(&thr->tid, &util_thrattr, util_thr_main, thr);
    if (rv != 0)
    {
        LOG_PRINT_ERROR("pthread_create: [%d](%s)", rv, strerror(rv));
    }
    pthread_attr_destroy(&util_thrattr);
    return (rv);
}

void util_thr_fini(util_thr *thr)
{
    int rv;

    if ((rv = pthread_join(thr->tid, NULL)) != 0)
    {
        LOG_PRINT_ERROR("pthread_join: [%d](%s)", rv, strerror(rv));
        abort();
    }
}

bool util_thr_is_self(util_thr *thr)
{
    return (pthread_self() == thr->tid);
}

void util_thr_set_name(util_thr *thr, const char *name)
{
    if (thr == NULL)
    {
        pthread_setname_np(pthread_self(), name);
    }
    else
    {
        pthread_setname_np(thr->tid, name);
    }
}

// atomic bool
void util_atomic_init_bool(util_atomic_bool *v)
{
    atomic_init(&v->v, false);
}

void util_atomic_set_bool(util_atomic_bool *v, bool b)
{
    atomic_store(&v->v, b);
}

bool util_atomic_get_bool(util_atomic_bool *v)
{
    return (atomic_load(&v->v));
}

bool util_atomic_swap_bool(util_atomic_bool *v, bool b)
{
    return (atomic_exchange(&v->v, b));
}

// atomic int
void util_atomic_init(util_atomic_int *v)
{
    atomic_init(&v->v, 0);
}

int util_atomic_get(util_atomic_int *v)
{
    return (atomic_load(&v->v));
}

void util_atomic_set(util_atomic_int *v, int i)
{
    atomic_store(&v->v, i);
}

int util_atomic_swap(util_atomic_int *v, int i)
{
    return (atomic_exchange(&v->v, i));
}

void util_atomic_add(util_atomic_int *v, int bump)
{
    (void)atomic_fetch_add(&v->v, bump);
}

void util_atomic_sub(util_atomic_int *v, int bump)
{
    (void)atomic_fetch_sub(&v->v, bump);
}

void util_atomic_inc(util_atomic_int *v)
{
    (void)atomic_fetch_add(&v->v, 1);
}

void util_atomic_dec(util_atomic_int *v)
{
    (void)atomic_fetch_sub(&v->v, 1);
}

int util_atomic_dec_nv(util_atomic_int *v)
{
    return (atomic_fetch_sub(&v->v, 1) - 1);
}

bool util_atomic_cas(util_atomic_int *v, int comp, int newi)
{
    return (atomic_compare_exchange_strong(&v->v, &comp, newi));
}

// atomic u64
void util_atomic_init64(util_atomic_u64 *v)
{
    atomic_init(&v->v, 0);
}

uint64_t util_atomic_get64(util_atomic_u64 *v)
{
    return (uint64_t)(atomic_load(&v->v));
}

void util_atomic_set64(util_atomic_u64 *v, uint64_t u)
{
    atomic_store(&v->v, (uint_fast64_t)u);
}

uint64_t util_atomic_swap64(util_atomic_u64 *v, uint64_t u)
{
    return ((uint64_t)atomic_exchange(&v->v, (uint_fast64_t)u));
}

void util_atomic_add64(util_atomic_u64 *v, uint64_t bump)
{
    (void)atomic_fetch_add(&v->v, (uint_fast64_t)bump);
}

void util_atomic_sub64(util_atomic_u64 *v, uint64_t bump)
{
    (void)atomic_fetch_sub(&v->v, (uint_fast64_t)bump);
}

void util_atomic_inc64(util_atomic_u64 *v)
{
    atomic_fetch_add(&v->v, 1);
}

uint64_t util_atomic_dec64_nv(util_atomic_u64 *v)
{
    uint64_t ov;
    // C11 atomics give the old rather than new value.
    ov = (uint64_t)atomic_fetch_sub(&v->v, 1);
    return (ov - 1);
}

bool util_atomic_cas64(util_atomic_u64 *v, uint64_t comp, uint64_t newu64)
{
    // It's possible that uint_fast64_t is not the same type underneath
    // as uint64_t.  (Would be unusual!)
    uint_fast64_t cv = (uint_fast64_t)comp;
    uint_fast64_t nv = (uint_fast64_t)newu64;
    return (atomic_compare_exchange_strong(&v->v, &cv, nv));
}

// atomic ptr
void *util_atomic_get_ptr(util_atomic_ptr *v)
{
    return ((void *)atomic_load(&v->v));
}

void util_atomic_set_ptr(util_atomic_ptr *v, void *p)
{
    atomic_store(&v->v, (uintptr_t)p);
}

int util_eventfd_open(int *wfd, int *rfd)
{
    int fd;

    if ((fd = eventfd(0, EFD_CLOEXEC)) < 0)
    {
        return -errno;
    }
    (void)fcntl(fd, F_SETFD, FD_CLOEXEC);
    (void)fcntl(fd, F_SETFL, O_NONBLOCK);

    *wfd = *rfd = fd;
    return (0);
}

void util_eventfd_raise(int wfd)
{
    uint64_t one = 1;
    ssize_t r = write(wfd, &one, sizeof(one));
    (void)r; // Ignore return value
}

void util_eventfd_clear(int rfd)
{
    uint64_t val;
    ssize_t r = read(rfd, &val, sizeof(val));
    (void)r; // Ignore return value
}

void util_eventfd_close(int wfd, int rfd)
{
    assert(wfd == rfd);
    (void)rfd;
    (void)close(wfd);
}

int util_pipe_open(int *wfd, int *rfd)
{
    int fds[2];

    if (pipe(fds) < 0)
    {
        return -errno;
    }
    *wfd = fds[1];
    *rfd = fds[0];

    (void)fcntl(fds[0], F_SETFD, FD_CLOEXEC);
    (void)fcntl(fds[1], F_SETFD, FD_CLOEXEC);
    (void)fcntl(fds[0], F_SETFL, O_NONBLOCK);
    (void)fcntl(fds[1], F_SETFL, O_NONBLOCK);

    return (0);
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-result"
void util_pipe_raise(int wfd)
{
    char c = 1;

    ssize_t r = write(wfd, &c, 1);
    (void)r; // Ignore return value
}
#pragma GCC diagnostic pop

void util_pipe_clear(int rfd)
{
    char buf[32];

    for (;;)
    {
        // Completely drain the pipe, but don't wait.  This coalesces
        // events somewhat.
        if (read(rfd, buf, sizeof(buf)) <= 0)
        {
            return;
        }
    }
}

void util_pipe_close(int wfd, int rfd)
{
    (void)close(wfd);
    (void)close(rfd);
}

int util_socket_pair(int fds[2])
{
    int rv;
    rv = socketpair(AF_UNIX, SOCK_STREAM, 0, fds);
    if (rv != 0)
    {
        return -errno;
    }

#ifdef SO_NOSIGPIPE
    int set = 1;
    setsockopt(fds[0], SOL_SOCKET, SO_NOSIGPIPE, (void *)&set, sizeof(int));
    setsockopt(fds[1], SOL_SOCKET, SO_NOSIGPIPE, (void *)&set, sizeof(int));
#endif

    return (0);
}

void util_pollable_init(util_pollable *p)
{
    util_atomic_init_bool(&p->p_raised);
    util_atomic_set64(&p->p_fds, (uint64_t)-1);
}

void util_pollable_fini(util_pollable *p)
{
    uint64_t fds;

    fds = util_atomic_get64(&p->p_fds);
    if (fds != (uint64_t)-1)
    {
        int rfd, wfd;
        // Read in the high order, write in the low order.
        rfd = RFD(fds);
        wfd = WFD(fds);
        util_eventfd_close(rfd, wfd);
    }
}

void util_pollable_raise(util_pollable *p)
{
    if (!util_atomic_swap_bool(&p->p_raised, true))
    {
        uint64_t fds;
        if ((fds = util_atomic_get64(&p->p_fds)) != (uint64_t)-1)
        {
            util_eventfd_raise(WFD(fds));
        }
    }
}

void util_pollable_clear(util_pollable *p)
{
    if (util_atomic_swap_bool(&p->p_raised, false))
    {
        uint64_t fds;
        if ((fds = util_atomic_get64(&p->p_fds)) != (uint64_t)-1)
        {
            util_eventfd_clear(RFD(fds));
        }
    }
}

int util_pollable_getfd(util_pollable *p, int *fdp)
{
    if (p == NULL)
    {
        return (-1);
    }

    for (;;)
    {
        int rfd;
        int wfd;
        int rv;
        uint64_t fds;

        if ((fds = util_atomic_get64(&p->p_fds)) != (uint64_t)-1)
        {
            *fdp = RFD(fds);
            return (0);
        }
        if ((rv = util_eventfd_open(&wfd, &rfd)) != 0)
        {
            return (rv);
        }
        fds = FD_JOIN(wfd, rfd);

        if (util_atomic_cas64(&p->p_fds, (uint64_t)-1, fds))
        {
            if (util_atomic_get_bool(&p->p_raised))
            {
                util_eventfd_raise(wfd);
            }
            *fdp = rfd;
            return (0);
        }

        // Someone beat us.  Close ours, and try again.
        util_eventfd_close(wfd, rfd);
    }
}
