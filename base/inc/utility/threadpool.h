#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include <stdlib.h>

#define THREADPOOL_FOREACH_ERR(X)               \
    X(0, SUCCESS, "Success")                    \
    X(-1, ERR_ARG, "Invalid argument")          \
    X(-2, ERR_MEM, "Memory allocation failed")  \
    X(-3, ERR_AGAIN, "again operation")         \
    X(-6, ERR_QUEUE_FULL, "Task queue is full") \
    X(-16, ERR_OTHER, "Other error")

enum
{
#define X(code, name, msg) THREADPOOL_RET_##name = (code),
    THREADPOOL_FOREACH_ERR(X)
#undef X
};

typedef struct threadtask_t
{
    void (*function)(void *arg);
    void *arg;
} threadtask_t;
typedef struct fix_threadpool_t fix_threadpool_t;

/**
 * @brief Create a fixed-size thread pool
 * @param thread_capacity Number of worker threads
 * @param queue_size Maximum task queue size
 * @return Thread pool handle or NULL on error
 */
extern fix_threadpool_t *fix_threadpool_create(size_t thread_capacity, size_t queue_size);

/**
 * @brief Destroy fixed thread pool
 * @param pool Thread pool handle
 * @return Error code
 */
extern int32_t fix_threadpool_destroy(fix_threadpool_t *pool);

/**
 * @brief Add single task to fixed thread pool
 * @param pool Thread pool handle
 * @param function Task function
 * @param arg Task argument
 * @return Error code
 */
extern int32_t fix_threadpool_addtask(fix_threadpool_t *pool, void (*function)(void *), void *arg);

/**
 * @brief Add multiple tasks to fixed thread pool
 * @param pool Thread pool handle
 * @param tasks Array of tasks
 * @param tasks_size Number of tasks
 * @return Error code
 */
extern int32_t fix_threadpool_addtasks(fix_threadpool_t *pool, threadtask_t *tasks, size_t tasks_size);

/**
 * @brief Get error string for error code
 * @param err Error code
 * @return Error message string
 */
extern const char *fix_threadpool_strerror(int err);

#ifdef __cplusplus
}
#endif
