#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>

#include "shm.h"

#define SHM_MAGIC_NUM          (0x95279527)
#define SHM_BLOCK_SIZE_INVALID (0)

/* aligning multiples of 8 */
#define SHM_SIZE(size) (((size) + 7) / 8 * 8)

#define MMAP_NAME "/posix_mmap"

typedef struct shm_block_info_t
{
    size_t offset;
    size_t block_size;     // aligned block size
    pthread_mutex_t mutex; //  process-shared
} shm_block_info_t;

typedef struct shm_header_t
{
    uint32_t magic_num; // see SHM_MAGIC_NUM
    uint32_t _pad;
    size_t refcount;    // total used shm modules count
    size_t block_count; // total shm block count
    size_t total_size;  // total aligned size
    pthread_mutex_t mutex;
    shm_block_info_t block[];
} shm_header_t;

/* share memory attach ptr */
static void *g_shm_ptr = MAP_FAILED;

static inline char *get_data_start(shm_header_t *hdr)
{
    size_t meta_size = sizeof(shm_block_info_t) * hdr->block_count;
    size_t header_with_meta = sizeof(shm_header_t) + meta_size;
    return (char *)hdr + SHM_SIZE(header_with_meta);
}

// Helper: init robust, process-shared mutex
static int init_mutex(pthread_mutex_t *mutex)
{
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    pthread_mutexattr_setrobust(&attr, PTHREAD_MUTEX_ROBUST);
    int ret = pthread_mutex_init(mutex, &attr);
    pthread_mutexattr_destroy(&attr);
    if (ret != 0)
    {
        printf("pthread_mutex_init fail, ret[%d], errno[%d](%s)\n", ret, errno, strerror(errno));
    }

    return ret;
}

// Recover dead-owner mutex
static int recover_mutex(pthread_mutex_t *mutex)
{
    int ret = pthread_mutex_consistent(mutex);
    if (ret != 0)
    {
        printf("pthread_mutex_consistent fail, ret[%d], errno[%d](%s)\n", ret, errno, strerror(errno));
        // 后续如果有其他进程再次尝试加锁收到ENOTRECOVERABLE错误码, 此时在这种情况下, 应该销毁并重新初始化互斥锁
    }

    return ret;
}

