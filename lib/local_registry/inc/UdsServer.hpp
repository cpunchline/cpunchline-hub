#ifndef HV_UDS_SERVER_HPP_
#define HV_UDS_SERVER_HPP_

#include "hv/hsocket.h"
#include "hv/hssl.h"
#include "hv/hlog.h"

#include "hv/EventLoopThreadPool.h"
#include "hv/Channel.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wunknown-pragmas"

namespace hv
{

template <class UdsSocketChannel = SocketChannel>
class UdsServerEventLoopTmpl
{
public:
    typedef std::shared_ptr<UdsSocketChannel> UdsSocketChannelPtr;

    UdsServerEventLoopTmpl(EventLoopPtr loop = NULL)
    {
        acceptor_loop = loop ? loop : std::make_shared<EventLoop>();
        listenfd = -1;
        unpack_setting = NULL;
        max_connections = UINT32_MAX;
        load_balance = LB_RoundRobin;
    }

    virtual ~UdsServerEventLoopTmpl()
    {
        HV_FREE(unpack_setting);
    }

    EventLoopPtr loop(int idx = -1)
    {
        EventLoopPtr worker_loop = worker_threads.loop(idx);
        if (worker_loop == NULL)
        {
            worker_loop = acceptor_loop;
        }
        return worker_loop;
    }

    //@retval >=0 listenfd, <0 error
    int createsocket(const char *name = "udsServer.ipc")
    {
        unlink(name);
        listenfd = ListenUnix(name);
        if (listenfd < 0)
            return listenfd;
        this->local_name = name;
        return listenfd;
    }
    // closesocket thread-safe
    void closesocket()
    {
        if (listenfd >= 0)
        {
            hloop_t *loop = acceptor_loop->loop();
            if (loop)
            {
                hio_t *listenio = hio_get(loop, listenfd);
                assert(listenio != NULL);
                hio_close_async(listenio);
            }
            listenfd = -1;
        }
    }

    void setMaxConnectionNum(uint32_t num)
    {
        max_connections = num;
    }

    void setLoadBalance(load_balance_e lb)
    {
        load_balance = lb;
    }

    // NOTE: totalThreadNum = 1 acceptor_thread + N worker_threads (N can be 0)
    void setThreadNum(int num)
    {
        worker_threads.setThreadNum(num);
    }

    int startAccept()
    {
        if (listenfd < 0)
        {
            listenfd = createsocket(local_name.c_str());
            if (listenfd < 0)
            {
                hloge("createsocket %s return %d!\n", local_name.c_str(), listenfd);
                return listenfd;
            }
        }
        hloop_t *loop = acceptor_loop->loop();
        if (loop == NULL)
            return -2;
        hio_t *listenio = haccept(loop, listenfd, onAccept);
        assert(listenio != NULL);
        hevent_set_userdata(listenio, this);
        return 0;
    }

    int stopAccept()
    {
        if (listenfd < 0)
            return -1;
        hloop_t *loop = acceptor_loop->loop();
        if (loop == NULL)
            return -2;
        hio_t *listenio = hio_get(loop, listenfd);
        assert(listenio != NULL);
        return hio_del(listenio, HV_READ);
    }

