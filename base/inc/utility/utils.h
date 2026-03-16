#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <errno.h>
#include <assert.h>
#include <stddef.h> // offsetof(T, m) = 成员 m 在结构体 T 中的起始偏移
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "platform.h"
#include "utils_generated.h"

#ifndef UTIL_DEPRECATED
#if defined(__GNUC__) || defined(__clang__)
#define UTIL_DEPRECATED(message) __attribute__((deprecated(message)))
#elif defined(_MSC_VER)
#define UTIL_DEPRECATED(message) __declspec(deprecated(message))
#else
#define UTIL_DEPRECATED(...)
#endif
#endif

#ifndef UTIL_UNUSED
#if defined(__GNUC__) || defined(__clang__)
#define UTIL_UNUSED(x) x __attribute__((unused))
#else
#define UTIL_UNUSED(x) (void)(x)
#endif
#endif

#ifndef UTIL_ALWAYS_INLINE
#if defined(__GNUC__) || defined(__clang__)
#define UTIL_ALWAYS_INLINE __attribute__((always_inline))
#elif defined(_MSC_VER)
#define UTIL_ALWAYS_INLINE __forceinline
#else
#define UTIL_ALWAYS_INLINE
#endif
#endif

#ifndef UTIL_TOSTR
#define _UTIL_TOSTR(x) #x
#define UTIL_TOSTR(x)  _UTIL_TOSTR(x)
#endif

#ifndef UTIL_CONN
#define _UTIL_CONN(x, y)          x##y
#define UTIL_CONN(x, y)           _UTIL_CONN(x, y)
#define UTIL_CONN3(x, y, z)       UTIL_CONN(x, UTIL_CONN(y, z))
#define UTIL_CONN4(x, y, z, w)    UTIL_CONN(UTIL_CONN(x, y), UTIL_CONN(z, w))
#define UTIL_CONN5(x, y, z, w, m) UTIL_CONN(UTIL_CONN4(x, y, z, w), m)
#endif

#ifndef UTIL_ASSERT
#include <assert.h>
#define UTIL_ASSERT(x) assert(x)
#endif

#ifndef UTIL_STATIC_ASSERT
#if defined(__cplusplus) && __cplusplus >= 201103L
// C++11
#define UTIL_STATIC_ASSERT(e) static_assert(e, "Assertion failed: " #e)
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
// C11
#define UTIL_STATIC_ASSERT(e) _Static_assert(e, "Assertion failed: " #e)
#else
// C89/C99
#define UTIL_STATIC_ASSERT(e) typedef char UTIL_UNUSED(UTIL_CONN(_util_static_assert_fail_, __LINE__)[(e) ? 1 : -1])
#endif
#endif

#ifndef UTIL_ARRAY_SIZE
#if defined(__GNUC__) || defined(__clang__)
#define UTIL_ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]) + _UTIL_MUST_BE_ARRAY(arr))
#else
#define UTIL_ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif
#endif

#ifndef UTIL_MIN
#define UTIL_MIN(x, y)                 \
    ({                                 \
        typeof(x) _min1 = (x);         \
        typeof(y) _min2 = (y);         \
        (void)(&_min1 == &_min2);      \
        _min1 < _min2 ? _min1 : _min2; \
    })
#endif

#ifndef UTIL_MAX
#define UTIL_MAX(x, y)                 \
    ({                                 \
        typeof(x) _max1 = (x);         \
        typeof(y) _max2 = (y);         \
        (void)(&_max1 == &_max2);      \
        _max1 > _max2 ? _max1 : _max2; \
    })
#endif

#ifndef UTIL_SWAP
#define UTIL_SWAP(x, y)       \
    ({                        \
        typeof(x) _tmp = (x); \
        (x) = (y);            \
        (y) = _tmp;           \
    })
#endif

// UTIL_OFFSETOFEND(T, m) = 成员 m 在结构体 T 中的起始偏移 + 成员 m 的大小
#ifndef UTIL_OFFSETOFEND
#define UTIL_OFFSETOFEND(type, member) (offsetof(type, member) + sizeof(((type *)0)->member))
#endif

