#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <assert.h>
#include "utility/utils.h"
#include "shm_block.h"
#include "shm.h"

const shm_block_t shm_blocks[] = {
    {SHM_BLOCK_ID_TEST, sizeof(shm_test_block_t)},
};

int main(int argc, const char *argv[])
{
    if (argc < 2)
    {
        printf("usage: %s <test_string>\n", argv[0]);
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0)
    {
        perror("fork failed");
        return -1;
    }
    else if (pid == 0)
    {
        // 子进程 - 管理器,设置数据
        shm_test_block_t test_block = {};
        memcpy(test_block.test, argv[1], strlen(argv[1]));

        int ret = shm_init_manager(shm_blocks, UTIL_ARRAY_SIZE(shm_blocks));
        printf("[Child Process] shm_init_manager ret[%d]\n", ret);
        assert(ret == 0);

        printf("[Child Process] set [%s][%s]\n", argv[1], test_block.test);
        shm_set_block_data(SHM_BLOCK_ID_TEST, sizeof(shm_test_block_t), &test_block);

        printf("[Child Process] Data set successfully, exiting...\n");
        return 0;
    }
    else
    {
        // 等待子进程完成写入
        wait(NULL);

        // 父进程 - 工作者,获取数据
        int ret = shm_init_worker();
        printf("[Parent Process] shm_init_worker ret[%d]\n", ret);
        assert(ret == 0);

        shm_test_block_t test_block = {};
        memset(&test_block, 0x00, sizeof(test_block));
        shm_get_block_data(SHM_BLOCK_ID_TEST, sizeof(shm_test_block_t), &test_block);
        printf("[Parent Process] get [%s]\n", test_block.test);

        printf("[Parent Process] Test completed successfully!\n");
        return 0;
    }
}