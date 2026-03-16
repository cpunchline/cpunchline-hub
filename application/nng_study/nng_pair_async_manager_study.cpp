#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include "nng/nng.h"

#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <csignal>
#include <sys/wait.h>

using namespace std;
using namespace std::chrono;

// 利用NNG pair模式, 实现异步通信.
// manager端  绑定地址, 回调函数里 接收 异步消息

static bool exit_flag = false;

void recv_data_callback(void *arg);
static void sig_handler(int sig)
{
    exit_flag = true;
    std::cout << "sig_handler " << sig << exit_flag << endl;
}

void fatal(const char *func, nng_err rv)
{
    fprintf(stderr, "%s: %s\n", func, nng_strerror(rv));
    exit(1);
}

class Manager
{
public:
    // 初始化
    bool init()
    {
        // 创建io 并绑定回调函数
        rv = nng_aio_alloc(&aio, recv_data_callback, this);
        if (rv < 0)
        {
            fatal("cannot allocate aio", rv);
        }

        // 打开
        int ret = nng_pair0_open(&sock);
        if (ret != 0)
        {
            printf("nng_pair0_open fail, ret[%d]", ret);
        }

        // 设置缓冲区大小
        nng_socket_set_int(sock, NNG_OPT_SENDBUF, 2048);
        nng_socket_set_int(sock, NNG_OPT_RECVBUF, 2048);

        // 开始监听
        ret = nng_listen(sock, url.c_str(), NULL, 0);
        if (ret != NNG_OK)
        {
            printf("nng_listen fail, ret[%d]", ret);
        }
        nng_socket_recv(sock, aio);

        isInit = true;
        return isInit;
    }

    // 发送数据
    void send(const std::string &msgStr)
    {
        if (!isInit)
            return;

        if (!isInit)
            return;

        nng_msg *msg = NULL;
        nng_msg_alloc(&msg, sizeof(msgStr));
        memcpy(nng_msg_body(msg), msgStr.c_str(), sizeof(msgStr));

        nng_sendmsg(sock, msg, 0);
    }

public:
    nng_socket sock;
    nng_aio *aio{nullptr};

private:
    nng_err rv = NNG_OK;
    std::string url{"ipc:///tmp/pair"};
    bool isInit{false};
};

void recv_data_callback(void *arg)
{
    nng_err rv = NNG_OK;
    Manager *manager = static_cast<Manager *>(arg);
    nng_msg *msg = NULL;
    size_t json_len = 0;
    char *json_str = NULL;

    rv = nng_aio_result(manager->aio);
    if (0 != rv)
    {
        fatal("nng_recv error ", rv);
    }

    msg = nng_aio_get_msg(manager->aio);
    json_str = static_cast<char *>(nng_msg_body(msg));
    json_len = nng_msg_len(msg);

    std::cout << "recv_data_callback " << json_str << "json_len: " << json_len << std::endl;

    nng_msg_free(msg);
    nng_socket_recv(manager->sock, manager->aio);
}

int main()
{
    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);
    signal(SIGABRT, sig_handler);

    Manager manager;
    if (manager.init())
    {
        cout << "init success" << endl;
    }
    else
    {
        cout << "init failed" << endl;
    }

    while (!exit_flag)
    {
        manager.send("Not bad");
        this_thread::sleep_for(seconds(1));
    }
    return 0;
}