// UTIL_CONTAINER_OF(ptr, type, member) = 根据结构体 type 中成员 member 的指针 ptr, 计算出该结构体的起始地址
#ifndef UTIL_CONTAINER_OF
#define UTIL_CONTAINER_OF(ptr, type, member) ((type *)((uintptr_t)(ptr) - offsetof(type, member)))
#endif

#ifndef UTIL_BITGET
#define UTIL_BITGET_32(i, n) ((i) & (1U << (n)))
#define UTIL_BITGET_64(i, n) ((i) & (1ULL << (n)))
#define UTIL_BITGET          BITGET_32
#endif

#ifndef UTIL_BITGETS
#define UTIL_BITGETS_32(i, start, n)         (((uint32_t)(i) >> (start)) & ((1U << (n)) - 1))
#define UTIL_BITGETS_64(i, start, n)         (((uint64_t)(i) >> (start)) & ((1ULL << (n)) - 1))
#define UTIL_BITGETS                         UTIL_BITGETS_32
#define UTIL_BITGETS_M_TO_N_32(i, high, low) (((uint32_t)(i) >> (low)) & ((1U << ((high) - (low) + 1)) - 1))
#define UTIL_BITGETS_M_TO_N_64(i, high, low) (((uint64_t)(i) >> (low)) & ((1ULL << ((high) - (low) + 1)) - 1))

#define UTIL_BITGETS_N_TO_M_32(i, high, low) util_bitreverse_n_32((UTIL_BITGETS_M_TO_N_32(i, high, low)), (high) - (low) + 1)
#define UTIL_BITGETS_N_TO_M_64(i, high, low) util_bitreverse_n_64((UTIL_BITGETS_M_TO_N_64(i, high, low)), (high) - (low) + 1)
#endif

#ifndef UTIL_BITSET
#define UTIL_BITSET_32(p, n) (*(p) |= (1U << (n)))
#define UTIL_BITSET_64(p, n) (*(p) |= (1ULL << (n)))
#define UTIL_BITSET          UTIL_BITSET_32
#endif

#ifndef UTIL_BITCLR
#define UTIL_BITCLR_32(p, n) (*(p) &= ~(1U << (n)))
#define UTIL_BITCLR_64(p, n) (*(p) &= ~(1ULL << (n)))
#define UTIL_BITCLR          UTIL_BITCLR_32
#endif

#ifndef UTIL_BITFLIP
#define UTIL_BITFLIP_32(p, n) (*(p) ^= (1U << (n)))
#define UTIL_BITFLIP_64(p, n) (*(p) ^= (1ULL << (n)))
#define UTIL_BITFLIP          UTIL_BITFLIP_32
#endif

#ifndef UTIL_BITTEST
#define UTIL_BITTEST_32(p, n) !!((p) & (1U << (n)))
#define UTIL_BITTEST_64(p, n) !!((p) & (1ULL << (n)))
#define UTIL_BITTEST          UTIL_BITTEST_32
#endif

// misc
extern uint32_t util_bitreverse_n_32(uint32_t val, int n);
extern uint64_t util_bitreverse_n_64(uint64_t val, int n);
extern unsigned long util_floor2e(unsigned long num); // floor of power-of-two ≤ num
extern unsigned long util_ceil2e(unsigned long num);  // ceiling of power-of-two ≥ num

extern void util_msleep(uint64_t ms);
extern void util_msleep_v2(uint64_t ms); // impl by poll

// boot ignore wakeup/sleep
// clock(ms)
extern uint64_t util_clock_now(void);
extern uint64_t util_clock_mono(void);
extern uint64_t util_clock_boot(void);

// time(s)
extern time_t util_time_s_now(void);
extern time_t util_time_s_mono(void);
extern time_t util_time_s_boot(void);

extern struct timespec util_time_now(void);
extern struct timespec util_time_mono(void);
extern struct timespec util_time_boot(void);

extern struct timespec util_time_after(uint32_t ms);
extern struct timespec util_time_mono_after(uint32_t ms);
extern struct timespec util_time_boot_after(uint32_t ms);

extern bool util_timezone(long int *tz);