    // start thread-safe
    void start(bool wait_threads_started = true)
    {
        if (worker_threads.threadNum() > 0)
        {
            worker_threads.start(wait_threads_started);
        }
        acceptor_loop->runInLoop(std::bind(&UdsServerEventLoopTmpl::startAccept, this));
    }
    // stop thread-safe
    void stop(bool wait_threads_stopped = true)
    {
        closesocket();
        if (worker_threads.threadNum() > 0)
        {
            worker_threads.stop(wait_threads_stopped);
        }
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

    // channel
    const UdsSocketChannelPtr &addChannel(hio_t *io)
    {
        uint32_t id = hio_id(io);
        auto channel = std::make_shared<UdsSocketChannel>(io);
        std::lock_guard<std::mutex> locker(mutex_);
        channels[id] = channel;
        return channels[id];
    }

    UdsSocketChannelPtr getChannelById(uint32_t id)
    {
        std::lock_guard<std::mutex> locker(mutex_);
        auto iter = channels.find(id);
        return iter != channels.end() ? iter->second : NULL;
    }

    void removeChannel(const UdsSocketChannelPtr &channel)
    {
        uint32_t id = channel->id();
        std::lock_guard<std::mutex> locker(mutex_);
        channels.erase(id);
    }

    size_t connectionNum()
    {
        std::lock_guard<std::mutex> locker(mutex_);
        return channels.size();
    }

    int foreachChannel(std::function<void(const UdsSocketChannelPtr &channel)> fn)
    {
        std::lock_guard<std::mutex> locker(mutex_);
        for (auto &pair : channels)
        {
            fn(pair.second);
        }
        return channels.size();
    }

    // broadcast thread-safe
    int broadcast(const void *data, int size)
    {
        return foreachChannel([data, size](const UdsSocketChannelPtr &channel)
                              {
                                  channel->write(data, size);
                              });
    }

    int broadcast(const std::string &str)
    {
        return broadcast(str.data(), str.size());
    }

private:
    static void newConnEvent(hio_t *connio)
    {
        UdsServerEventLoopTmpl *server = (UdsServerEventLoopTmpl *)hevent_userdata(connio);
        if (server->connectionNum() >= server->max_connections)
        {
            hlogw("over max_connections");
            hio_close(connio);
            return;
        }

        // NOTE: attach to worker loop
        EventLoop *worker_loop = currentThreadEventLoop;
        assert(worker_loop != NULL);
        hio_attach(worker_loop->loop(), connio);

        const UdsSocketChannelPtr &channel = server->addChannel(connio);
        channel->status = SocketChannel::CONNECTED;

        channel->onread = [server, &channel](Buffer *buf)
        {
            if (server->onMessage)
            {
                server->onMessage(channel, buf);
            }
        };
        channel->onwrite = [server, &channel](Buffer *buf)
        {
            if (server->onWriteComplete)
            {
                server->onWriteComplete(channel, buf);
            }
        };
        channel->onclose = [server, &channel]()
        {
            EventLoop *p_worker_loop = currentThreadEventLoop;
            assert(p_worker_loop != NULL);
            --p_worker_loop->connectionNum;

            channel->status = SocketChannel::CLOSED;
            if (server->onConnection)
            {
                server->onConnection(channel);
            }
            server->removeChannel(channel);
            // NOTE: After removeChannel, channel may be destroyed,
            // so in this lambda function, no code should be added below.
        };

        if (server->unpack_setting)
        {
            channel->setUnpack(server->unpack_setting);
        }
        channel->startRead();
        if (server->onConnection)
        {
            server->onConnection(channel);
        }
    }

    static void onAccept(hio_t *connio)
    {
        UdsServerEventLoopTmpl *server = (UdsServerEventLoopTmpl *)hevent_userdata(connio);
        // NOTE: detach from acceptor loop
        hio_detach(connio);
        EventLoopPtr worker_loop = server->worker_threads.nextLoop(server->load_balance);
        if (worker_loop == NULL)
        {
            worker_loop = server->acceptor_loop;
        }
        ++worker_loop->connectionNum;
        worker_loop->runInLoop(std::bind(&UdsServerEventLoopTmpl::newConnEvent, connio));
    }

public:
    std::string local_name;
    int listenfd;
    unpack_setting_t *unpack_setting;
    // Callback
    std::function<void(const UdsSocketChannelPtr &)> onConnection;
    std::function<void(const UdsSocketChannelPtr &, Buffer *)> onMessage;
    // NOTE: Use Channel::isWriteComplete in onWriteComplete callback to determine whether all data has been written.
    std::function<void(const UdsSocketChannelPtr &, Buffer *)> onWriteComplete;

    uint32_t max_connections;
    load_balance_e load_balance;

private:
    // id => UdsSocketChannelPtr
    std::map<uint32_t, UdsSocketChannelPtr> channels; // GUAREDE_BY(mutex_)
    std::mutex mutex_;

    EventLoopPtr acceptor_loop;
    EventLoopThreadPool worker_threads;
};

template <class UdsSocketChannel = SocketChannel>
class UdsServerTmpl : private EventLoopThread, public UdsServerEventLoopTmpl<UdsSocketChannel>
{
public:
    UdsServerTmpl(EventLoopPtr loop = NULL) :
        EventLoopThread(loop), UdsServerEventLoopTmpl<UdsSocketChannel>(EventLoopThread::loop()), is_loop_owner(loop == NULL)
    {
    }
    virtual ~UdsServerTmpl()
    {
        stop(true);
    }

    EventLoopPtr loop(int idx = -1)
    {
        return UdsServerEventLoopTmpl<UdsSocketChannel>::loop(idx);
    }

    // start thread-safe
    void start(bool wait_threads_started = true)
    {
        UdsServerEventLoopTmpl<UdsSocketChannel>::start(wait_threads_started);
        if (!isRunning())
        {
            EventLoopThread::start(wait_threads_started);
        }
    }

    // stop thread-safe
    void stop(bool wait_threads_stopped = true)
    {
        if (is_loop_owner)
        {
            EventLoopThread::stop(wait_threads_stopped);
        }
        UdsServerEventLoopTmpl<UdsSocketChannel>::stop(wait_threads_stopped);
    }

private:
    bool is_loop_owner;
};

typedef UdsServerTmpl<SocketChannel> UdsServer;

} // namespace hv

#pragma GCC diagnostic pop

#endif // HV_UDS_SERVER_HPP_