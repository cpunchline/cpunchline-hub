#include "nng/nng.h"

#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <csignal>
#include <sys/wait.h>
#include <cstring>

using namespace std;
using namespace std::chrono;

// 利用NNG pair模式, 实现异步通信.
// adapter 端, 同步发送数据, 单开一个线程 进行数据的轮询接收

static bool exit_flag = false;

static void sig_handler(int sig)
{
    exit_flag = true;
    std::cout << "sig_handler " << sig << exit_flag << endl;
}

class Adapter
{
public:
    // 初始化
    bool init()
    {
        // 打开
        int ret = nng_pair0_open(&sock);
        if (ret != 0)
        {
            printf("nng_pair0_open fail, ret[%d]\n", ret);
        }

        // 设置缓冲区大小
        nng_socket_set_int(sock, NNG_OPT_SENDBUF, 2048);
        nng_socket_set_int(sock, NNG_OPT_RECVBUF, 2048);

        ret = nng_dial(sock, url.c_str(), &dialer, 0);
        if (ret != 0)
        {
            printf("nng_dial fail, ret[%d]\n", ret);
        }

        isInit = true;
        return isInit;
    }

    // 开始接收
    void start()
    {
        if (!isInit)
            return;
        std::thread t([&]()
                      {
                          while (!isStop)
                          {
                              nng_msg *msg = NULL;
                              char *json_str = NULL;
                              nng_recvmsg(sock, &msg, 0);
                              json_str = static_cast<char *>(nng_msg_body(msg));
                              std::cout << "nng_recvmsg " << json_str << std::endl;
                          }
                      });
        t.detach();
    }

    void stop()
    {
        isStop = true;
        cout << "stop " << isStop << endl;
    }

    void send(const std::string &msgStr)
    {
        if (!isInit)
            return;

        nng_msg *msg = NULL;
        nng_msg_alloc(&msg, sizeof(msgStr));
        memcpy(nng_msg_body(msg), msgStr.c_str(), sizeof(msgStr));

        nng_sendmsg(sock, msg, 0);
    }

public:
    nng_socket sock;
    nng_dialer dialer;
    std::atomic<bool> isStop{false};

private:
    std::string url{"ipc:///tmp/pair"};
    bool isInit{false};
};

int main()
{
    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);
    signal(SIGABRT, sig_handler);

    Adapter adapter;
    if (adapter.init())
    {
        cout << "init success" << endl;
    }
    else
    {
        cout << "init failed" << endl;
    }
    adapter.start();

    while (!exit_flag)
    {
        adapter.send("How are you?");
        this_thread::sleep_for(seconds(1));
    }
    adapter.stop();
    return 0;
}