// random
// see application/modern_cpp_study/random
extern uint32_t util_random(void);                             // arc4random
extern uint32_t util_random_v2(void);                          // getentropy
extern uint32_t util_random_v3(void);                          // getrandom
extern uint32_t util_random_v4(void);                          // urandom
uint32_t util_random_range_number(uint32_t min, uint32_t max); // [min, max]
extern char *util_random_string(char *buf, size_t len);

// file
extern int32_t util_file_write(const char *abs_filename, const uint8_t *data, size_t len);
extern int32_t util_file_read(const char *abs_filename, uint8_t *data, size_t *len);

// system
extern int32_t util_execute_command(const char *cmd);
extern int32_t util_get_output_command(const char *cmd, uint8_t *output, size_t output_len);
extern bool util_daemonize(void);
extern int32_t util_get_exec_name(pid_t p_pid, char *p_exec_name, size_t p_exec_name_maxsize);
int32_t util_get_abs_exec_name(pid_t p_pid, char *p_exec_name, size_t p_exec_name_maxsize);

// mutex & cond & thread
typedef struct util_mtx
{
    pthread_mutex_t mtx;
} util_mtx;

typedef struct util_cv
{
    pthread_cond_t cv;
    pthread_mutex_t *mtx;
} util_cv;

typedef struct util_thr
{
    pthread_t tid;
    void (*func)(void *);
    void *arg;
} util_thr;

typedef struct util_spinlock util_spinlock;

// util_spinlock_init initializes a spinlock structure. An initialized spinlock
// must be distinguishable from zeroed memory (though the implementation sets
// the atomic integer to 0, the act of initialization establishes the valid state).
void util_spinlock_init(util_spinlock *);

// util_spinlock_lock locks the spinlock. If the lock is already held by another
// thread, this function will busy-wait (spin) until the lock becomes available.
// This is not recursive -- a spinlock can only be entered once by a thread.
void util_spinlock_lock(util_spinlock *);

// util_spinlock_trylock attempts to lock the spinlock without blocking.
// Returns true if the lock was successfully acquired, false if it was already held.
bool util_spinlock_trylock(util_spinlock *);

// util_spinlock_unlock unlocks the spinlock. This can only be performed by the
// thread that currently owns the spinlock.
void util_spinlock_unlock(util_spinlock *);

// util_spinlock_fini destroys the spinlock and releases any resources allocated
// for its use. Since this implementation relies on atomic operations without
// dynamic allocation, this effectively does nothing, but serves as a lifecycle marker.
// If the spinlock is zeroed memory, this should do nothing.
void util_spinlock_fini(util_spinlock *);

// util_mtx_init initializes a mutex structure.  An initialized mutex must
// be distinguishable from zeroed memory.
extern void util_mtx_init(util_mtx *);

// util_mtx_fini destroys the mutex and releases any resources allocated
// for it's use.  If the mutex is zeroed memory, this should do nothing.
extern void util_mtx_fini(util_mtx *);

// util_mtx_lock locks the mutex.  This is not recursive -- a mutex can
// only be entered once.
extern void util_mtx_lock(util_mtx *);

// util_mtx_unlock unlocks the mutex.  This can only be performed by the
// thread that owned the mutex.
extern void util_mtx_unlock(util_mtx *);

// util_cv_init initializes a condition variable.  We require a mutex be
// supplied with it, and that mutex must always be held when performing any
// operations on the condition variable (other than fini.)  As with mutexes, an
// initialized mutex should be distinguishable from zeroed memory.
extern void util_cv_init(util_cv *, util_mtx *);

// util_cv_fini releases all resources associated with condition variable.
// If the cv points to just zeroed memory (was never initialized), it does
// nothing.
extern void util_cv_fini(util_cv *);

// util_cv_wake wakes all waiters on the condition.  This should be
// called with the lock held.
extern void util_cv_wake(util_cv *);

// util_cv_wake1 wakes only a single waiter.  Use with caution
// to avoid losing the wakeup when multiple waiters may be present.
extern void util_cv_wake1(util_cv *);

// util_cv_wait waits for a wake up on the condition variable.  The
// associated lock is atomically released and reacquired upon wake up.
// Callers can be spuriously woken.  The associated lock must be held.
extern void util_cv_wait(util_cv *);

