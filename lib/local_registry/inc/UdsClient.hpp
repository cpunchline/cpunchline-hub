#ifndef HV_UDS_CLIENT_HPP_
#define HV_UDS_CLIENT_HPP_

#include "hv/hsocket.h"
#include "hv/hssl.h"
#include "hv/hlog.h"

#include "hv/EventLoopThread.h"
#include "hv/Channel.h"

namespace hv
{

template <class UdsSocketChannel = SocketChannel>
class UdsClientEventLoopTmpl
{
public:
    typedef std::shared_ptr<UdsSocketChannel> UdsSocketChannelPtr;

    UdsClientEventLoopTmpl(EventLoopPtr loop = NULL)
    {
        loop_ = loop ? loop : std::make_shared<EventLoop>();
        connect_timeout = HIO_DEFAULT_CONNECT_TIMEOUT;
        reconn_setting = NULL;
        unpack_setting = NULL;
    }

    virtual ~UdsClientEventLoopTmpl()
    {
        HV_FREE(reconn_setting);
        HV_FREE(unpack_setting);
    }

    const EventLoopPtr &loop()
    {
        return loop_;
    }

    // delete thread-safe
    void deleteInLoop()
    {
        loop_->runInLoop([this]()
                         {
                             delete this;
                         });
    }

    // NOTE: By default, not bind local name. If necessary, you can call bind() after createsocket().
    // @retval >=0 connfd, <0 error
    int createsocket(const char *name = "UdsServer.ipc")
    {
        memset(&remote_addr, 0, sizeof(remote_addr));
        int ret = sockaddr_set_ipport(&remote_addr, (char *)name, -1);
        if (ret != 0)
        {
            return NABS(ret);
        }
        this->remote_name = name;
        return createsocket(&remote_addr.sa);
    }

    int createsocket(struct sockaddr *addr)
    {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
        int connfd = ::socket(addr->sa_family, SOCK_STREAM, 0);
        // SOCKADDR_PRINT(addr);
        if (connfd < 0)
        {
            perror("socket");
            return -2;
        }

        hio_t *io = hio_get(loop_->loop(), connfd);
        assert(io != NULL);
        hio_set_peeraddr(io, addr, (int)SOCKADDR_LEN(addr));
        channel = std::make_shared<UdsSocketChannel>(io);
        return connfd;
#pragma GCC diagnostic pop
    }

    int bind(const char *local_name = "UdsClient.ipc")
    {
        sockaddr_u local_addr;
        memset(&local_addr, 0, sizeof(local_addr));
        int ret = sockaddr_set_ipport(&local_addr, local_name, -1);
        if (ret != 0)
        {
            return NABS(ret);
        }
        return bind(&local_addr.sun);
    }

    int bind(struct sockaddr_un *local_addr)
    {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
        if (channel == NULL || channel->isClosed())
        {
            return -1;
        }
        unlink(local_addr->sun_path);
        int ret = ::bind(channel->fd(), (const struct sockaddr *)local_addr, SOCKADDR_LEN(local_addr));
        if (ret != 0)
        {
            perror("bind");
        }
        return ret;
#pragma GCC diagnostic pop
    }

    // closesocket thread-safe
    void closesocket()
    {
        if (channel && channel->status != SocketChannel::CLOSED)
        {
            loop_->runInLoop([this]()
                             {
                                 if (channel)
                                 {
                                     setReconnect(NULL);
                                     channel->close();
                                 }
                             });
        }
    }

    int startConnect()
    {
        if (channel == NULL || channel->isClosed())
        {
            int connfd = -1;
            connfd = createsocket(remote_name.c_str());
            if (connfd < 0)
            {
                hloge("createsocket %s return %d!\n", remote_name.c_str(), connfd);
                return connfd;
            }
        }
        if (channel == NULL || channel->status >= SocketChannel::CONNECTING)
        {
            return -1;
        }
        if (connect_timeout)
        {
            channel->setConnectTimeout(connect_timeout);
        }
        channel->onconnect = [this]()
        {
            if (unpack_setting)
            {
                channel->setUnpack(unpack_setting);
            }
            channel->startRead();
            if (onConnection)
            {
                onConnection(channel);
            }
            if (reconn_setting)
            {
                reconn_setting_reset(reconn_setting);
            }
        };
        channel->onread = [this](Buffer *buf)
        {
            if (onMessage)
            {
                onMessage(channel, buf);
            }
        };
        channel->onwrite = [this](Buffer *buf)
        {
            if (onWriteComplete)
            {
                onWriteComplete(channel, buf);
            }
        };
        channel->onclose = [this]()
        {
            bool reconnect = reconn_setting != NULL;
            if (onConnection)
            {
                onConnection(channel);
            }
            if (reconnect)
            {
                startReconnect();
            }
        };
        return channel->startConnect();
    }

