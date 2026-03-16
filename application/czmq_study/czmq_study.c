#include "czmq.h"

/*
CZMQ,一个用于 C 语言的 ZeroMQ 语言绑定
更高级别 API 中实现的目标:
    自动处理套接字.我发现必须手动关闭套接字,并且在某些(但不是全部)情况下必须显式定义延迟超时很麻烦.如果能有办法在我关闭上下文时自动关闭套接字,那就太好了.
    可移植线程管理.每个重要的 ZeroMQ 应用程序都使用线程,但 POSIX 线程是不可移植的.因此,一个体面的高级 API 应该将其隐藏在可移植层下.
    从父线程到子线程的管道连接.这是一个反复出现的问题:如何在父线程和子线程之间发出信号.我们的 API 应该提供一个 ZeroMQ 消息管道(自动使用 PAIR sockets 和 inproc).
    便携式时钟.即使将时间提高到毫秒级精度,或者休眠几毫秒,也是不可行的.现实的 ZeroMQ 应用程序需要可移植的时钟,因此我们的 API 应该提供它们.
    一个 Reactor 来替换 zmq_poll().轮询循环很简单,但很笨拙.编写大量这些,我们最终会一遍又一遍地做相同的工作:计算计时器,并在套接字准备就绪时调用代码.带有插座阅读器和计时器的简单反应器将节省大量重复工作.
    正确处理 Ctrl-C.我们已经看到了如何捕获中断.如果所有应用程序都发生这种情况,那将非常有用.
*/
// zstr     帮助程序
// zframe   消息框架
// zmessage 一个或多个框架的列表

#define NBR_CLIENTS            10
#define NBR_WORKERS            3
#define LBBROKER2_WORKER_READY "READY" //  Signals worker is ready
#define LBBROKER3_WORKER_READY "\001"  //  Signals worker is ready

//  Basic request-reply client using REQ socket
//
static void lbbroker2_client_task(zsock_t *pipe, void *args)
{
    (void)args;
    // Signal caller zactor has started
    zsock_signal(pipe, 0);
    zsock_t *client = zsock_new(ZMQ_REQ);

#if (defined(WIN32))
    zsock_connect(client, "tcp://localhost:5672"); // frontend
#else
    zsock_connect(client, "ipc://frontend.ipc");
#endif

    //  Send request, get reply
    zstr_send(client, "HELLO");
    char *reply = zstr_recv(client);
    if (reply)
    {
        printf("Client: %s\n", reply);
        free(reply);
    }

    zsock_destroy(&client);
}

//  Worker using REQ socket to do load-balancing
//
static void lbbroker2_worker_task(zsock_t *pipe, void *args)
{
    (void)args;
    // Signal caller zactor has started
    zsock_signal(pipe, 0);
    zsock_t *worker = zsock_new(ZMQ_REQ);

#if (defined(WIN32))
    zsock_connect(worker, "tcp://localhost:5673"); // backend
#else
    zsock_connect(worker, "ipc://backend.ipc");
#endif

    //  Tell broker we're ready for work
    zframe_t *frame = zframe_new(LBBROKER2_WORKER_READY, strlen(LBBROKER2_WORKER_READY));
    zframe_send(&frame, worker, 0);

    //  Process messages as they arrive
    zpoller_t *poll = zpoller_new(pipe, worker, NULL);
    while (true)
    {
        zsock_t *ready = zpoller_wait(poll, -1);
        if (ready == pipe)
            break; //  Done

        assert(ready == worker);
        zmsg_t *msg = zmsg_recv(worker);
        if (!msg)
            break; //  Interrupted
        zframe_print(zmsg_last(msg), "Worker: ");
        zframe_reset(zmsg_last(msg), "OK", 2);
        zmsg_send(&msg, worker);
    }

    if (frame)
        zframe_destroy(&frame);
    zsock_destroy(&worker);
    zpoller_destroy(&poll);

    // Signal done
    zsock_signal(pipe, 0);
}

//  .split main task
//  Now we come to the main task. This has the identical functionality to
//  the previous {{lbbroker}} broker example, but uses CZMQ to start child
//  threads, to hold the list of workers, and to read and send messages:

