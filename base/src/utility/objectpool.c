#include "utility/utils.h"
#include "utility/objectpool.h"

// Calculate alignment helper
static size_t get_aligned_size(size_t block_size)
{
    return (block_size + ALIGNOF(max_align_t) - 1) & ~(ALIGNOF(max_align_t) - 1);
}

void FixedPool_InitInternal(FixedMemPoolInternal *pool, void *storage, size_t block_size, uint32_t max_blocks)
{
    if (!pool || !storage || max_blocks == 0 || max_blocks >= K_INVALID_INDEX)
    {
        return;
    }

    // Validations (Runtime equivalent of static_assert)
    if (block_size < sizeof(uint32_t))
    {
        LOG_PRINT_ERROR("Error: BlockSize must be >= sizeof(uint32_t)");
        return;
    }

    pool->storage = (uint8_t *)storage;
    pool->aligned_block_size = get_aligned_size(block_size);
    pool->max_blocks = max_blocks;
    pool->used_count = 0;
    pool->free_head = 0;

    // Initialize Mutex
    pthread_mutexattr_t mutexattr = {};
    pthread_mutexattr_init(&mutexattr);
    (void)pthread_mutexattr_settype(&mutexattr, PTHREAD_MUTEX_ERRORCHECK);
    while ((pthread_mutex_init(&pool->mutex, &mutexattr) != 0) &&
           (pthread_mutex_init(&pool->mutex, NULL) != 0))
    {
        // We must have memory exhaustion -- ENOMEM, or
        // in some cases EAGAIN.  Wait a bit before we try to
        // give things a chance to settle down.
        util_msleep(10);
    }
    pthread_mutexattr_destroy(&mutexattr);

    // Build the embedded free list: block[i].next = i + 1
    for (uint32_t i = 0; i < max_blocks - 1; ++i)
    {
        // Store next index in the first 4 bytes of the block
        uint32_t next_idx = i + 1;
        memcpy(&pool->storage[i * pool->aligned_block_size], &next_idx, sizeof(uint32_t));
    }
    // Last block points to invalid
    uint32_t last_idx = K_INVALID_INDEX;
    memcpy(&pool->storage[(max_blocks - 1) * pool->aligned_block_size], &last_idx, sizeof(uint32_t));
}

void *FixedPool_Allocate(FixedMemPoolInternal *pool)
{
    if (!pool)
        return NULL;

    pthread_mutex_lock(&pool->mutex);

    if (pool->free_head == K_INVALID_INDEX)
    {
        pthread_mutex_unlock(&pool->mutex);
        return NULL; // Pool full
    }

    uint32_t idx = pool->free_head;

    // Load next free index from the current head block
    uint32_t next_idx;
    memcpy(&next_idx, &pool->storage[idx * pool->aligned_block_size], sizeof(uint32_t));

    pool->free_head = next_idx;
    pool->used_count++;

    pthread_mutex_unlock(&pool->mutex);

    return &pool->storage[idx * pool->aligned_block_size];
}

void FixedPool_Free(FixedMemPoolInternal *pool, void *ptr)
{
    if (!pool || !ptr)
        return;

    // Basic validation (optional, adds overhead)
    if (!FixedPool_OwnsPointer(pool, ptr))
    {
        LOG_PRINT_ERROR("Warning: Attempting to free pointer not owned by pool");
        return;
    }

    pthread_mutex_lock(&pool->mutex);

    // Calculate index
    size_t offset = (size_t)((uint8_t *)ptr - pool->storage);
    uint32_t idx = (uint32_t)(offset / pool->aligned_block_size);

    // Store current head into this block (making it the new head of free list)
    memcpy(&pool->storage[idx * pool->aligned_block_size], &pool->free_head, sizeof(uint32_t));

    // Update head to this block
    pool->free_head = idx;

    if (pool->used_count > 0)
    {
        pool->used_count--;
    }

    pthread_mutex_unlock(&pool->mutex);
}

bool FixedPool_OwnsPointer(const FixedMemPoolInternal *pool, const void *ptr)
{
    if (!pool || !ptr)
        return false;

    uintptr_t addr = (uintptr_t)ptr;
    uintptr_t base = (uintptr_t)pool->storage;
    size_t total_size = pool->aligned_block_size * pool->max_blocks;

    if (addr < base || addr >= base + total_size)
    {
        return false;
    }

    // Check alignment
    if ((addr - base) % pool->aligned_block_size != 0)
    {
        return false;
    }

    return true;
}

uint32_t FixedPool_GetCapacity(const FixedMemPoolInternal *pool)
{
    if (!pool)
        return 0;
    pthread_mutex_lock((pthread_mutex_t *)&pool->mutex);
    uint32_t count = pool->max_blocks;
    pthread_mutex_unlock((pthread_mutex_t *)&pool->mutex);
    return count;
}

uint32_t FixedPool_GetFreeCount(const FixedMemPoolInternal *pool)
{
    if (!pool)
        return 0;
    // Lock not strictly necessary for count read if approximate is ok,
    // but for accuracy we lock.
    pthread_mutex_lock((pthread_mutex_t *)&pool->mutex);
    uint32_t count = pool->max_blocks - pool->used_count;
    pthread_mutex_unlock((pthread_mutex_t *)&pool->mutex);
    return count;
}

uint32_t FixedPool_GetUsedCount(const FixedMemPoolInternal *pool)
{
    if (!pool)
        return 0;
    pthread_mutex_lock((pthread_mutex_t *)&pool->mutex);
    uint32_t count = pool->used_count;
    pthread_mutex_unlock((pthread_mutex_t *)&pool->mutex);
    return count;
}

void FixedPool_DumpState(const FixedMemPoolInternal *pool, const char *label)
{
    if (!pool)
        return;
    pthread_mutex_lock((pthread_mutex_t *)&pool->mutex);
    LOG_PRINT_INFO("[%s] capacity=%u used=%u free=%u block_size=%zu aligned_size=%zu\n",
                   label ? label : "Pool",
                   pool->max_blocks,
                   pool->used_count,
                   pool->max_blocks - pool->used_count,
                   pool->aligned_block_size - (pool->aligned_block_size % ALIGNOF(max_align_t)), // Approx original
                   pool->aligned_block_size);
    pthread_mutex_unlock((pthread_mutex_t *)&pool->mutex);
}
