#pragma once

#include <list>
#include <memory>
#include <mutex>
#include <condition_variable>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

/// Sentinel value for end of embedded free list.
static constexpr uint32_t kInvalidIndex = UINT32_MAX;

// ---------------------------------------------------------------------------
// FixedMemPool<BlockSize, MaxBlocks>
//
// Compile-time sized memory pool using an embedded free list.
// Each free block stores the index of the next free block in its first
// sizeof(uint32_t) bytes, providing O(1) allocation and deallocation.
//
// Thread-safe via std::mutex. Zero heap allocation -- all storage is inline.
//
// @tparam BlockSize  Size of each block in bytes (>= sizeof(uint32_t))
// @tparam MaxBlocks  Maximum number of blocks in the pool
// ---------------------------------------------------------------------------
template <uint32_t BlockSize, uint32_t MaxBlocks>
class FixedMemPool
{
    static_assert(BlockSize >= sizeof(uint32_t), "BlockSize must be >= sizeof(uint32_t)");
    static_assert(MaxBlocks > 0, "MaxBlocks must be > 0");
    static_assert(MaxBlocks < kInvalidIndex, "MaxBlocks must be < UINT32_MAX");

public:
    FixedMemPool() :
        free_head_(0), used_count_(0)
    {
        // Build the embedded free list: block[i].next = i + 1
        for (uint32_t i = 0; i < MaxBlocks - 1; ++i)
        {
            StoreIndex(i, i + 1);
        }
        StoreIndex(MaxBlocks - 1, kInvalidIndex);
    }

    ~FixedMemPool() = default;

    FixedMemPool(const FixedMemPool &) = delete;
    FixedMemPool &operator=(const FixedMemPool &) = delete;
    FixedMemPool(FixedMemPool &&) = delete;
    FixedMemPool &operator=(FixedMemPool &&) = delete;

    /// Allocate a block from the pool.
    /// @return Pointer to the allocated block, or nullptr if the pool is full.
    void *Allocate()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (free_head_ == kInvalidIndex)
        {
            return nullptr;
        }
        uint32_t idx = free_head_;
        free_head_ = LoadIndex(idx);
        ++used_count_;
        return BlockPtr(idx);
    }

    /// Free a previously allocated block.
    /// The caller must ensure ptr was returned by Allocate() on this pool.
    void Free(void *ptr)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        uint32_t idx = PtrToIndex(ptr);
        StoreIndex(idx, free_head_);
        free_head_ = idx;
        --used_count_;
    }

    /// Check if a pointer belongs to this pool's address range and is block-aligned.
    /// Returns true for both allocated and free blocks within the pool.
    bool OwnsPointer(const void *ptr) const
    {
        auto addr = reinterpret_cast<uintptr_t>(ptr);
        auto base = reinterpret_cast<uintptr_t>(storage_);
        if (addr < base || addr >= base + sizeof(storage_))
        {
            return false;
        }
        return (addr - base) % kAlignedBlockSize == 0;
    }

    /// Number of free blocks available.
    uint32_t FreeCount() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return MaxBlocks - used_count_;
    }

    /// Number of currently allocated blocks.
    uint32_t UsedCount() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return used_count_;
    }

    /// Total pool capacity (compile-time constant).
    static constexpr uint32_t Capacity()
    {
        return MaxBlocks;
    }

    /// User-specified block size.
    static constexpr uint32_t BlockSizeValue()
    {
        return BlockSize;
    }

    /// Actual stride between blocks (aligned).
    static constexpr size_t AlignedBlockSize()
    {
        return kAlignedBlockSize;
    }

    /// Print pool state to stdout for debugging.
    void DumpState(const char *label = "FixedMemPool") const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        std::printf("[%s] capacity=%u used=%u free=%u block_size=%u aligned_size=%zu\n", label, MaxBlocks, used_count_,
                    MaxBlocks - used_count_, BlockSize, kAlignedBlockSize);
    }