void lbbroker2()
{
    zsock_t *frontend = zsock_new(ZMQ_ROUTER);
    zsock_t *backend = zsock_new(ZMQ_ROUTER);

    // IPC doesn't yet work on MS Windows.
#if (defined(WIN32))
    zsock_bind(frontend, "tcp://*:5672");
    zsock_bind(backend, "tcp://*:5673");
#else
    zsock_bind(frontend, "ipc://frontend.ipc");
    zsock_bind(backend, "ipc://backend.ipc");
#endif

    int actor_nbr = 0;
    zactor_t *actors[NBR_CLIENTS + NBR_WORKERS];

    int client_nbr;
    for (client_nbr = 0; client_nbr < NBR_CLIENTS; client_nbr++)
        actors[actor_nbr++] = zactor_new(lbbroker2_client_task, NULL);
    int worker_nbr;
    for (worker_nbr = 0; worker_nbr < NBR_WORKERS; worker_nbr++)
        actors[actor_nbr++] = zactor_new(lbbroker2_worker_task, NULL);

    //  Queue of available workers
    zlist_t *workers = zlist_new();

    //  .split main load-balancer loop
    //  Here is the main loop for the load balancer. It works the same way
    //  as the previous example, but is a lot shorter because CZMQ gives
    //  us an API that does more with fewer calls:
    zpoller_t *poll1 = zpoller_new(backend, NULL);
    zpoller_t *poll2 = zpoller_new(backend, frontend, NULL);
    while (true)
    {
        //  Poll frontend only if we have available workers
        zpoller_t *poll = zlist_size(workers) ? poll2 : poll1;
        zsock_t *ready = zpoller_wait(poll, -1);
        if (ready == NULL)
            break; //  Interrupted

        //    Handle worker activity on backend
        if (ready == backend)
        {
            //  Use worker identity for load-balancing
            zmsg_t *msg = zmsg_recv(backend);
            if (!msg)
                break; //  Interrupted

#if 0
            // zmsg_unwrap is DEPRECATED as over-engineered, poor style
            zframe_t *identity = zmsg_unwrap(msg);
#else
            zframe_t *identity = zmsg_pop(msg);
            zframe_t *delimiter = zmsg_pop(msg);
            zframe_destroy(&delimiter);
#endif

            zlist_append(workers, identity);

            //  Forward message to client if it's not a READY
            zframe_t *frame = zmsg_first(msg);
            if (memcmp(zframe_data(frame), LBBROKER2_WORKER_READY, strlen(LBBROKER2_WORKER_READY)) == 0)
            {
                zmsg_destroy(&msg);
            }
            else
            {
                zmsg_send(&msg, frontend);
                if (--client_nbr == 0)
                    break; // Exit after N messages
            }
        }
        else if (ready == frontend)
        {
            //  Get client request, route to first available worker
            zmsg_t *msg = zmsg_recv(frontend);
            if (msg)
            {
#if 0
                // zmsg_wrap is DEPRECATED as unsafe
                zmsg_wrap(msg, (zframe_t *)zlist_pop(workers));
#else
                zmsg_pushmem(msg, NULL, 0); // delimiter
                zmsg_push(msg, (zframe_t *)zlist_pop(workers));
#endif

                zmsg_send(&msg, backend);
            }
        }
    }
    //  When we're done, clean up properly
    while (zlist_size(workers))
    {
        zframe_t *frame = (zframe_t *)zlist_pop(workers);
        zframe_destroy(&frame);
    }
    zlist_destroy(&workers);

    for (actor_nbr = 0; actor_nbr < NBR_CLIENTS + NBR_WORKERS; actor_nbr++)
    {
        zactor_destroy(&actors[actor_nbr]);
    }

    zpoller_destroy(&poll1);
    zpoller_destroy(&poll2);
    zsock_destroy(&frontend);
    zsock_destroy(&backend);
}

//  Basic request-reply client using REQ socket
//
static void lbbroker3_client_task(zsock_t *pipe, void *args)
{
    (void)args;
    // Signal ready
    zsock_signal(pipe, 0);

    zsock_t *client = zsock_new_req("ipc://frontend.ipc");
    zpoller_t *poller = zpoller_new(pipe, client, NULL);
    zpoller_set_nonstop(poller, true);

    //  Send request, get reply
    while (true)
    {
        zstr_send(client, "HELLO");

        zsock_t *ready = zpoller_wait(poller, -1);
        if (ready == NULL)
            continue; // Interrupted
        else if (ready == pipe)
            break; // Shutdown
        else
            assert(ready == client); // Data Available

        char *reply = zstr_recv(client);
        if (!reply)
            break;
        printf("Client: %s\n", reply);
        free(reply);
        sleep(1);
    }

    zpoller_destroy(&poller);
    zsock_destroy(&client);
}

