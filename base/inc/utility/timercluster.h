#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>
#include <stdint.h>

#define TIMERCLUSTER_FOREACH_ERR(X)            \
    X(0, SUCCESS, "Success")                   \
    X(-1, ERR_ARG, "Invalid argument")         \
    X(-2, ERR_MEM, "Memory allocation failed") \
    X(-3, ERR_AGAIN, "again operation")        \
    X(-16, ERR_OTHER, "Other error")

enum
{
#define X(code, name, msg) TIMERCLUSTER_RET_##name = (code),
    TIMERCLUSTER_FOREACH_ERR(X)
#undef X
};

/**
 * @brief Timer ID type
 */
typedef uint32_t timerid_t;

/**
 * @brief Timer callback function type
 * @param id Timer ID
 * @param userdata User data pointer
 * @warning Callback and userdata should keep valid forever
 */
typedef void (*timercallback_t)(timerid_t id, void *userdata);

/**
 * @brief Timer type enumeration
 */
typedef enum
{
    TIMER_TYPE_SINGLE = 1, /**< Single shot timer */
    TIMER_TYPE_CYCLE = 2   /**< Cycle timer */
} timertype_e;

typedef struct timercluster_t timercluster_t;
typedef struct timercluster_v2_t timercluster_v2_t;

/**
 * @brief Initialize timer cluster (using cond(CLOCK_MONOTONIC) + mutex)
 * @return Timer cluster handle or NULL on error
 */
extern timercluster_t *timercluster_init(void);

/**
 * @brief Destroy timer cluster
 * @param cluster Timer cluster handle
 */
extern void timercluster_destroy(timercluster_t *cluster);

/**
 * @brief Check if timer exists
 * @param cluster Timer cluster handle
 * @param id Timer ID
 * @return true if timer exists, false otherwise
 */
extern bool timercluster_timer_exist(timercluster_t *cluster, timerid_t id);

/**
 * @brief Add timer to cluster
 * @param cluster Timer cluster handle
 * @param id Timer ID
 * @param type Timer type (single or cycle)
 * @param interval Timer interval in milliseconds
 * @param cb Timer callback function
 * @param userdata User data for callback
 * @return true on success, false on error
 */
extern bool timercluster_timer_add(timercluster_t *cluster, timerid_t id, timertype_e type, uint32_t interval, timercallback_t cb, void *userdata);

/**
 * @brief Delete timer from cluster
 * @param cluster Timer cluster handle
 * @param id Timer ID
 * @return true on success, false on error
 */
extern bool timercluster_timer_del(timercluster_t *cluster, timerid_t id);

/**
 * @brief Reset timer with new interval
 * @param cluster Timer cluster handle
 * @param id Timer ID
 * @param new_interval New timer interval in milliseconds
 * @return true on success, false on error
 */
extern bool timercluster_timer_reset(timercluster_t *cluster, timerid_t id, uint32_t new_interval);

/**
 * @brief Get error string for error code
 * @param err Error code
 * @return Error message string
 */
extern const char *timercluster_strerror(int err);

/**
 * @brief Initialize timer cluster V2 (using timerfd + epoll)
 * @return Timer cluster V2 handle or NULL on error
 */
extern timercluster_v2_t *timercluster_v2_init(void);

/**
 * @brief Destroy timer cluster V2
 * @param cluster Timer cluster V2 handle
 */
extern void timercluster_v2_destroy(timercluster_v2_t *cluster);

/**
 * @brief Check if timer exists in V2 cluster
 * @param cluster Timer cluster V2 handle
 * @param id Timer ID
 * @return true if timer exists, false otherwise
 */
extern bool timercluster_v2_timer_exist(timercluster_v2_t *cluster, timerid_t id);

/**
 * @brief Add timer to V2 cluster
 * @param cluster Timer cluster V2 handle
 * @param id Timer ID
 * @param type Timer type (single or cycle)
 * @param interval Timer interval in milliseconds
 * @param cb Timer callback function
 * @param userdata User data for callback
 * @return true on success, false on error
 */
extern bool timercluster_v2_timer_add(timercluster_v2_t *cluster, timerid_t id, timertype_e type, uint32_t interval, timercallback_t cb, void *userdata);

/**
 * @brief Delete timer from V2 cluster
 * @param cluster Timer cluster V2 handle
 * @param id Timer ID
 * @return true on success, false on error
 */
extern bool timercluster_v2_timer_del(timercluster_v2_t *cluster, timerid_t id);

/**
 * @brief Reset timer with new interval in V2 cluster
 * @param cluster Timer cluster V2 handle
 * @param id Timer ID
 * @param new_interval New timer interval in milliseconds
 * @return true on success, false on error
 */
extern bool timercluster_v2_timer_reset(timercluster_v2_t *cluster, timerid_t id, uint32_t new_interval);

/**
 * @brief Get error string for V2 error code
 * @param err Error code
 * @return Error message string
 */
extern const char *timercluster_v2_strerror(int err);

#ifdef __cplusplus
}
#endif