    int startReconnect()
    {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
        if (!reconn_setting)
            return -1;
        if (!reconn_setting_can_retry(reconn_setting))
            return -2;
        uint32_t delay = reconn_setting_calc_delay(reconn_setting);
        hlogi("reconnect... cnt=%d, delay=%d", reconn_setting->cur_retry_cnt, reconn_setting->cur_delay);
        loop_->setTimeout((int)delay, [this](TimerID timerID)
                          {
                              (void)(timerID);
                              startConnect();
                          });
        return 0;
#pragma GCC diagnostic pop
    }

    // start thread-safe
    void start()
    {
        loop_->runInLoop(std::bind(&UdsClientEventLoopTmpl::startConnect, this));
    }

    bool isConnected()
    {
        if (channel == NULL)
            return false;
        return channel->isConnected();
    }

    // send thread-safe
    int send(const void *data, int size)
    {
        if (!isConnected())
            return -1;
        return channel->write(data, size);
    }
    int send(Buffer *buf)
    {
        return send(buf->data(), buf->size());
    }
    int send(const std::string &str)
    {
        return send(str.data(), str.size());
    }

    void setConnectTimeout(int ms)
    {
        connect_timeout = ms;
    }

    void setReconnect(reconn_setting_t *setting)
    {
        if (setting == NULL)
        {
            HV_FREE(reconn_setting);
            return;
        }
        if (reconn_setting == NULL)
        {
            HV_ALLOC_SIZEOF(reconn_setting);
        }
        *reconn_setting = *setting;
    }
    bool isReconnect()
    {
        return reconn_setting && reconn_setting->cur_retry_cnt > 0;
    }

    void setUnpack(unpack_setting_t *setting)
    {
        if (setting == NULL)
        {
            HV_FREE(unpack_setting);
            return;
        }
        if (unpack_setting == NULL)
        {
            HV_ALLOC_SIZEOF(unpack_setting);
        }
        *unpack_setting = *setting;
    }

public:
    UdsSocketChannelPtr channel;

    std::string remote_name;
    sockaddr_u remote_addr;
    int connect_timeout;
    reconn_setting_t *reconn_setting;
    unpack_setting_t *unpack_setting;

    // Callback
    std::function<void(const UdsSocketChannelPtr &)> onConnection;
    std::function<void(const UdsSocketChannelPtr &, Buffer *)> onMessage;
    // NOTE: Use Channel::isWriteComplete in onWriteComplete callback to determine whether all data has been written.
    std::function<void(const UdsSocketChannelPtr &, Buffer *)> onWriteComplete;

private:
    EventLoopPtr loop_;
};

template <class UdsSocketChannel = SocketChannel>
class UdsClientTmpl : private EventLoopThread, public UdsClientEventLoopTmpl<UdsSocketChannel>
{
public:
    UdsClientTmpl(EventLoopPtr loop = NULL) :
        EventLoopThread(loop), UdsClientEventLoopTmpl<UdsSocketChannel>(EventLoopThread::loop()), is_loop_owner(loop == NULL)
    {
    }
    virtual ~UdsClientTmpl()
    {
        stop(true);
    }

    const EventLoopPtr &loop()
    {
        return EventLoopThread::loop();
    }

    // start thread-safe
    void start(bool wait_threads_started = true)
    {
        if (isRunning())
        {
            UdsClientEventLoopTmpl<UdsSocketChannel>::start();
        }
        else
        {
            EventLoopThread::start(wait_threads_started, [this]()
                                   {
                                       UdsClientTmpl::startConnect();
                                       return 0;
                                   });
        }
    }

    // stop thread-safe
    void stop(bool wait_threads_stopped = true)
    {
        UdsClientEventLoopTmpl<UdsSocketChannel>::closesocket();
        if (is_loop_owner)
        {
            EventLoopThread::stop(wait_threads_stopped);
        }
    }

private:
    bool is_loop_owner;
};

typedef UdsClientTmpl<SocketChannel> UdsClient;

} // namespace hv

#endif // HV_UDS_CLIENT_HPP_