//  Worker using REQ socket to do load-balancing
//
static void lbbroker3_worker_task(zsock_t *pipe, void *args)
{
    (void)args;
    // Signal ready
    zsock_signal(pipe, 0);

    zsock_t *worker = zsock_new_req("ipc://backend.ipc");
    zpoller_t *poller = zpoller_new(pipe, worker, NULL);
    zpoller_set_nonstop(poller, true);

    //  Tell broker we're ready for work
    zframe_t *frame = zframe_new(LBBROKER3_WORKER_READY, 1);
    zframe_send(&frame, worker, 0);

    //  Process messages as they arrive
    while (true)
    {
        zsock_t *ready = zpoller_wait(poller, -1);
        if (ready == NULL)
            continue; // Interrupted
        else if (ready == pipe)
            break; // Shutdown
        else
            assert(ready == worker); // Data Available

        zmsg_t *msg = zmsg_recv(worker);
        if (!msg)
            break; //  Interrupted
        zframe_print(zmsg_last(msg), "Worker: ");
        zframe_reset(zmsg_last(msg), "OK", 2);
        zmsg_send(&msg, worker);
    }

    zpoller_destroy(&poller);
    zsock_destroy(&worker);
}

//  .until
//  Our load-balancer structure, passed to reactor handlers
typedef struct
{
    zsock_t *frontend; //  Listen to clients
    zsock_t *backend;  //  Listen to workers
    zlist_t *workers;  //  List of ready workers
} lbbroker_t;

//  .split reactor design
//  In the reactor design, each time a message arrives on a socket, the
//  reactor passes it to a handler function. We have two handlers; one
//  for the frontend, one for the backend:

//  Handle input from client, on frontend
static int s_handle_frontend(zloop_t *loop, zsock_t *reader, void *arg)
{
    (void)reader;
    lbbroker_t *self = (lbbroker_t *)arg;
    zmsg_t *msg = zmsg_recv(self->frontend);
    if (msg)
    {
        zmsg_pushmem(msg, NULL, 0); // delimiter
        zmsg_push(msg, (zframe_t *)zlist_pop(self->workers));
        zmsg_send(&msg, self->backend);

        //  Cancel reader on frontend if we went from 1 to 0 workers
        if (zlist_size(self->workers) == 0)
        {
            zloop_reader_end(loop, self->frontend);
        }
    }
    return 0;
}

//  Handle input from worker, on backend
static int s_handle_backend(zloop_t *loop, zsock_t *reader, void *arg)
{
    (void)reader;
    //  Use worker identity for load-balancing
    lbbroker_t *self = (lbbroker_t *)arg;
    zmsg_t *msg = zmsg_recv(self->backend);
    if (msg)
    {
        zframe_t *identity = zmsg_pop(msg);
        zframe_t *delimiter = zmsg_pop(msg);
        zframe_destroy(&delimiter);
        zlist_append(self->workers, identity);

        //  Enable reader on frontend if we went from 0 to 1 workers
        if (zlist_size(self->workers) == 1)
        {
            zloop_reader(loop, self->frontend, s_handle_frontend, self);
        }
        //  Forward message to client if it's not a READY
        zframe_t *frame = zmsg_first(msg);
        if (memcmp(zframe_data(frame), LBBROKER3_WORKER_READY, 1) == 0)
            zmsg_destroy(&msg);
        else
            zmsg_send(&msg, self->frontend);
    }
    return 0;
}

//  .split main task
//  And the main task now sets up child tasks, then starts its reactor.
//  If you press Ctrl-C, the reactor exits and the main task shuts down.
//  Because the reactor is a CZMQ class, this example may not translate
//  into all languages equally well.

void lbbroker3()
{
    lbbroker_t *self = (lbbroker_t *)zmalloc(sizeof(lbbroker_t));
    self->frontend = zsock_new_router("ipc://frontend.ipc");
    self->backend = zsock_new_router("ipc://backend.ipc");

    zactor_t *actors[NBR_CLIENTS + NBR_WORKERS];
    int actor_nbr = 0;

    int client_nbr;
    for (client_nbr = 0; client_nbr < NBR_CLIENTS; client_nbr++)
        actors[actor_nbr++] = zactor_new(lbbroker3_client_task, NULL);
    int worker_nbr;
    for (worker_nbr = 0; worker_nbr < NBR_WORKERS; worker_nbr++)
        actors[actor_nbr++] = zactor_new(lbbroker3_worker_task, NULL);

    //  Queue of available workers
    self->workers = zlist_new();

    //  Prepare reactor and fire it up
    zloop_t *reactor = zloop_new();
    zloop_reader(reactor, self->backend, s_handle_backend, self);
    zloop_start(reactor);
    zloop_destroy(&reactor);
    for (actor_nbr = 0; actor_nbr < NBR_CLIENTS + NBR_WORKERS; actor_nbr++)
        zactor_destroy(&actors[actor_nbr]);

    //  When we're done, clean up properly
    while (zlist_size(self->workers))
    {
        zframe_t *frame = (zframe_t *)zlist_pop(self->workers);
        zframe_destroy(&frame);
    }
    zlist_destroy(&self->workers);
    zsock_destroy(&self->frontend);
    zsock_destroy(&self->backend);
    free(self);
}