int shm_init_manager(const shm_block_t *blocks, size_t count)
{
    if (NULL == blocks || count == 0)
    {
        printf("invalid params!");
        return -1;
    }

    int ret = -1;
    // Total size needed:
    //   sizeof(shm_header_t) + N * sizeof(shm_block_info_t) + sum(aligned block sizes)
    size_t meta_size = sizeof(shm_block_info_t) * count;
    size_t data_offset = SHM_SIZE(sizeof(shm_header_t) + meta_size);
    size_t blocks_size = 0;
    for (size_t i = 0; i < count; i++)
    {
        blocks_size += SHM_SIZE(blocks[i].block_size);
    }
    size_t total_size = data_offset + blocks_size;

    /* convert to shm_header_t type */
    int fd = shm_open(MMAP_NAME, O_RDWR | O_CREAT | O_EXCL, 0666);
    int need_init = (fd != -1);
    if (fd == -1)
    {
        if (errno == EEXIST)
        {
            fd = shm_open(MMAP_NAME, O_RDWR, 0);
        }
        if (fd == -1)
        {
            perror("shm_open");
            return -1;
        }
    }

    if (need_init)
    {
        if (ftruncate(fd, (off_t)total_size) != 0)
        {
            perror("ftruncate");
            close(fd);
            shm_unlink(MMAP_NAME); // cleanup on failure
            return -1;
        }
    }

    void *addr = mmap(NULL, total_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (addr == MAP_FAILED)
    {
        perror("mmap");
        if (need_init)
        {
            shm_unlink(MMAP_NAME);
        }
        return -1;
    }

    g_shm_ptr = addr;

    if (need_init)
    {
        do
        {
            shm_header_t *hdr = (shm_header_t *)addr;
            memset(hdr, 0, total_size);
            hdr->magic_num = SHM_MAGIC_NUM;
            hdr->block_count = count;
            hdr->total_size = total_size;
            hdr->refcount++;

            if (init_mutex(&hdr->mutex) != 0)
            {
                ret = -1;
                break;
            }

            size_t offset = 0;
            char *data_start = get_data_start(hdr);
            for (size_t i = 0; i < hdr->block_count; i++)
            {
                hdr->block[i].offset = offset;
                hdr->block[i].block_size = SHM_SIZE(blocks[i].block_size);
                if (init_mutex(&hdr->block[i].mutex) != 0)
                {
                    ret = -1;
                    break;
                }
                memset(data_start + offset, 0, hdr->block[i].block_size);
                offset += hdr->block[i].block_size;
            }

            ret = 0;
        } while (0);

        if (0 != ret)
        {
            munmap(addr, total_size);
            g_shm_ptr = MAP_FAILED;
            shm_unlink(MMAP_NAME);
        }

        return ret;
    }

    // If not creator, just verify
    shm_header_t *hdr = (shm_header_t *)addr;
    if (hdr->magic_num != SHM_MAGIC_NUM)
    {
        printf("Invalid magic number!");
        munmap(addr, total_size);
        g_shm_ptr = MAP_FAILED;
        return -1;
    }

    // Increment refcount (optional, for debugging)
    int lock_err = pthread_mutex_lock(&hdr->mutex);
    if (lock_err == EOWNERDEAD)
    {
        if (recover_mutex(&hdr->mutex) != 0)
        {
            pthread_mutex_unlock(&hdr->mutex);
            return -1;
        }
    }
    else if (lock_err != 0)
    {
        printf("pthread_mutex_lock fail, ret[%d], errno[%d](%s)", lock_err, errno, strerror(errno));
        return -1;
    }
    hdr->refcount++;
    pthread_mutex_unlock(&hdr->mutex);

    return 0;
}

int shm_init_worker(void)
{
    int fd = shm_open(MMAP_NAME, O_RDWR, 0);
    if (fd == -1)
    {
        perror("shm_open (worker)");
        return -1;
    }

    struct stat sb;
    if (fstat(fd, &sb) == -1)
    {
        perror("fstat");
        close(fd);
        return -1;
    }

    void *addr = mmap(NULL, (size_t)sb.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (addr == MAP_FAILED)
    {
        perror("mmap (worker)");
        return -1;
    }

    shm_header_t *hdr = (shm_header_t *)addr;
    if (hdr->magic_num != SHM_MAGIC_NUM)
    {
        fprintf(stderr, "Worker: invalid magic number\n");
        munmap(addr, (size_t)sb.st_size);
        return -1;
    }

    g_shm_ptr = addr;
    return 0;
}

static inline shm_header_t *get_hdr(void)
{
    if (g_shm_ptr == MAP_FAILED)
    {
        return NULL;
    }
    return (shm_header_t *)g_shm_ptr;
}

int shm_lock(unsigned int block_id)
{
    shm_header_t *hdr = get_hdr();
    if (NULL == hdr || block_id >= hdr->block_count)
        return -1;

    int err = pthread_mutex_lock(&hdr->block[block_id].mutex);
    if (err == EOWNERDEAD)
    {
        return recover_mutex(&hdr->block[block_id].mutex) ? -1 : 0;
    }
    return (err == 0) ? 0 : -1;
}

int shm_unlock(uint32_t block_id)
{
    shm_header_t *hdr = get_hdr();
    if (NULL == hdr || block_id >= hdr->block_count)
    {
        return -1;
    }

    return pthread_mutex_unlock(&hdr->block[block_id].mutex) == 0 ? 0 : -1;
}

void *shm_get_block_addr(uint32_t block_id)
{
    shm_header_t *hdr = get_hdr();
    if (NULL == hdr || block_id >= hdr->block_count)
    {
        return NULL;
    }

    return get_data_start(hdr) + hdr->block[block_id].offset;
}

int shm_get_block_data(uint32_t block_id, size_t len, void *buf)
{
    if (NULL == buf || len == 0)
    {
        return -1;
    }

    if (shm_lock(block_id) != 0)
    {
        return -1;
    }

    void *addr = shm_get_block_addr(block_id);
    if (NULL == addr)
    {
        shm_unlock(block_id);
        return -1;
    }
    memcpy(buf, addr, len);
    shm_unlock(block_id);

    return 0;
}

int shm_set_block_data(uint32_t block_id, size_t len, const void *buf)
{
    if (NULL == buf || len == 0)
    {
        return -1;
    }

    if (shm_lock(block_id) != 0)
    {
        return -1;
    }

    void *addr = shm_get_block_addr(block_id);
    if (NULL == addr)
    {
        shm_unlock(block_id);
        return -1;
    }
    memcpy(addr, buf, len);
    shm_unlock(block_id);

    return 0;
}