// util_cv_until waits for a wakeup on the condition variable, or
// until the system time reaches the specified absolute time.  (It is an
// absolute form of util_cond_timedwait.)  Early wakeups are possible, so
// check the condition.  It will return either ETIMEDOUT, or 0.
extern int util_cv_until(util_cv *, uint64_t when);

// util_thr_init creates a thread that runs the given function. The
// thread receives a single argument.  The thread starts execution
// immediately.
extern int util_thr_init(util_thr *, void (*)(void *), void *);

// util_thr_fini waits for the thread to exit, and then releases any
// resources associated with the thread.  After this returns, it
// is an error to reference the thread in any further way.
extern void util_thr_fini(util_thr *);

// util_thr_is_self returns true if the caller is the thread
// identified, and false otherwise.  (This allows some deadlock
// prevention in callbacks, for example.)
extern bool util_thr_is_self(util_thr *);

// util_thr_set_name is used to set the thread name, which
// should be a short ASCII string.  It may or may not be supported --
// this is intended to facilitate debugging.
extern void util_thr_set_name(util_thr *, const char *);

// eventfd
// util_eventfd creates a pair of linked file descriptors that are
// suitable for notification via SENDFD/RECVFD.  These are platform
// specific and exposed to applications for integration into event loops.
// The first eventfd is written to by user to notify, and the second eventfd is
// generally read from to clear the event.   The implementation is not
// obliged to provide two eventfds -- for example eventfd can be used with
// just a single file descriptor.  In such a case the implementation may
// just provide the same value twice.
extern int util_eventfd_open(int *wfd, int *rfd);

// util_eventfd_raise pushes a notification to the eventfd.  Usually this
// will just be a non-blocking attempt to write a single byte.  It may
// however use any other underlying system call that is appropriate.
extern void util_eventfd_raise(int wfd);

// util_eventfd_clear clears all notifications from the eventfd.  Usually this
// will just be a non-blocking read.  (The call should attempt to read
// all data on a eventfd, for example.)
extern void util_eventfd_clear(int rfd);

// util_eventfd_close closes both eventfds that were provided by the open
// routine.
extern void util_eventfd_close(int wfd, int rfd);

// pipe
// util_pipe creates a pair of linked file descriptors that are
// suitable for notification via SENDFD/RECVFD.  These are platform
// specific and exposed to applications for integration into event loops.
// The first pipe is written to by user to notify, and the second pipe is
// generally read from to clear the event.   The implementation is not
// obliged to provide two pipes -- for example eventfd can be used with
// just a single file descriptor.  In such a case the implementation may
// just provide the same value twice.
extern int util_pipe_open(int *wfd, int *rfd);

// util_pipe_raise pushes a notification to the pipe.  Usually this
// will just be a non-blocking attempt to write a single byte.  It may
// however use any other underlying system call that is appropriate.
extern void util_pipe_raise(int wfd);

// util_pipe_clear clears all notifications from the pipe.  Usually this
// will just be a non-blocking read.  (The call should attempt to read
// all data on a pipe, for example.)
extern void util_pipe_clear(int rfd);

// util_pipe_close closes both pipes that were provided by the open
// routine.
extern void util_pipe_close(int wfd, int rfd);

// socketpair

// util_socket_pair is used to create a socket pair using socketpair()
// on POSIX systems.  (Windows might provide a similar solution, using
// AF_UNIX at some point, in which case the arguments will actually be
// an array of HANDLEs.)  If not supported, this returns ENOTSUP.
//
// This API can only create a pair of open file descriptors, suitable for use
// with the socket transport, each bound to the other.  The transport must be
// a bidirectional reliable byte stream.  This should be suitable for use
// in APIs to transport file descriptors, or across a fork/exec boundary (so
// that child processes may use these with socket to inherit a socket that is
// connected to the parent.)
extern int util_socket_pair(int fds[2]);

#ifndef __cplusplus

// atomic
#include <stdatomic.h>

typedef struct util_atomic_bool
{
    atomic_bool v;
} util_atomic_bool;

typedef struct util_atomic_int
{
    atomic_int v;
} util_atomic_int;

typedef struct util_atomic_u64
{
    atomic_uint_fast64_t v;
} util_atomic_u64;

typedef struct util_atomic_ptr
{
    atomic_uintptr_t v;
} util_atomic_ptr;