int main()
{
    // lbbroker2();
    lbbroker3();
    return 0;
}

#define WORKER_READY "1"
// 基本可靠队列 (Simple Pirate Pattern)
//  client就使用lpclient
void spqueue()
{
    void *ctx = zmq_ctx_new();
    void *frontend = zmq_socket(ctx, ZMQ_ROUTER);
    void *backend = zmq_socket(ctx, ZMQ_ROUTER);
    zmq_bind(frontend, "tcp://*:5555"); //  For clients
    zmq_bind(backend, "tcp://*:5556");  //  For workers

    //  Queue of available workers
    zlist_t *workers = zlist_new();

    //  The body of this example is exactly the same as lbbroker2.
    //  .skip
    while (true)
    {
        zmq_pollitem_t items[] = {
            {backend,  0, ZMQ_POLLIN, 0},
            {frontend, 0, ZMQ_POLLIN, 0}
        };
        //  Poll frontend only if we have available workers
        int rc = zmq_poll(items, zlist_size(workers) ? 2 : 1, -1);
        if (rc == -1)
            break; //  Interrupted

        //  Handle worker activity on backend
        if (items[0].revents & ZMQ_POLLIN)
        {
            //  Use worker identity for load-balancing
            zmsg_t *msg = zmsg_recv(backend);
            if (!msg)
                break; //  Interrupted
            zframe_t *identity = zmsg_unwrap(msg);
            zlist_append(workers, identity);

            //  Forward message to client if it's not a READY
            zframe_t *frame = zmsg_first(msg);
            if (memcmp(zframe_data(frame), WORKER_READY, 1) == 0)
                zmsg_destroy(&msg);
            else
                zmsg_send(&msg, frontend);
        }
        if (items[1].revents & ZMQ_POLLIN)
        {
            //  Get client request, route to first available worker
            zmsg_t *msg = zmsg_recv(frontend);
            if (msg)
            {
                zmsg_wrap(msg, (zframe_t *)zlist_pop(workers));
                zmsg_send(&msg, backend);
            }
        }
    }
    //  When we're done, clean up properly
    while (zlist_size(workers))
    {
        zframe_t *frame = (zframe_t *)zlist_pop(workers);
        zframe_destroy(&frame);
    }
    zlist_destroy(&workers);
    zmq_ctx_destroy(ctx);
}

void spworker()
{
    void *ctx = zmq_ctx_new();
    void *worker = zmq_socket(ctx, ZMQ_ROUTER);

    //  Set random identity to make tracing easier
    srandom((unsigned)time(NULL));
    char identity[10];
    sprintf(identity, "%04X-%04X", randof(0x10000), randof(0x10000));
    zmq_setsockopt(worker, ZMQ_IDENTITY, identity, strlen(identity));
    zmq_connect(worker, "tcp://localhost:5556");

    //  Tell broker we're ready for work
    printf("I: (%s) worker ready\n", identity);
    zframe_t *frame = zframe_new(WORKER_READY, 1);
    zframe_send(&frame, worker, 0);

    int cycles = 0;
    while (true)
    {
        zmsg_t *msg = zmsg_recv(worker);
        if (!msg)
            break; //  Interrupted

        //  Simulate various problems, after a few cycles
        cycles++;
        if (cycles > 3 && randof(5) == 0)
        {
            printf("I: (%s) simulating a crash\n", identity);
            zmsg_destroy(&msg);
            break;
        }
        else if (cycles > 3 && randof(5) == 0)
        {
            printf("I: (%s) simulating CPU overload\n", identity);
            sleep(3);
            if (zctx_interrupted)
                break;
        }
        printf("I: (%s) normal reply\n", identity);
        sleep(1); //  Do some heavy work
        zmsg_send(&msg, worker);
    }
    zmq_ctx_destroy(ctx);
}