private:
    // Round up BlockSize to the nearest multiple of alignof(max_align_t).
    static constexpr size_t kAlignedBlockSize =
        (BlockSize + alignof(std::max_align_t) - 1) & ~(alignof(std::max_align_t) - 1);

    // Inline storage -- zero heap allocation.
    alignas(std::max_align_t) uint8_t storage_[kAlignedBlockSize * MaxBlocks];

    mutable std::mutex mutex_;
    uint32_t free_head_;
    uint32_t used_count_;

    void *BlockPtr(uint32_t idx)
    {
        return &storage_[idx * kAlignedBlockSize];
    }

    uint32_t PtrToIndex(const void *ptr) const
    {
        auto offset = static_cast<size_t>(static_cast<const uint8_t *>(ptr) - storage_);
        return static_cast<uint32_t>(offset / kAlignedBlockSize);
    }

    void StoreIndex(uint32_t block_idx, uint32_t next_idx)
    {
        std::memcpy(&storage_[block_idx * kAlignedBlockSize], &next_idx, sizeof(uint32_t));
    }

    uint32_t LoadIndex(uint32_t block_idx) const
    {
        uint32_t idx;
        std::memcpy(&idx, &storage_[block_idx * kAlignedBlockSize], sizeof(uint32_t));
        return idx;
    }
};

// ---------------------------------------------------------------------------
// FixedObjectPool<T, MaxObjects>
//
// Type-safe memory pool for objects of type T.
// Uses placement new for construction and explicit destructor calls.
// Safe with -fno-exceptions (placement new never throws).
//
// @tparam T           Object type to pool
// @tparam MaxObjects  Maximum number of objects
// ---------------------------------------------------------------------------
template <typename T, uint32_t MaxObjects>
class FixedObjectPool
{
    static constexpr uint32_t kBlockSize =
        static_cast<uint32_t>(sizeof(T) > sizeof(uint32_t) ? sizeof(T) : sizeof(uint32_t));

public:
    FixedObjectPool() = default;

    FixedObjectPool(const FixedObjectPool &) = delete;
    FixedObjectPool &operator=(const FixedObjectPool &) = delete;

    /// Construct an object in the pool using placement new.
    /// @return Pointer to the constructed object, or nullptr if pool is full.
    template <typename... Args>
    T *Create(Args &&...args)
    {
        void *mem = pool_.Allocate();
        if (!mem)
        {
            return nullptr;
        }
        return ::new (mem) T(std::forward<Args>(args)...);
    }

    /// Destroy an object: call destructor and return memory to pool.
    /// No-op if obj is nullptr.
    void Destroy(T *obj)
    {
        if (!obj)
            return;
        obj->~T();
        pool_.Free(obj);
    }

    /// Check if a pointer belongs to this pool.
    bool OwnsPointer(const T *obj) const
    {
        return pool_.OwnsPointer(obj);
    }

    uint32_t FreeCount() const
    {
        return pool_.FreeCount();
    }
    uint32_t UsedCount() const
    {
        return pool_.UsedCount();
    }
    static constexpr uint32_t Capacity()
    {
        return MaxObjects;
    }

    void DumpState(const char *label = "FixedObjectPool") const
    {
        pool_.DumpState(label);
    }

private:
    FixedMemPool<kBlockSize, MaxObjects> pool_;
};

// 对象池模式(Object Pool Pattern)
// 高效管理可重用的对象集合
// 创建型设计模式, 它通过存储一系列已经初始化的对象来避免频繁创建和销毁对象所带来的性能开销;
// 特别适用于对象创建成本高或频繁请求相同类型对象的场景

#define DEFAULT_OBJECT_POOL_INIT_NUM 0    // 对象池初始时预创建对象的数量
#define DEFAULT_OBJECT_POOL_MAX_NUM  4    // 对象池允许的最大对象数量, 默认为4
#define DEFAULT_OBJECT_POOL_TIMEOUT  3000 //  尝试从池中借用对象时的超时时间 ms

// 这是一个通用工厂类模板, 用于生成特定类型的对象.
// 默认实现是通过new操作符创建类型T的对象, 但这个工厂类可以被用户自定义以实现更复杂的对象创建逻辑.
template <class T>
class ObjectFactory
{
public:
    static T *create()
    {
        return new T;
    }
};

template <class T, class TFactory = ObjectFactory<T>>
class ObjectPool
{
public:
    ObjectPool(
        size_t init_num = DEFAULT_OBJECT_POOL_INIT_NUM,
        size_t max_num = DEFAULT_OBJECT_POOL_MAX_NUM,
        size_t timeout = DEFAULT_OBJECT_POOL_TIMEOUT) :
        _max_num(max_num), _timeout(timeout)
    {
        for (size_t i = 0; i < init_num; ++i)
        {
            T *p = TFactory::create();
            if (p)
            {
                objects_.push_back(std::shared_ptr<T>(p));
            }
        }
        _object_num = objects_.size();
    }