extern void util_atomic_init_bool(util_atomic_bool *v);
extern void util_atomic_set_bool(util_atomic_bool *v, bool b);
extern bool util_atomic_get_bool(util_atomic_bool *v);
extern bool util_atomic_swap_bool(util_atomic_bool *v, bool b);

// atomic int
extern void util_atomic_init(util_atomic_int *v);
extern int util_atomic_get(util_atomic_int *v);
extern void util_atomic_set(util_atomic_int *v, int i);
extern int util_atomic_swap(util_atomic_int *v, int i);
extern void util_atomic_add(util_atomic_int *v, int bump);
extern void util_atomic_sub(util_atomic_int *v, int bump);
extern void util_atomic_inc(util_atomic_int *v);
extern void util_atomic_dec(util_atomic_int *v);
extern int util_atomic_dec_nv(util_atomic_int *v);
extern bool util_atomic_cas(util_atomic_int *v, int comp, int newi);

// atomic u64
extern void util_atomic_init64(util_atomic_u64 *v);
extern uint64_t util_atomic_get64(util_atomic_u64 *v);
extern void util_atomic_set64(util_atomic_u64 *v, uint64_t u);
extern uint64_t util_atomic_swap64(util_atomic_u64 *v, uint64_t u);
extern void util_atomic_add64(util_atomic_u64 *v, uint64_t bump);
extern void util_atomic_sub64(util_atomic_u64 *v, uint64_t bump);
extern void util_atomic_inc64(util_atomic_u64 *v);
extern uint64_t util_atomic_dec64_nv(util_atomic_u64 *v);
extern bool util_atomic_cas64(util_atomic_u64 *v, uint64_t comp, uint64_t newu64);

// atomic ptr
extern void *util_atomic_get_ptr(util_atomic_ptr *v);
extern void util_atomic_set_ptr(util_atomic_ptr *v, void *p);

// pollable can impl by eventfd or pipe
/*
1. init and get a pollable fd and add it to epoll
    util_pollable pollable;
    util_pollable_init(&pollable);
    int notify_fd;
    util_pollable_getfd(&pollable, &notify_fd);  // ← 获取用于监听的 fd(通常是读端)
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, notify_fd, &(struct epoll_event){ .events = EPOLLIN });
2. another I/O thread
    while (running)
    {
        epoll_wait(epoll_fd, events, ...);
        for (...)
        {
            if (event.fd == notify_fd)
            {
                // 有通知到达!
                util_pollable_clear(&pollable); // ← 消费通知(读空 pipe 或重置状态)
                handle_notification();         // 执行对应逻辑(如退出,处理任务等)
            }
        }
    }
*/

//  util_pollable implementation details are private.  Only here for inlining.
// We have joined the write and read file descriptors into a single
// atomic 64, so we can update them together (and we can use cas to be sure
// that such updates are always safe.)
typedef struct util_pollable
{
    util_atomic_u64 p_fds;
    util_atomic_bool p_raised;
} util_pollable;

extern void util_pollable_init(util_pollable *p);
extern void util_pollable_fini(util_pollable *p);
extern void util_pollable_raise(util_pollable *p);
extern void util_pollable_clear(util_pollable *p);
extern int util_pollable_getfd(util_pollable *p, int *fdp);
#endif

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

#include <atomic>
struct util_atomic_bool
{
    std::atomic<bool> v;
};

struct util_atomic_int
{
    std::atomic<int> v;
};

struct util_atomic_u64
{
    std::atomic<uint64_t> v;
};

struct util_atomic_ptr
{
    std::atomic<void *> v;
};

// clang-format off
// atomic bool
inline void util_atomic_init_bool(util_atomic_bool *v) { v->v.store(false, std::memory_order_relaxed); }
inline void util_atomic_set_bool(util_atomic_bool *v, bool b) { v->v.store(b, std::memory_order_seq_cst); }
inline bool util_atomic_get_bool(util_atomic_bool *v) { return v->v.load(std::memory_order_seq_cst); }
inline bool util_atomic_swap_bool(util_atomic_bool *v, bool b) { return v->v.exchange(b, std::memory_order_seq_cst); }

