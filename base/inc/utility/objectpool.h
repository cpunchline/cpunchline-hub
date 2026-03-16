#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <pthread.h>
#include <assert.h>

// Sentinel value for end of embedded free list.
#define K_INVALID_INDEX UINT32_MAX

// Alignment macro (compatible with C11 and older compilers)
#ifndef ALIGNOF
#define ALIGNOF(t) _Alignof(t)
#endif
#ifndef ALIGNAS
#define ALIGNAS(t) _Alignas(t)
#endif

// ---------------------------------------------------------------------------
// Internal Core Structure (Opaque to user, used by macros)
// ---------------------------------------------------------------------------

typedef struct FixedMemPoolInternal
{
    uint8_t *storage; // Pointer to the static array provided by caller
    size_t aligned_block_size;
    uint32_t max_blocks;
    uint32_t free_head;
    uint32_t used_count;
    pthread_mutex_t mutex;
} FixedMemPoolInternal;

// Initialize the internal pool structure
// storage: Pre-allocated static array
// block_size: Size of user data (must be >= sizeof(uint32_t))
// max_blocks: Number of blocks
void FixedPool_InitInternal(FixedMemPoolInternal *pool, void *storage, size_t block_size, uint32_t max_blocks);

// Allocate a block (returns raw void*)
void *FixedPool_Allocate(FixedMemPoolInternal *pool);

// Free a block
void FixedPool_Free(FixedMemPoolInternal *pool, void *ptr);

// Check ownership
bool FixedPool_OwnsPointer(const FixedMemPoolInternal *pool, const void *ptr);

// Stats
uint32_t FixedPool_GetCapacity(const FixedMemPoolInternal *pool);
uint32_t FixedPool_GetFreeCount(const FixedMemPoolInternal *pool);
uint32_t FixedPool_GetUsedCount(const FixedMemPoolInternal *pool);

// Debug
void FixedPool_DumpState(const FixedMemPoolInternal *pool, const char *label);

// ---------------------------------------------------------------------------
// Macro Magic to Generate Type-Safe Pools
// ---------------------------------------------------------------------------

/**
 * USAGE EXAMPLE:
 * 
 * 1. Define the pool type for a specific struct (e.g., MyStruct) with capacity 100:
 *    DEFINE_FIXED_OBJECT_POOL(MyStruct, 100);
 * 
 * 2. In your .c file (or before usage), instantiate the storage:
 *    IMPLEMENT_FIXED_OBJECT_POOL(MyStruct, 100);
 * 
 * 3. Usage:
 *    MyStructPool *pool = MyStructPool_GetInstance();
 *    MyStruct *obj = MyStructPool_Create(pool, arg1, arg2); // Simulates Constructor
 *    if (obj) {
 *        // use obj
 *        MyStructPool_Destroy(pool, obj); // Simulates Destructor + Free
 *    }
 */

// Helper to calculate aligned size
#define CALC_ALIGNED_SIZE(size) (((size) + ALIGNOF(max_align_t) - 1) & ~(ALIGNOF(max_align_t) - 1))

