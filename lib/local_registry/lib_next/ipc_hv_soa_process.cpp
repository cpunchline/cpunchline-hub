#include "ipc_hv_soa_manager.hpp"

static void onProcessConnection(const hv::SocketChannelPtr &channel)
{
    (void)channel;
}

static void onProcessMessage(const hv::SocketChannelPtr &channel, hv::Buffer *inbuf)
{
    (void)channel;
    (void)inbuf;
}

static void onProcessWriteComplete(const hv::SocketChannelPtr &channel, hv::Buffer *inbuf)
{
    (void)channel;
    (void)inbuf;
}