// atomic int
inline void util_atomic_init(util_atomic_int *v) { v->v.store(0, std::memory_order_relaxed); }
inline int util_atomic_get(util_atomic_int *v) { return v->v.load(std::memory_order_seq_cst); }
inline void util_atomic_set(util_atomic_int *v, int i) { v->v.store(i, std::memory_order_seq_cst); }
inline int util_atomic_swap(util_atomic_int *v, int i) { return v->v.exchange(i, std::memory_order_seq_cst); }
inline void util_atomic_add(util_atomic_int *v, int bump) { v->v.fetch_add(bump, std::memory_order_seq_cst); }
inline void util_atomic_sub(util_atomic_int *v, int bump) { v->v.fetch_sub(bump, std::memory_order_seq_cst); }
inline void util_atomic_inc(util_atomic_int *v) { v->v.fetch_add(1, std::memory_order_seq_cst); }
inline void util_atomic_dec(util_atomic_int *v) { v->v.fetch_sub(1, std::memory_order_seq_cst); }
inline int util_atomic_dec_nv(util_atomic_int *v) { return v->v.fetch_sub(1, std::memory_order_seq_cst) - 1; }
inline bool util_atomic_cas(util_atomic_int *a, int comp, int newi) \
    { int expected = comp; return a->v.compare_exchange_strong(expected, newi, std::memory_order_seq_cst); }

// atomic u64
inline void util_atomic_init64(util_atomic_u64 *v) { v->v.store(0, std::memory_order_relaxed); }
inline uint64_t util_atomic_get64(util_atomic_u64 *v) { return v->v.load(std::memory_order_seq_cst); }
inline void util_atomic_set64(util_atomic_u64 *v, uint64_t u) { v->v.store(u, std::memory_order_seq_cst); }
inline uint64_t util_atomic_swap64(util_atomic_u64 *v, uint64_t u) { return v->v.exchange(u, std::memory_order_seq_cst); }
inline void util_atomic_add64(util_atomic_u64 *v, uint64_t bump) {  v->v.fetch_add(bump, std::memory_order_seq_cst); }
inline void util_atomic_sub64(util_atomic_u64 *v, uint64_t bump) { v->v.fetch_sub(bump, std::memory_order_seq_cst); }
inline void util_atomic_inc64(util_atomic_u64 *v) { v->v.fetch_add(1, std::memory_order_seq_cst); }
inline uint64_t util_atomic_dec64_nv(util_atomic_u64 *v) { return v->v.fetch_sub(1, std::memory_order_seq_cst) - 1; }
inline bool util_atomic_cas64(util_atomic_u64 *v, uint64_t comp, uint64_t newu64) \
    { uint64_t expected = comp; return v->v.compare_exchange_strong(expected, newu64, std::memory_order_seq_cst); }

// atomic ptr
inline void* util_atomic_get_ptr(util_atomic_ptr *v) {  return v->v.load(std::memory_order_seq_cst); }
inline void util_atomic_set_ptr(util_atomic_ptr *v, void *p) { v->v.store(p, std::memory_order_seq_cst); }
// clang-format on

// We pack the wfd and rfd into a uint64_t so that we can update the pair
// atomically and use util_atomic_cas64, to be lock free.
#define WFD(fds)          ((int)((fds) & 0xffffffffu))
#define RFD(fds)          ((int)(((fds) >> 32u) & 0xffffffffu))
#define FD_JOIN(wfd, rfd) ((uint64_t)(wfd) + ((uint64_t)(rfd) << 32u))

//  util_pollable implementation details are private.  Only here for inlining.
// We have joined the write and read file descriptors into a single
// atomic 64, so we can update them together (and we can use cas to be sure
// that such updates are always safe.)
struct util_pollable
{
    util_atomic_u64 p_fds;
    util_atomic_bool p_raised;
};

inline void util_pollable_init(util_pollable *p)
{
    util_atomic_init_bool(&p->p_raised);
    util_atomic_set64(&p->p_fds, (uint64_t)-1);
}

inline void util_pollable_fini(util_pollable *p)
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

inline void util_pollable_raise(util_pollable *p)
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

inline void util_pollable_clear(util_pollable *p)
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

inline int util_pollable_getfd(util_pollable *p, int *fdp)
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
#endif
