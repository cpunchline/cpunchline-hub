#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#define THREADQUEUE_FOREACH_ERR(X)             \
    X(0, SUCCESS, "Success")                   \
    X(-1, ERR_ARG, "Invalid argument")         \
    X(-2, ERR_MEM, "Memory allocation failed") \
    X(-3, ERR_LEN, "Invalid message length")   \
    X(-4, ERR_CONFLICT, "Resource conflict")   \
    X(-5, ERR_NO_MSG, "No message available")  \
    X(-6, ERR_FULL, "Queue is full")           \
    X(-7, ERR_EMPTY, "Queue is empty")         \
    X(-8, ERR_TIMEOUT, "Operation timeout")    \
    X(-9, ERR_STOP, "Queue has been stopped")  \
    X(-16, ERR_OTHER, "Other error")

enum
{
#define X(code, name, msg) THREADQUEUE_RET_##name = (code),
    THREADQUEUE_FOREACH_ERR(X)
#undef X
};

typedef struct threadboundedqueue_t threadboundedqueue_t;
typedef struct threadunboundedqueue_t threadunboundedqueue_t;

/**
 * @brief Create a bounded queue
 * @param[in] node_count Number of queue nodes
 * @param[in] node_size Size of each node
 * @return threadboundedqueue_t pointer on success, NULL on failure
 */
extern threadboundedqueue_t *threadboundedqueue_create(size_t node_count, size_t node_size);

/**
 * @brief Block push data to a queue
 * @param[in] q threadboundedqueue_t pointer
 * @param[in] data Data pointer to push to queue
 * @param[in] len Data buffer length
 * @param[in] timeout Timeout in milliseconds, wait time if queue is full
 * @return int32_t Error code
 */
extern int32_t threadboundedqueue_push_block(threadboundedqueue_t *q, void *data, size_t len, uint32_t timeout);

/**
 * @brief Non-block push data to a queue
 * @param[in] q threadboundedqueue_t pointer
 * @param[in] data Data pointer to push to queue
 * @param[in] len Data buffer length
 * @return int32_t Error code
 */
extern int32_t threadboundedqueue_push_nonblock(threadboundedqueue_t *q, void *data, size_t len);

/**
 * @brief Block pop data from a queue
 * @param[in] q threadboundedqueue_t pointer
 * @param[in] data Data buffer that receives data from queue
 * @param[in,out] len In: data buffer max length, Out: actual data length
 * @param[in] timeout Timeout in milliseconds, wait time if queue is empty
 * @return int32_t Error code
 */
extern int32_t threadboundedqueue_pop_block(threadboundedqueue_t *q, void *data, size_t *len, uint32_t timeout);

/**
 * @brief Non-block pop data from a queue
 * @param[in] q threadboundedqueue_t pointer
 * @param[in] data Data buffer that receives data from queue
 * @param[in,out] len In: data buffer max length, Out: actual data length
 * @return int32_t Error code
 */
extern int32_t threadboundedqueue_pop_nonblock(threadboundedqueue_t *q, void *data, size_t *len);

/**
 * @brief Check if queue is full
 * @param[in] q threadboundedqueue_t pointer
 * @return bool true if queue is full, false otherwise
 */
extern bool threadboundedqueue_is_full(threadboundedqueue_t *q);

/**
 * @brief Check if queue is empty
 * @param[in] q threadboundedqueue_t pointer
 * @return bool true if queue is empty, false otherwise
 */
extern bool threadboundedqueue_is_empty(threadboundedqueue_t *q);

/**
 * @brief Stop queue operations
 * @param[in] q threadboundedqueue_t pointer
 * @return bool true on success, false on error
 */
extern bool threadboundedqueue_stop(threadboundedqueue_t *q);

/**
 * @brief Uninitialize queue (stop queue before and ensure waiting threads are joined)
 * @param[in] q threadboundedqueue_t pointer
 * @return void
 */
extern void threadboundedqueue_uninit(threadboundedqueue_t **q);

/**
 * @brief Create an unbounded queue
 * @return threadunboundedqueue_t pointer on success, NULL on failure
 */
extern threadunboundedqueue_t *threadunboundedqueue_create(void);

/**
 * @brief Push data to a queue
 * @param[in] q threadunboundedqueue_t pointer
 * @param[in] data Data pointer to push to queue
 * @param[in] len Data buffer length
 * @return int32_t Error code
 */
extern int32_t threadunboundedqueue_push(threadunboundedqueue_t *q, void *data, size_t len);

/**
 * @brief Block pop data from a queue
 * @param[in] q threadunboundedqueue_t pointer
 * @param[in] data Data buffer that receives data from queue
 * @param[in,out] len In: data buffer max length, Out: actual data length
 * @param[in] timeout Timeout in milliseconds, wait time if queue is empty
 * @return int32_t Error code
 */
extern int32_t threadunboundedqueue_pop_block(threadunboundedqueue_t *q, void *data, size_t *len, uint32_t timeout);

/**
 * @brief Non-block pop data from a queue
 * @param[in] q threadunboundedqueue_t pointer
 * @param[in] data Data buffer that receives data from queue
 * @param[in,out] len In: data buffer max length, Out: actual data length
 * @return int32_t Error code
 */
extern int32_t threadunboundedqueue_pop_nonblock(threadunboundedqueue_t *q, void *data, size_t *len);

/**
 * @brief Check if queue is empty
 * @param[in] q threadunboundedqueue_t pointer
 * @return bool true if queue is empty, false otherwise
 */
extern bool threadunboundedqueue_is_empty(threadunboundedqueue_t *q);

/**
 * @brief Stop queue operations
 * @param[in] q threadunboundedqueue_t pointer
 * @return bool true on success, false on error
 */
extern bool threadunboundedqueue_stop(threadunboundedqueue_t *q);

/**
 * @brief Uninitialize queue (stop queue before and ensure waiting threads are joined)
 * @param[in] q threadunboundedqueue_t pointer
 * @return void
 */
extern void threadunboundedqueue_uninit(threadunboundedqueue_t **q);

/**
 * @brief Convert thread queue error number to error string
 * @param[in] err Error number
 * @return const char pointer to error string
 */
extern const char *threadqueue_strerror(int err);

#ifdef __cplusplus
}
#endif
