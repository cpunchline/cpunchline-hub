#include <iostream>
#include <list>
#include <memory_resource>

// C++17 内存资源抽象层 - memory_resource
// 将内存的分配和回收与具体的数据结构解耦, 从而允许开发人员使用不同的内存管理策略, 提高内存管理的灵活性

/*
memory_resource 是一个抽象基类, 提供了三个纯虚函数, 需要被子类实现:
do_allocate(size_t bytes, size_t alignment);              // 根据指定的字节和对齐方式分配内存.
do_deallocate(void* p, size_t bytes, size_t alignment);   // 释放之前分配的内存.
do_is_equal(const memory_resource& other) const noexcept; // 判断两个内存资源是否等效
*/

std::pmr::pool_options options{20, sizeof(int)};
// 每个块中最多可以包含的块数量.用于限制内存池从上游内存资源一次性获取的内存量.如果设置为 0,内核会通过munge_options函数去修改pool_options.
// 内存池能够处理的最大块大小.
std::pmr::synchronized_pool_resource sync_pool(options); // 全局内存池(线程安全 mutex + unsynchronized_pool_resource)
// std::pmr::unsynchronized_pool_resource unsync_pool; // 全局内存池(线程不安全)

// new/delete < sync < unsync < monot

/*
内部维护了一系列的内存池, 每个内存池负责管理一定大小范围的内存块.
当需要分配内存时, 内存池资源会根据请求的大小选择一个适当的内存池, 并从中分配一个内存块.
如果对应的内存池没有可用的内存块, 那么内存池资源会从上游内存资源那里获取更多的内存;

unsynchronized_pool_resource 具有一些可配置的参数, 包括最大和最小块大小,最大块数等.
这些参数可以通过 std::pmr::pool_options 结构体来设置

pool_options 结构体包括以下几个参数:
max_blocks_per_chunk        :每个块中最多可以包含的块数量.这个参数用于限制内存池从上游内存资源一次性获取的内存量.如果设置为 0, 则表示没有限制.
largest_required_pool_block :要求内存池能够处理的最大块大小.这个参数用于确定内存池应该管理哪些大小的内存块.
*/

// static char g_buf[sizeof(int) * 20] = {0};

void test_new_delete()
{
    // new/delete
    std::vector<char> v;
}

void test_synchronized_pool_resource()
{
    // get_default_resource 默认为new_delete_resource
    // new_delete_resource  默认
    // null_memory_resource 始终抛出异常
    std::pmr::list<char> v1;
    std::pmr::list<char> v2{std::pmr::get_default_resource()};

    // 即使有多个线程同时执行本段代码,synchronized_pool_resource 也能保证每次 allocate() 和 deallocate() 操作的正确性
    std::pmr::synchronized_pool_resource pool(options);

    // 从 pool 中分配内存
    void *p = pool.allocate(100);

    // 将内存归还给 pool
    pool.deallocate(p, 100);
}

void test_monotonic_buffer_resource()
{
    // 局部内存池(缓冲区), 只分配不释放, 且无需维护链表结构
    std::pmr::monotonic_buffer_resource mem_resource{sizeof(int) * 20, &sync_pool}; // sizeof(int) * 20不够用, 则从全局申请
    // std::pmr::monotonic_buffer_resource mem_resource2{g_buf, sizeof(g_buf), &sync_pool};                       // g_buf不够用, 则从全局申请
    // std::pmr::monotonic_buffer_resource mem_resource3{g_buf, sizeof(g_buf), std::pmr::null_memory_resource()}; // g_buf不够用则抛出异常
    std::vector<char, std::pmr::polymorphic_allocator<char>> v1{std::pmr::polymorphic_allocator<char>{&mem_resource}};
    std::vector<char, std::pmr::polymorphic_allocator<char>> v2{&mem_resource};
    std::pmr::vector<char> v3{&mem_resource};
}

// 也可以自己写资源管理类继承 std::pmr::memory_resource

int main()
{
    return EXIT_SUCCESS;
}
