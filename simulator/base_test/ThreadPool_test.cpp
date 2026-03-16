#include <cstdio>
#include <unistd.h>
#include <sys/syscall.h>

#include "ThreadPool.hpp"

void print_task(int i)
{
    printf("thread[%ld]: task[%d]\n", (long)syscall(SYS_gettid), i);
    sleep(1);
}

int main()
{
    ThreadPool tp(1, 4);
    tp.start();

    int i = 0;
    for (; i < 10; ++i)
    {
        tp.commit(print_task, i);
    }

    tp.wait();

    for (; i < 20; ++i)
    {
        tp.commit(print_task, i);
    }

    tp.wait();

    return 0;
}