    ~ObjectPool()
    {
    }

    size_t ObjectNum() // 对象总数
    {
        return _object_num;
    }

    size_t IdleNum() // 空闲对象数
    {
        return objects_.size();
    }

    size_t BorrowNum() // 已借出对象数
    {
        return ObjectNum() - IdleNum();
    }

    std::shared_ptr<T> TryBorrow() // 尝试无等待地从池中借用一个对象
    {
        std::shared_ptr<T> pObj = NULL;
        std::lock_guard<std::mutex> locker(mutex_);
        if (!objects_.empty())
        {
            pObj = objects_.front();
            objects_.pop_front();
        }
        return pObj;
    }

    std::shared_ptr<T> Borrow() // 在没有空闲对象时, 根据配置可能等待指定时间尝试获取对象, 或者在达到最大对象限制时返回NULL.
    {
        std::shared_ptr<T> pObj = TryBorrow();
        if (pObj)
        {
            return pObj;
        }

        std::unique_lock<std::mutex> locker(mutex_);
        if (_object_num < _max_num)
        {
            ++_object_num;
            // NOTE: unlock to avoid TFactory::create block
            mutex_.unlock();
            T *p = TFactory::create();
            mutex_.lock();
            if (!p)
            {
                --_object_num;
            }
            return std::shared_ptr<T>(p);
        }

        if (_timeout > 0)
        {
            std::cv_status status = cond_.wait_for(locker, std::chrono::milliseconds(_timeout));
            if (status == std::cv_status::timeout)
            {
                return NULL;
            }
            if (!objects_.empty())
            {
                pObj = objects_.front();
                objects_.pop_front();
                return pObj;
            }
            else
            {
                // WARN: No idle object
            }
        }
        return pObj;
    }

    void Return(std::shared_ptr<T> &pObj) // 归还对象到池中
    {
        if (!pObj)
        {
            return;
        }
        std::lock_guard<std::mutex> locker(mutex_);
        objects_.push_back(pObj);
        cond_.notify_one();
    }

    bool Add(std::shared_ptr<T> &pObj) // 增加对象到对象池
    {
        std::lock_guard<std::mutex> locker(mutex_);
        if (_object_num >= _max_num)
        {
            return false;
        }
        objects_.push_back(pObj);
        ++_object_num;
        cond_.notify_one();
        return true;
    }

    bool Remove(std::shared_ptr<T> &pObj) // 从对象池删除对象
    {
        std::lock_guard<std::mutex> locker(mutex_);
        auto iter = objects_.begin();
        while (iter != objects_.end())
        {
            if (*iter == pObj)
            {
                iter = objects_.erase(iter);
                --_object_num;
                return true;
            }
            else
            {
                ++iter;
            }
        }
        return false;
    }

    void Clear()
    {
        std::lock_guard<std::mutex> locker(mutex_);
        objects_.clear();
        _object_num = 0;
    }

    size_t _object_num;
    size_t _max_num;
    size_t _timeout;

private:
    std::list<std::shared_ptr<T>> objects_;
    std::mutex mutex_;
    std::condition_variable cond_;
};

// 智能指针包装类, 用于自动管理从对象池中借用和归还对象的过程.
// 通过RAII(Resource Acquisition Is Initialization)机制, 在对象生命周期结束时自动将对象归还给对象池, 简化了资源管理并减少了资源泄露的风险.
template <class T, class TFactory = ObjectFactory<T>>
class PoolObject
{
public:
    typedef ObjectPool<T, TFactory> PoolType;

    PoolObject(PoolType &pool) :
        pool_(pool)
    {
        sptr_ = pool_.Borrow();
    }

    ~PoolObject()
    {
        if (sptr_)
        {
            pool_.Return(sptr_);
        }
    }

    PoolObject(const PoolObject<T> &) = delete;
    PoolObject<T> &operator=(const PoolObject<T> &) = delete;

    T *get()
    {
        return sptr_.get();
    }

    operator bool()
    {
        return sptr_.get() != NULL;
    }

    T *operator->()
    {
        return sptr_.get();
    }

    T operator*()
    {
        return *sptr_.get();
    }

private:
    PoolType &pool_;
    std::shared_ptr<T> sptr_;
};