// Define the Pool Struct Type
#define DEFINE_FIXED_OBJECT_POOL(TypeName, MaxCount)                                                                                         \
    typedef struct TypeName##Pool                                                                                                            \
    {                                                                                                                                        \
        FixedMemPoolInternal internal;                                                                                                       \
        uint8_t _storage[CALC_ALIGNED_SIZE(sizeof(TypeName) > sizeof(uint32_t) ? sizeof(TypeName) : sizeof(uint32_t)) * (MaxCount)];         \
    } TypeName##Pool;                                                                                                                        \
                                                                                                                                             \
    /* Getter for singleton instance if needed, or user allocates this struct */                                                             \
    static inline void TypeName##Pool_Init(TypeName##Pool *p)                                                                                \
    {                                                                                                                                        \
        _Static_assert(sizeof(TypeName) >= sizeof(uint32_t) || sizeof(uint32_t) >= sizeof(uint32_t), "Block size must be at least 4 bytes"); \
        size_t b_size = sizeof(TypeName) > sizeof(uint32_t) ? sizeof(TypeName) : sizeof(uint32_t);                                           \
        FixedPool_InitInternal(&p->internal, p->_storage, b_size, MaxCount);                                                                 \
    }                                                                                                                                        \
                                                                                                                                             \
    static inline TypeName *TypeName##Pool_Create(TypeName##Pool *p)                                                                         \
    {                                                                                                                                        \
        void *mem = FixedPool_Allocate(&p->internal);                                                                                        \
        if (!mem)                                                                                                                            \
            return NULL;                                                                                                                     \
        /* Zero initialize memory (C doesn't have constructors automatically) */                                                             \
        memset(mem, 0, sizeof(TypeName));                                                                                                    \
        return (TypeName *)mem;                                                                                                              \
    }                                                                                                                                        \
                                                                                                                                             \
    /* If the type has a specific destructor function, pass it, otherwise just free */                                                       \
    static inline void TypeName##Pool_Destroy(TypeName##Pool *p, TypeName *obj)                                                              \
    {                                                                                                                                        \
        if (!obj)                                                                                                                            \
            return;                                                                                                                          \
        /* User can manually call a destructor here if needed before freeing */                                                              \
        /* e.g., if (obj->destroy) obj->destroy(obj); */                                                                                     \
        FixedPool_Free(&p->internal, (void *)obj);                                                                                           \
    }                                                                                                                                        \
                                                                                                                                             \
    static inline uint32_t TypeName##Pool_Capacity(const TypeName##Pool *p)                                                                  \
    {                                                                                                                                        \
        return FixedPool_GetCapacity(&p->internal);                                                                                          \
    }                                                                                                                                        \
    static inline uint32_t TypeName##Pool_FreeCount(const TypeName##Pool *p)                                                                 \
    {                                                                                                                                        \
        return FixedPool_GetFreeCount(&p->internal);                                                                                         \
    }                                                                                                                                        \
    static inline uint32_t TypeName##Pool_UsedCount(const TypeName##Pool *p)                                                                 \
    {                                                                                                                                        \
        return FixedPool_GetUsedCount(&p->internal);                                                                                         \
    }                                                                                                                                        \
    static inline bool TypeName##Pool_Owns(const TypeName##Pool *p, const void *ptr)                                                         \
    {                                                                                                                                        \
        return FixedPool_OwnsPointer(&p->internal, ptr);                                                                                     \
    }                                                                                                                                        \
    static inline void TypeName##Pool_Dump(const TypeName##Pool *p)                                                                          \
    {                                                                                                                                        \
        FixedPool_DumpState(&p->internal, #TypeName "Pool");                                                                                 \
    }

// For cases where you just want a raw memory pool without type wrapping
#define DEFINE_FIXED_MEM_POOL(Name, BlockSize, MaxCount)                        \
    typedef struct Name                                                         \
    {                                                                           \
        FixedMemPoolInternal internal;                                          \
        uint8_t _storage[CALC_ALIGNED_SIZE(BlockSize) * (MaxCount)];            \
    } Name;                                                                     \
                                                                                \
    static inline void Name##_Init(Name *p)                                     \
    {                                                                           \
        FixedPool_InitInternal(&p->internal, p->_storage, BlockSize, MaxCount); \
    }                                                                           \
    static inline void *Name##_Allocate(Name *p)                                \
    {                                                                           \
        return FixedPool_Allocate(&p->internal);                                \
    }                                                                           \
    static inline void Name##_Free(Name *p, void *ptr)                          \
    {                                                                           \
        FixedPool_Free(&p->internal, ptr);                                      \
    }                                                                           \
    static inline uint32_t Name##_FreeCount(const Name *p)                      \
    {                                                                           \
        return FixedPool_GetFreeCount(&p->internal);                            \
    }

#ifdef __cplusplus
}
#endif