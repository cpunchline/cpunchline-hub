#include "zmq_test.h"

void get_zmq_lib_version()
{
    int major = -1, minor = -1, patch = -1;
    zmq_version(&major, &minor, &patch);
    printf("Current 0MQ version is %d.%d.%d\n", major, minor, patch);
}

void hwserver()
{
    //  Hello World server
    //  Socket to talk to clients
    void *context = zmq_ctx_new();
    void *responder = zmq_socket(context, ZMQ_REP);
    int rc = zmq_bind(responder, "tcp://*:5555");
    assert(rc == 0);
    (void)rc;

    while (1)
    {
        char buffer[10];
        zmq_recv(responder, buffer, 10, 0);
        printf("Received Hello\n");
        sleep(1); //  Do some 'work'
        zmq_send(responder, "World", 5, 0);
    }
}

void hwclient()
{
    printf("Connecting to hello world server...\n");
    void *context = zmq_ctx_new();
    void *requester = zmq_socket(context, ZMQ_REQ);
    zmq_connect(requester, "tcp://localhost:5555");

    int request_nbr;
    for (request_nbr = 0; request_nbr != 10; request_nbr++)
    {
        char buffer[10];
        printf("Sending Hello %d...\n", request_nbr);
        zmq_send(requester, "Hello", 5, 0);
        zmq_recv(requester, buffer, 10, 0);
        printf("Received World %d\n", request_nbr);
    }
    zmq_close(requester);
    zmq_ctx_destroy(context);
}

#define REQUEST_TIMEOUT 2500 // msecs, (>1000!)
#define REQUEST_RETRIES 3    // Before we abandon
#define SERVER_ENDPOINT "tcp://localhost:5555"

void lpserver()
{
    srandom((unsigned)time(NULL));

    void *context = zmq_ctx_new();
    void *server = zmq_socket(context, ZMQ_REP);
    zmq_bind(server, "tcp://*:5555");

    int cycles = 0;
    while (1)
    {
        char *request = s_recv(server);
        cycles++;

        //  Simulate various problems, after a few cycles
        if (cycles > 3 && randof(3) == 0)
        {
            printf("I: simulating a crash\n");
            break;
        }
        else if (cycles > 3 && randof(3) == 0)
        {
            printf("I: simulating CPU overload\n");
            sleep(2);
        }
        printf("I: normal request (%s)\n", request);
        sleep(1); //  Do some heavy work
        s_send(server, request);
        free(request);
    }
    zmq_close(server);
    zmq_ctx_destroy(context);
}

void lpclient()
{
    void *context = zmq_ctx_new();
    void *client = zmq_socket(context, ZMQ_REQ);
    int rc = zmq_connect(client, SERVER_ENDPOINT);
    assert(rc == 0);

    printf("I: Connecting to server...\n");

    int sequence = 0;
    int retries_left = REQUEST_RETRIES;

    while (retries_left)
    {
        char request[32] = {0};
        sprintf(request, "%d", ++sequence);
        rc = zmq_send(client, request, strlen(request), 0);
        assert(rc != -1);

        int expect_reply = 1;
        while (expect_reply)
        {
            zmq_pollitem_t items[] = {
                {client, 0, ZMQ_POLLIN, 0}
            };
            rc = zmq_poll(items, 1, REQUEST_TIMEOUT);
            if (rc == -1)
                break; // Interrupted

            if (items[0].revents & ZMQ_POLLIN)
            {
                char buffer[256];
                rc = zmq_recv(client, buffer, 255, 0);
                if (rc == -1)
                    break; // interrupted

                buffer[rc] = 0; // Null terminate the received string
                if (atoi(buffer) == sequence)
                {
                    printf("I: server replied OK (%s)\n", buffer);
                    retries_left = REQUEST_RETRIES;
                    expect_reply = 0;
                }
                else
                {
                    printf("E: malformed reply from server: %s\n", buffer);
                }
            }
            else
            {
                if (--retries_left == 0)
                {
                    printf("E: Server seems to be offline, abandoning\n");
                    break;
                }
                else
                {
                    printf("W: no response from server, retrying...\n");
                    zmq_close(client);
                    client = zmq_socket(context, ZMQ_REQ);
                    rc = zmq_connect(client, SERVER_ENDPOINT);
                    assert(rc == 0);
                    rc = zmq_send(client, request, strlen(request), 0);
                    assert(rc != -1);
                }
            }
        }
    }

    zmq_close(client);
    zmq_ctx_destroy(context);
}

void hwserver2()
{
    void *context = zmq_ctx_new();
    void *responder = zmq_socket(context, ZMQ_REP);
    int rc = zmq_bind(responder, "tcp://*:5555");
    assert(rc == 0);
    (void)rc;

    while (1)
    {
        char buffer[10] = {0};
        zmq_recv(responder, buffer, 10, 0);
        printf("Received request msg: %s\n", buffer);
        printf("Send respond msg: World\n\n");
        zmq_send(responder, "World", 5, 0);
        sleep(1); // Do some work
    }
}

void hwclient2()
{
    printf("Connecting to hello world server...\n");
    void *context = zmq_ctx_new();
    void *requester = zmq_socket(context, ZMQ_DEALER);
    zmq_connect(requester, "tcp://localhost:5555");
    int request_nbr;   // request number
    int reply_nbr = 0; // receive respond number
    for (request_nbr = 0; request_nbr < 10; request_nbr++)
    {
        char buffer[10];
        memset(buffer, 0, sizeof(buffer));
        printf("Sending request msg: Hello NO=%d...\n", request_nbr + 1);
        // send request msg to server
        s_sendmore(requester, "");          // send multi part msg,the first part is empty part
        zmq_send(requester, "Hello", 5, 0); // the second part is your request msg
        // receive reply msg
        int len;
        len = zmq_recv(requester, buffer, 10, 0);
        if (len == -1)
        {
            printf("Error:%s\n", zmq_strerror(errno));
            exit(-1);
        }
        // if the first part you received is empty part,then continue receiving next part
        if (strcmp(buffer, "") == 0)
        {
            memset(buffer, 0, sizeof(buffer));
            len = zmq_recv(requester, buffer, 10, 0);
            if (len == -1)
            {
                printf("Error:%s\n", zmq_strerror(errno));
                exit(-1);
            }
            printf("Received respond msg: %s NO=%d\n\n", buffer, ++reply_nbr);
        }
        // if the first part you received is not empty part,discard the whole ZMQ msg
        else
        {
            printf("Discard the ZMQ message!\n");
        }
    }
    zmq_close(requester);
    zmq_ctx_destroy(context);
}

void hwserver3()
{
    void *context = zmq_ctx_new();
    void *responder = zmq_socket(context, ZMQ_ROUTER);
    int rc = zmq_bind(responder, "tcp://*:5555");
    assert(rc == 0);
    (void)rc;

    while (1)
    {
        char *identity, *string, *content;
        // receive client request msg
        identity = s_recv(responder); // receive the first part is identity frame
        printf("[Frame_1] identity = %s\n", identity);
        // if the second part is empty frame, then continue receiving the third part
        string = s_recv(responder);
        if (NULL != string)
            printf("[Frame_2] string = %s\n", string);
        if (strcmp(string, "") == 0)
        {
            // receive the third part is the data frame
            content = s_recv(responder);
            printf("[Frame_3] Received request msg: %s\n", content);
            free(string);
            free(content);
        }
        // if the second part is not empty frame,then discard the whole ZMQ msg.
        else
        {
            printf("Discard the ZMQ message!\n");
            free(string);
            free(identity);
            continue;
        }
        sleep(1); // Do some work
        // send reply msg to client
        printf("Send respond msg: World\n\n");
        s_sendmore(responder, identity);    // send identity frame
        s_sendmore(responder, "");          // send empty frame
        zmq_send(responder, "World", 5, 0); // send data frame
        free(identity);
    }
    zmq_close(responder);
    zmq_ctx_destroy(context);
}

void hwclient3()
{
    printf("Connecting to hello world server...\n");
    void *context = zmq_ctx_new();
    void *requester = zmq_socket(context, ZMQ_REQ);
    // set REQ socket identity
    zmq_setsockopt(requester, ZMQ_IDENTITY, "ZMQ", strlen("ZMQ"));
    zmq_connect(requester, "tcp://localhost:5555");
    int request_nbr;
    for (request_nbr = 0; request_nbr < 10; request_nbr++)
    {
        char buffer[10] = {0};
        // send request msg
        printf("Sending request msg: Hello NO=%d...\n", request_nbr + 1);
        zmq_send(requester, "Hello", 5, 0);
        // receive respond msg
        zmq_recv(requester, buffer, 10, 0);
        printf("Received respond msg: %s NO=%d\n\n", buffer, request_nbr + 1);
    }
    zmq_close(requester);
    zmq_ctx_destroy(context);
}

void identity()
{
    //  Demonstrate request-reply identities
    void *context = zmq_ctx_new();
    void *sink = zmq_socket(context, ZMQ_ROUTER);
    zmq_bind(sink, "inproc://example");

    //  First allow 0MQ to set the identity
    void *anonymous = zmq_socket(context, ZMQ_REQ);
    zmq_connect(anonymous, "inproc://example");
    s_send(anonymous, "ROUTER uses a generated 5 byte identity");
    s_dump(sink);

    //  Then set the identity ourselves
    void *identified = zmq_socket(context, ZMQ_REQ);
    zmq_setsockopt(identified, ZMQ_IDENTITY, "PEER2", 5);
    zmq_connect(identified, "inproc://example");
    s_send(identified, "ROUTER socket uses REQ's socket identity");
    s_dump(sink);

    zmq_close(sink);
    zmq_close(anonymous);
    zmq_close(identified);
    zmq_ctx_destroy(context);
}

#define NBR_WORKERS 10

static void *req_worker_task(void *args)
{
    (void)args;
    void *context = zmq_ctx_new();
    void *worker = zmq_socket(context, ZMQ_REQ);

#if (defined(WIN32))
    s_set_id(worker, (intptr_t)args);
#else
    s_set_id(worker); //  Set a printable identity.
#endif

    zmq_connect(worker, "tcp://localhost:5671");

    // 该示例运行 5 秒钟,然后每个工作线程打印他们处理的任务数.如果路由有效,我们期望工作分配公平
    int total = 0;
    while (1)
    {
        //  Tell the broker we're ready for work
        s_send(worker, "Hi Boss");

        //  Get workload from broker, until finished
        char *workload = s_recv(worker);
        int finished = (strcmp(workload, "Fired!") == 0);
        free(workload);
        if (finished)
        {
            printf("Completed: %d tasks\n", total);
            break;
        }
        total++;

        //  Do some random work
        s_sleep(randof(500) + 1);
    }
    zmq_close(worker);
    zmq_ctx_destroy(context);
    return NULL;
}
void rtreq()
{
    //  .split main task
    //  While this example runs in a single process, that is only to make
    //  it easier to start and stop the example. Each thread has its own
    //  context and conceptually acts as a separate process.

    void *context = zmq_ctx_new();
    void *broker = zmq_socket(context, ZMQ_ROUTER);

    zmq_bind(broker, "tcp://*:5671");
    srandom((unsigned)time(NULL));

    // 要与此示例中的 worker 通信,我们必须创建一个 REQ 友好的信封,该信封由一个标识和一个空的信封分隔符框架组成
    int worker_nbr;
    for (worker_nbr = 0; worker_nbr < NBR_WORKERS; worker_nbr++)
    {
        pthread_t worker;
        pthread_create(&worker, NULL, req_worker_task, (void *)(intptr_t)worker_nbr);
    }
    //  Run for five seconds and then tell workers to end
    int64_t end_time = s_clock() + 5000;
    int workers_fired = 0;
    while (1)
    {
        //  Next message gives us least recently used worker
        char *identity = s_recv(broker);
        s_sendmore(broker, identity);
        free(identity);
        free(s_recv(broker)); //  Envelope delimiter
        free(s_recv(broker)); //  Response from worker
        s_sendmore(broker, "");

        //  Encourage workers until it's time to fire them
        if (s_clock() < end_time)
            s_send(broker, "Work harder");
        else
        {
            s_send(broker, "Fired!");
            if (++workers_fired == NBR_WORKERS)
                break;
        }
    }
    zmq_close(broker);
    zmq_ctx_destroy(context);
}

static void *dealer_worker_task(void *args)
{
    (void)args;
    void *context = zmq_ctx_new();
    void *worker = zmq_socket(context, ZMQ_DEALER);

#if (defined(WIN32))
    s_set_id(worker, (intptr_t)args);
#else
    s_set_id(worker); //  Set a printable identity
#endif

    zmq_connect(worker, "tcp://localhost:5671");

    /*
    代码几乎相同,只是 worker 使用 DEALER 套接字,并在数据帧之前读取和写入该空帧.
    当我想保持与 REQ worker 的兼容性时,我会使用这种方法.
    但是,请记住该空分隔符帧的原因:这是为了允许在 REP 套接字中终止的多跳扩展请求,该套接字使用该分隔符来拆分回复信封,以便它可以将数据帧传递给其应用程序.
    如果我们永远不需要将消息传递给 REP 套接字,我们可以简单地在两侧放置空的分隔符帧,这样事情就更简单了.这通常是我用于纯 DEALER 到 ROUTER 协议的设计.
    */
    int total = 0;
    while (1)
    {
        //  Tell the broker we're ready for work
        s_sendmore(worker, "");
        s_send(worker, "Hi Boss");

        //  Get workload from broker, until finished
        free(s_recv(worker)); //  Envelope delimiter
        char *workload = s_recv(worker);
        //  .skip
        int finished = (strcmp(workload, "Fired!") == 0);
        free(workload);
        if (finished)
        {
            printf("Completed: %d tasks\n", total);
            break;
        }
        total++;

        //  Do some random work
        s_sleep(randof(500) + 1);
    }
    zmq_close(worker);
    zmq_ctx_destroy(context);
    return NULL;
}

void rtdealer()
{
    /*
    只要您可以使用 REQ,就可以使用 DEALER.有两个具体区别:
        REQ 套接字始终在任何数据帧之前发送一个空的分隔符帧;DEALER 则不然.
        REQ 套接字在收到回复之前将只发送一条消息;DEALER 是完全异步的.
    */
    void *context = zmq_ctx_new();
    void *broker = zmq_socket(context, ZMQ_ROUTER);

    zmq_bind(broker, "tcp://*:5671");
    srandom((unsigned)time(NULL));

    int worker_nbr;
    for (worker_nbr = 0; worker_nbr < NBR_WORKERS; worker_nbr++)
    {
        pthread_t worker;
        pthread_create(&worker, NULL, dealer_worker_task, (void *)(intptr_t)worker_nbr);
    }
    //  Run for five seconds and then tell workers to end
    int64_t end_time = s_clock() + 5000;
    int workers_fired = 0;
    while (1)
    {
        //  Next message gives us least recently used worker
        char *identity = s_recv(broker);
        s_sendmore(broker, identity);
        free(identity);
        free(s_recv(broker)); //  Envelope delimiter
        free(s_recv(broker)); //  Response from worker
        s_sendmore(broker, "");

        //  Encourage workers until it's time to fire them
        if (s_clock() < end_time)
            s_send(broker, "Work harder");
        else
        {
            s_send(broker, "Fired!");
            if (++workers_fired == NBR_WORKERS)
                break;
        }
    }
    zmq_close(broker);
    zmq_ctx_destroy(context);
}

#define LBROKER_NBR_WORKERS 3
#define LBROKER_NBR_CLIENTS 10

//  Dequeue operation for queue implemented as array of anything
#define DEQUEUE(q) memmove(&(q)[0], &(q)[1], sizeof(q) - sizeof(q[0]))

//  Basic request-reply client using REQ socket
//  Because s_send and s_recv can't handle 0MQ binary identities, we
//  set a printable text identity to allow routing.
//
static void *lbbroker_client_task(void *args)
{
    (void)args;
    void *context = zmq_ctx_new();
    void *client = zmq_socket(context, ZMQ_REQ);
    // 客户端发送 [empty][request]

#if (defined(WIN32))
    s_set_id(client, (intptr_t)args);
    zmq_connect(client, "tcp://localhost:5672"); // frontend
#else
    s_set_id(client); // Set a printable identity
    zmq_connect(client, "ipc://frontend.ipc");
#endif

    //  Send request, get reply
    s_send(client, "HELLO");
    char *reply = s_recv(client);
    printf("Client: %s\n", reply);
    free(reply);
    zmq_close(client);
    zmq_ctx_destroy(context);
    return NULL;
}

//  .split worker task
//  While this example runs in a single process, that is just to make
//  it easier to start and stop the example. Each thread has its own
//  context and conceptually acts as a separate process.
//  This is the worker task, using a REQ socket to do load-balancing.
//  Because s_send and s_recv can't handle 0MQ binary identities, we
//  set a printable text identity to allow routing.

static void *lbbroker_worker_task(void *args)
{
    (void)args;
    void *context = zmq_ctx_new();
    void *worker = zmq_socket(context, ZMQ_REQ);

#if (defined(WIN32))
    s_set_id(worker, (intptr_t)args);
    zmq_connect(worker, "tcp://localhost:5673"); // backend
#else
    s_set_id(worker);
    zmq_connect(worker, "ipc://backend.ipc");
#endif

    //  Tell broker we're ready for work
    s_send(worker, "READY");

    while (1)
    {
        // recv from backend [identity][empty][request]
        //  Read and save all frames until we get an empty frame
        //  In this example there is only 1, but there could be more
        char *identity = s_recv(worker);
        char *empty = s_recv(worker);
        assert(*empty == 0);
        free(empty);

        //  Get request, send reply
        char *request = s_recv(worker);
        printf("Worker: %s\n", request);
        free(request);

        // resp to worker [identity][empty][request]
        s_sendmore(worker, identity);
        s_sendmore(worker, "");
        s_send(worker, "OK");
        free(identity);
    }
    zmq_close(worker);
    zmq_ctx_destroy(context);
    return NULL;
}

void lbbroker()
{
    //  Prepare our context and sockets
    void *context = zmq_ctx_new();
    void *frontend = zmq_socket(context, ZMQ_ROUTER);
    void *backend = zmq_socket(context, ZMQ_ROUTER);

#if (defined(WIN32))
    zmq_bind(frontend, "tcp://*:5672"); // frontend
    zmq_bind(backend, "tcp://*:5673");  // backend
#else
    zmq_bind(frontend, "ipc://frontend.ipc");
    zmq_bind(backend, "ipc://backend.ipc");
#endif

    int client_nbr;
    for (client_nbr = 0; client_nbr < LBROKER_NBR_CLIENTS; client_nbr++)
    {
        pthread_t client;
        pthread_create(&client, NULL, lbbroker_client_task, (void *)(intptr_t)client_nbr);
    }
    int worker_nbr;
    for (worker_nbr = 0; worker_nbr < LBROKER_NBR_WORKERS; worker_nbr++)
    {
        pthread_t worker;
        pthread_create(&worker, NULL, lbbroker_worker_task, (void *)(intptr_t)worker_nbr);
    }
    //  .split main task body
    //  Here is the main loop for the least-recently-used queue. It has two
    //  sockets; a frontend for clients and a backend for workers. It polls
    //  the backend in all cases, and polls the frontend only when there are
    //  one or more workers ready. This is a neat way to use 0MQ's own queues
    //  to hold messages we're not ready to process yet. When we get a client
    //  request, we pop the next available worker and send the request to it,
    //  including the originating client identity. When a worker replies, we
    //  requeue that worker and forward the reply to the original client
    //  using the reply envelope.

    //  Queue of available workers
    int available_workers = 0;
    char *worker_queue[10];

    while (1)
    {
        zmq_pollitem_t items[] = {
            {backend,  0, ZMQ_POLLIN, 0},
            {frontend, 0, ZMQ_POLLIN, 0}
        };
        //  Poll frontend only if we have available workers
        int rc = zmq_poll(items, available_workers ? 2 : 1, -1);
        if (rc == -1)
            break; //  Interrupted

        //  Handle worker activity on backend
        if (items[0].revents & ZMQ_POLLIN)
        {
            // recv from frontend [Worker][empty][identity][empty][request]
            //  Queue worker identity for load-balancing
            char *worker_id = s_recv(backend);
            assert(available_workers < NBR_WORKERS);
            worker_queue[available_workers++] = worker_id;

            //  Second frame is empty
            char *empty = s_recv(backend);
            assert(empty[0] == 0);
            free(empty);

            //  Third frame is READY or else a client reply identity
            char *client_id = s_recv(backend);

            //  If client reply, send rest back to frontend
            if (strcmp(client_id, "READY") != 0)
            {
                empty = s_recv(backend);
                assert(empty[0] == 0);
                free(empty);
                char *reply = s_recv(backend);

                // send to worker [identity][empty][request]
                s_sendmore(frontend, client_id);
                s_sendmore(frontend, "");
                s_send(frontend, reply);
                free(reply);
                if (--client_nbr == 0)
                    break; //  Exit after N messages
            }
            free(client_id);
        }
        //  .split handling a client request
        //  Here is how we handle a client request:

        if (items[1].revents & ZMQ_POLLIN)
        {
            //  Now get next client request, route to last-used worker
            //  Client request is [identity][empty][request]
            char *client_id = s_recv(frontend);
            char *empty = s_recv(frontend);
            assert(empty[0] == 0);
            free(empty);
            char *request = s_recv(frontend);

            // send to backend [Worker][empty][identity][empty][request]
            s_sendmore(backend, worker_queue[0]);
            s_sendmore(backend, "");
            s_sendmore(backend, client_id);
            s_sendmore(backend, "");
            s_send(backend, request);

            free(client_id);
            free(request);

            //  Dequeue and drop the next worker identity
            free(worker_queue[0]);
            DEQUEUE(worker_queue);
            available_workers--;
        }
    }
    zmq_close(frontend);
    zmq_close(backend);
    zmq_ctx_destroy(context);
}

void hwserver4()
{
    void *context = zmq_ctx_new();
    void *responder = zmq_socket(context, ZMQ_ROUTER);
    int rc = zmq_bind(responder, "tcp://*:5555");
    assert(rc == 0);
    (void)rc;

    while (1)
    {
        char identity[10] = {0};
        // recv client`s request msg
        // the 1st received frame is identity frame
        char *frame1 = s_recv(responder);
        printf("[recv] frame1 = %s\n", frame1);
        memcpy(identity, frame1, strlen(frame1) + 1); // save the identity to char array identity
        free(frame1);
        // the 2nd frame received is empty delimiter frame
        char *frame2 = s_recv(responder);
        printf("[recv] frame2 = %s\n", frame2);
        assert(frame2[0] == 0);
        free(frame2);
        // the 3rd frame received is data frame
        char *frame3 = s_recv(responder);
        printf("[recv] frame3 = %s\n", frame3);
        free(frame3);

        sleep(1); // Do some work

        // send respond msg to client
        printf("[send] frame1 = %s\n", identity);
        printf("[send] frame2 = %s\n", "");
        printf("[send] frame3 = %s\n\n", "World");
        s_sendmore(responder, identity);       // send identity frame
        s_sendmore(responder, "");             // send empty delimiter frame
        s_send(responder, "World");            // send data frame
        memset(identity, 0, sizeof(identity)); // clear up identity array
    }

    zmq_close(responder);
    zmq_ctx_destroy(context);
}

void hwclient4()
{
    printf("Connecting to hello world server...\n");
    void *context = zmq_ctx_new();
    void *requester = zmq_socket(context, ZMQ_DEALER);
    // set socket identity
    zmq_setsockopt(requester, ZMQ_IDENTITY, "ZMQ", strlen("ZMQ"));
    zmq_connect(requester, "tcp://localhost:5555");
    int request_nbr;
    for (request_nbr = 0; request_nbr < 10; request_nbr++)
    {
        printf("Sending request msg: Hello NO=%d...\n", request_nbr + 1);
        // send request msg
        s_sendmore(requester, "");  // send empty delimiter frame
        s_send(requester, "Hello"); // send data frame
        // recv reply msg
        char *empty = s_recv(requester);
        assert(empty[0] == 0);
        (void)empty;
        char *reply = s_recv(requester);
        if (NULL != reply)
            printf("Received respond msg: %s NO=%d\n\n", reply, request_nbr + 1);
        free(reply);
    }

    zmq_close(requester);
    zmq_ctx_destroy(context);
}

void rrworker()
{
    void *context = zmq_ctx_new();

    //  Socket to talk to clients
    void *responder = zmq_socket(context, ZMQ_REP);
    zmq_connect(responder, "tcp://localhost:5560");

    while (1)
    {
        //  Wait for next request from client
        char *string = s_recv(responder);
        printf("Received request: [%s]\n", string);
        free(string);

        //  Do some 'work'
        sleep(1);

        //  Send reply back to client
        s_send(responder, "World");
    }
    //  We never get here, but clean up anyhow
    zmq_close(responder);
    zmq_ctx_destroy(context);
}

void rrclient()
{
    void *context = zmq_ctx_new();

    //  Socket to talk to server
    void *requester = zmq_socket(context, ZMQ_REQ);
    zmq_connect(requester, "tcp://localhost:5559");

    int request_nbr;
    for (request_nbr = 0; request_nbr != 10; request_nbr++)
    {
        s_send(requester, "Hello");
        char *string = s_recv(requester);
        if (NULL != string)
            printf("Received reply %d [%s]\n", request_nbr, string);
        free(string);
    }
    zmq_close(requester);
    zmq_ctx_destroy(context);
}

void rrbroker()
{
    //  Prepare our context and sockets
    void *context = zmq_ctx_new();
    void *frontend = zmq_socket(context, ZMQ_ROUTER);
    void *backend = zmq_socket(context, ZMQ_DEALER);
    zmq_bind(frontend, "tcp://*:5559");
    zmq_bind(backend, "tcp://*:5560");

    //  Initialize poll set
    zmq_pollitem_t items[] = {
        {frontend, 0, ZMQ_POLLIN, 0},
        {backend,  0, ZMQ_POLLIN, 0}
    };
    //  Switch messages between sockets
    while (1)
    {
        zmq_msg_t message;
        zmq_poll(items, 2, -1);
        if (items[0].revents & ZMQ_POLLIN)
        {
            while (1)
            {
                //  Process all parts of the message
                zmq_msg_init(&message);
                zmq_msg_recv(&message, frontend, 0);
                int more = zmq_msg_more(&message);
                zmq_msg_send(&message, backend, more ? ZMQ_SNDMORE : 0);
                zmq_msg_close(&message);
                if (!more)
                    break; //  Last message part
            }
        }
        if (items[1].revents & ZMQ_POLLIN)
        {
            while (1)
            {
                //  Process all parts of the message
                zmq_msg_init(&message);
                zmq_msg_recv(&message, backend, 0);
                int more = zmq_msg_more(&message);
                zmq_msg_send(&message, frontend, more ? ZMQ_SNDMORE : 0);
                zmq_msg_close(&message);
                if (!more)
                    break; //  Last message part
            }
        }
    }
    //  We never get here, but clean up anyhow
    zmq_close(frontend);
    zmq_close(backend);
    zmq_ctx_destroy(context);
}

void msgqueue()
{
    void *context = zmq_ctx_new();

    //  Socket facing clients
    void *frontend = zmq_socket(context, ZMQ_ROUTER);
    int rc = zmq_bind(frontend, "tcp://*:5559");
    assert(rc == 0);
    (void)rc;

    //  Socket facing services
    void *backend = zmq_socket(context, ZMQ_DEALER);
    rc = zmq_bind(backend, "tcp://*:5560");
    assert(rc == 0);

    //  Start the proxy
    zmq_proxy(frontend, backend, NULL);

    //  We never get here...
    zmq_close(frontend);
    zmq_close(backend);
    zmq_ctx_destroy(context);
}

static void *worker_routine(void *context)
{
    //  Socket to talk to dispatcher
    void *receiver = zmq_socket(context, ZMQ_REP);
    zmq_connect(receiver, "inproc://workers");

    while (1)
    {
        char *string = s_recv(receiver);
        printf("Received request: [%s]\n", string);
        free(string);
        //  Do some 'work'
        sleep(1);
        //  Send reply back to client
        s_send(receiver, "World");
    }
    zmq_close(receiver);
    return NULL;
}

void mtserver()
{
    void *context = zmq_ctx_new();

    //  Socket to talk to clients
    void *clients = zmq_socket(context, ZMQ_ROUTER);
    zmq_bind(clients, "tcp://*:5555");

    //  Socket to talk to workers
    void *workers = zmq_socket(context, ZMQ_DEALER);
    zmq_bind(workers, "inproc://workers");

    //  Launch pool of worker threads
    int thread_nbr;
    for (thread_nbr = 0; thread_nbr < 5; thread_nbr++)
    {
        pthread_t worker;
        pthread_create(&worker, NULL, worker_routine, context);
    }
    //  Connect work threads to client threads via a queue proxy
    zmq_proxy(clients, workers, NULL);

    //  We never get here, but clean up anyhow
    zmq_close(clients);
    zmq_close(workers);
    zmq_ctx_destroy(context);
}

void wuserver()
{
    //  Weather update server
    //  Binds PUB socket to tcp://*:5556
    //  Publishes random weather updates

    // 从 ZeroMQ v3.x 开始,当使用连接协议(tcp:@<>@ 或 ipc:@<>@)时,过滤发生在发布者端.使用 epgm:@<//>@ 协议,过滤发生在订阅者端. 如果发布者没有连接的订阅者,则它只会丢弃所有消息
    // 在 ZeroMQ v2.x 中,所有过滤都发生在订阅者端.

    //  Prepare our context and publisher
    void *context = zmq_ctx_new();
    void *publisher = zmq_socket(context, ZMQ_PUB);
    int rc = zmq_bind(publisher, "tcp://*:5556");
    assert(rc == 0);
    (void)rc;

    //  Initialize random number generator
    srandom((unsigned)time(NULL));
    while (1)
    {
        //  Get values that will fool the boss
        int zipcode, temperature, relhumidity;
        zipcode = randof(100000);
        temperature = randof(215) - 80;
        relhumidity = randof(50) + 10;

        //  Send message to all subscribers
        char update[128];
        sprintf(update, "%05d %d %d", zipcode, temperature, relhumidity);
        printf("pub[%s]\n", update);
        s_send(publisher, update);
        sleep(1);
    }
    zmq_close(publisher);
    zmq_ctx_destroy(context);
}

void wuclient()
{
    //  Weather update client
    //  Connects SUB socket to tcp://localhost:5556
    //  Collects weather updates and finds avg temp in zipcode

    // “慢速加入” 症状
    // 订阅服务器也总是会错过发布服务器发送的第一条消息.
    // 这是因为当订阅者连接到发布者时(这需要一小段时间但非零),发布者可能已经在发送消息.
    //  Socket to talk to server
    printf("Collecting updates from weather server...\n");
    void *context = zmq_ctx_new();
    void *subscriber = zmq_socket(context, ZMQ_SUB);
    int rc = zmq_connect(subscriber, "tcp://localhost:5556");
    assert(rc == 0);
    (void)rc;

    // 如果您未设置任何订阅,则不会收到任何消息.
    // 订阅者可以设置多个订阅,这些订阅将相加.
    // 订阅者还可以取消特定订阅.
    //  Subscribe to zipcode
#if 0
    const char *filter = "10001 ";
    rc = zmq_setsockopt(subscriber, ZMQ_SUBSCRIBE, filter, strlen(filter));
    assert(rc == 0);
#endif

    const char *filter = "";
    rc = zmq_setsockopt(subscriber, ZMQ_SUBSCRIBE, filter, strlen(filter));
    assert(rc == 0);

    //  Process 100 updates
    int update_nbr;
    long total_temp = 0;
    for (update_nbr = 0; update_nbr < 100; update_nbr++)
    {
        char *string = s_recv(subscriber);

        printf("received[%s]\n", string);
        int zipcode, temperature, relhumidity;
        sscanf(string, "%d %d %d", &zipcode, &temperature, &relhumidity);
        total_temp += temperature;
        free(string);
    }
    printf("Average temperature for zipcode '%s' was %dF\n",
           filter, (int)(total_temp / update_nbr));

    zmq_close(subscriber);
    zmq_ctx_destroy(context);
}

#define SUBSCRIBERS_EXPECTED 10 //  We wait for 10 subscribers

void syncsub()
{
    void *context = zmq_ctx_new();

    //  First, connect our subscriber socket
    void *subscriber = zmq_socket(context, ZMQ_SUB);
    zmq_connect(subscriber, "tcp://localhost:5561");
    zmq_setsockopt(subscriber, ZMQ_SUBSCRIBE, "", 0);

    //  0MQ is so fast, we need to wait a while...
    sleep(1);

    //  Second, synchronize with publisher
    void *syncclient = zmq_socket(context, ZMQ_REQ);
    zmq_connect(syncclient, "tcp://localhost:5562");

    //  - send a synchronization request
    s_send(syncclient, "");

    //  - wait for synchronization reply
    char *string = s_recv(syncclient);
    free(string);

    //  Third, get our updates and report how many we got
    int update_nbr = 0;
    while (1)
    {
        char *string = s_recv(subscriber);
        if (strcmp(string, "END") == 0)
        {
            free(string);
            break;
        }
        free(string);
        update_nbr++;
    }
    printf("Received %d updates\n", update_nbr);

    zmq_close(subscriber);
    zmq_close(syncclient);
    zmq_ctx_destroy(context);
}

void syncpub()
{
    void *context = zmq_ctx_new();

    //  Socket to talk to clients
    void *publisher = zmq_socket(context, ZMQ_PUB);

    int sndhwm = 1100000;
    zmq_setsockopt(publisher, ZMQ_SNDHWM, &sndhwm, sizeof(int));

    zmq_bind(publisher, "tcp://*:5561");

    //  Socket to receive signals
    void *syncservice = zmq_socket(context, ZMQ_REP);
    zmq_bind(syncservice, "tcp://*:5562");

    //  Get synchronization from subscribers
    printf("Waiting for subscribers\n");
    int subscribers = 0;
    while (subscribers < SUBSCRIBERS_EXPECTED)
    {
        //  - wait for synchronization request
        char *string = s_recv(syncservice);
        free(string);
        //  - send synchronization reply
        s_send(syncservice, "");
        subscribers++;
    }
    //  Now broadcast exactly 1M updates followed by END
    printf("Broadcasting messages\n");
    int update_nbr;
    for (update_nbr = 0; update_nbr < 1000000; update_nbr++)
        s_send(publisher, "Rhubarb");

    s_send(publisher, "END");

    zmq_close(publisher);
    zmq_close(syncservice);
    zmq_ctx_destroy(context);
}

void psenvsub()
{
    //  Prepare our context and subscriber
    void *context = zmq_ctx_new();
    void *subscriber = zmq_socket(context, ZMQ_SUB);
    zmq_connect(subscriber, "tcp://localhost:5563");
    zmq_setsockopt(subscriber, ZMQ_SUBSCRIBE, "B", 1);

    while (1)
    {
        //  Read envelope with address
        char *address = s_recv(subscriber);

        //  Read message contents
        char *contents = s_recv(subscriber);
        printf("[%s] %s\n", address, contents);
        free(address);
        free(contents);
    }
    //  We never get here, but clean up anyhow
    zmq_close(subscriber);
    zmq_ctx_destroy(context);
}

void psenvpub()
{
    //  Prepare our context and publisher
    void *context = zmq_ctx_new();
    void *publisher = zmq_socket(context, ZMQ_PUB);
    zmq_bind(publisher, "tcp://*:5563");

    while (1)
    {
        //  Write two messages, each with an envelope and content
        s_sendmore(publisher, "A");
        s_send(publisher, "We don't want to see this");

        s_sendmore(publisher, "B");
        s_send(publisher, "We would like to see this");
        sleep(1);
    }
    //  We never get here, but clean up anyhow
    zmq_close(publisher);
    zmq_ctx_destroy(context);
}

void wuproxy()
{
    void *context = zmq_ctx_new();

    //  This is where the weather server sits
    void *frontend = zmq_socket(context, ZMQ_XSUB);
    zmq_connect(frontend, "tcp://192.168.55.210:5556");

    //  This is our public endpoint for subscribers
    void *backend = zmq_socket(context, ZMQ_XPUB);
    zmq_bind(backend, "tcp://10.1.1.0:8100");

    //  Run the proxy until the user interrupts us
    zmq_proxy(frontend, backend, NULL);

    zmq_close(frontend);
    zmq_close(backend);
    zmq_ctx_destroy(context);
}

void taskvent()
{
    //  Task ventilator
    //  Binds PUSH socket to tcp://localhost:5557
    //  Sends batch of tasks to workers via that socket

    void *context = zmq_ctx_new();

    //  Socket to send messages on
    void *sender = zmq_socket(context, ZMQ_PUSH);
    zmq_bind(sender, "tcp://*:5557");

    //  Socket to send start of batch message on
    void *sink = zmq_socket(context, ZMQ_PUSH);
    zmq_connect(sink, "tcp://localhost:5558");

    printf("Press Enter when the workers are ready: ");
    getchar();
    printf("Sending tasks to workers...\n");

    //  The first message is "0" and signals start of batch
    s_send(sink, "0");

    //  Initialize random number generator
    srandom((unsigned)time(NULL));

    //  Send 100 tasks
    int task_nbr;
    int total_msec = 0; //  Total expected cost in msecs
    for (task_nbr = 0; task_nbr < 100; task_nbr++)
    {
        int workload;
        //  Random workload from 1 to 100msecs
        workload = randof(100) + 1;
        total_msec += workload;
        char string[128];
        sprintf(string, "%d", workload);
        s_send(sender, string);
    }
    printf("Total expected cost: %d msec\n", total_msec);

    zmq_close(sink);
    zmq_close(sender);
    zmq_ctx_destroy(context);
}

void taskwork()
{
    //  Task worker
    //  Connects PULL socket to tcp://localhost:5557
    //  Collects workloads from ventilator via that socket
    //  Connects PUSH socket to tcp://localhost:5558
    //  Sends results to sink via that socket

    //  Socket to receive messages on
    void *context = zmq_ctx_new();
    void *receiver = zmq_socket(context, ZMQ_PULL);
    zmq_connect(receiver, "tcp://localhost:5557");

    //  Socket to send messages to
    void *sender = zmq_socket(context, ZMQ_PUSH);
    zmq_connect(sender, "tcp://localhost:5558");

    //  Process tasks forever
    while (1)
    {
        char *string = s_recv(receiver);
        printf("%s.\n", string); //  Show progress
        fflush(stdout);
        s_sleep(atoi(string)); //  Do the work
        free(string);
        s_send(sender, ""); //  Send results to sink
    }
    zmq_close(receiver);
    zmq_close(sender);
    zmq_ctx_destroy(context);
}

void tasksink()
{
    //  Task sink
    //  Binds PULL socket to tcp://localhost:5558
    //  Collects results from workers via that socket

    //  Prepare our context and socket
    void *context = zmq_ctx_new();
    void *receiver = zmq_socket(context, ZMQ_PULL);
    zmq_bind(receiver, "tcp://*:5558");

    //  Wait for start of batch
    char *string = s_recv(receiver);
    printf("start to sink\n");
    free(string);

    //  Start our clock now
    int64_t start_time = s_clock();

    //  Process 100 confirmations
    int task_nbr;
    for (task_nbr = 0; task_nbr < 100; task_nbr++)
    {
        char *string = s_recv(receiver);
        free(string);
        if (task_nbr % 10 == 0)
            printf(":\n");
        else
            printf(".\n");
        fflush(stdout);
    }
    //  Calculate and report duration of batch
    printf("Total elapsed time: %d msec\n",
           (int)(s_clock() - start_time));

    zmq_close(receiver);
    zmq_ctx_destroy(context);
}

void taskwork2()
{
    //  Task worker - design 2
    //  Adds pub-sub flow to receive and respond to kill signal

    //  Socket to receive messages on
    void *context = zmq_ctx_new();
    void *receiver = zmq_socket(context, ZMQ_PULL);
    zmq_connect(receiver, "tcp://localhost:5557");

    //  Socket to send messages to
    void *sender = zmq_socket(context, ZMQ_PUSH);
    zmq_connect(sender, "tcp://localhost:5558");

    //  Socket for control input
    void *controller = zmq_socket(context, ZMQ_SUB);
    zmq_connect(controller, "tcp://localhost:5559");
    zmq_setsockopt(controller, ZMQ_SUBSCRIBE, "", 0);

    //  Process messages from either socket
    while (1)
    {
        zmq_pollitem_t items[] = {
            {receiver,   0, ZMQ_POLLIN, 0},
            {controller, 0, ZMQ_POLLIN, 0}
        };
        zmq_poll(items, 2, -1);
        if (items[0].revents & ZMQ_POLLIN)
        {
            char *string = s_recv(receiver);
            printf("%s.", string); //  Show progress
            fflush(stdout);
            s_sleep(atoi(string)); //  Do the work
            free(string);
            s_send(sender, ""); //  Send results to sink
        }
        //  Any waiting controller command acts as 'KILL'
        if (items[1].revents & ZMQ_POLLIN)
            break; //  Exit loop
    }
    zmq_close(receiver);
    zmq_close(sender);
    zmq_close(controller);
    zmq_ctx_destroy(context);
}

void tasksink2()
{
    //  Task sink - design 2
    //  Adds pub-sub flow to send kill signal to workers

    //  Socket to receive messages on
    void *context = zmq_ctx_new();
    void *receiver = zmq_socket(context, ZMQ_PULL);
    zmq_bind(receiver, "tcp://*:5558");

    //  Socket for worker control
    void *controller = zmq_socket(context, ZMQ_PUB);
    zmq_bind(controller, "tcp://*:5559");

    //  Wait for start of batch
    char *string = s_recv(receiver);
    free(string);

    //  Start our clock now
    int64_t start_time = s_clock();

    //  Process 100 confirmations
    int task_nbr;
    for (task_nbr = 0; task_nbr < 100; task_nbr++)
    {
        char *string = s_recv(receiver);
        free(string);
        if (task_nbr % 10 == 0)
            printf(":");
        else
            printf(".");
        fflush(stdout);
    }
    printf("Total elapsed time: %d msec\n",
           (int)(s_clock() - start_time));

    //  Send kill signal to workers
    s_send(controller, "KILL");

    zmq_close(receiver);
    zmq_close(controller);
    zmq_ctx_destroy(context);
}

void msreader()
{
    // 既可以作为天气更新的订阅者,也可以作为并行任务的 worker
    //  Connect to task ventilator
    void *context = zmq_ctx_new();
    void *receiver = zmq_socket(context, ZMQ_PULL);
    zmq_connect(receiver, "tcp://localhost:5557");

    //  Connect to weather server
    void *subscriber = zmq_socket(context, ZMQ_SUB);
    zmq_connect(subscriber, "tcp://localhost:5556");
    zmq_setsockopt(subscriber, ZMQ_SUBSCRIBE, "10001 ", 6);

    //  Process messages from both sockets
    //  We prioritize traffic from the task ventilator
    while (1)
    {
        char msg[256];
        while (1)
        {
            int size = zmq_recv(receiver, msg, 255, ZMQ_DONTWAIT);
            if (size != -1)
            {
                //  Process task
            }
            else
                break;
        }

        while (1)
        {
            int size = zmq_recv(subscriber, msg, 255, ZMQ_DONTWAIT);
            if (size != -1)
            {
                //  Process weather update
            }
            else
                break;
        }
        //  No activity, so sleep for 1 msec
        s_sleep(1);
    }
    zmq_close(receiver);
    zmq_close(subscriber);
    zmq_ctx_destroy(context);
}

void mspoller()
{
    //  Reading from multiple sockets
    //  This version uses zmq_poll()

    //  Connect to task ventilator
    void *context = zmq_ctx_new();
    void *receiver = zmq_socket(context, ZMQ_PULL);
    zmq_connect(receiver, "tcp://localhost:5557");

    //  Connect to weather server
    void *subscriber = zmq_socket(context, ZMQ_SUB);
    zmq_connect(subscriber, "tcp://localhost:5556");
    zmq_setsockopt(subscriber, ZMQ_SUBSCRIBE, "10001 ", 6);

    zmq_pollitem_t items[] = {
        {receiver,   0, ZMQ_POLLIN, 0},
        {subscriber, 0, ZMQ_POLLIN, 0}
    };
    //  Process messages from both sockets
    while (1)
    {
        char msg[256];

        zmq_poll(items, 2, -1);
        if (items[0].revents & ZMQ_POLLIN)
        {
            int size = zmq_recv(receiver, msg, 255, 0);
            if (size != -1)
            {
                //  Process task
            }
        }
        if (items[1].revents & ZMQ_POLLIN)
        {
            int size = zmq_recv(subscriber, msg, 255, 0);
            if (size != -1)
            {
                //  Process weather update
            }
        }
    }
    zmq_close(subscriber);
    zmq_ctx_destroy(context);
}

//  Signal handling
//
//  Create a self-pipe and call s_catch_signals(pipe's writefd) in your application
//  at startup, and then exit your main loop if your pipe contains any data.
//  Works especially well with zmq_poll.

#define S_NOTIFY_MSG " "
#define S_ERROR_MSG  "Error while writing to self-pipe.\n"
static int s_fd;
static void s_signal_handler(int signal_value)
{
    (void)signal_value;
    ssize_t rc = write(s_fd, S_NOTIFY_MSG, sizeof(S_NOTIFY_MSG));
    if (rc != sizeof(S_NOTIFY_MSG))
    {
        rc = write(STDOUT_FILENO, S_ERROR_MSG, sizeof(S_ERROR_MSG) - 1);
        printf("write rc[%zd]\n", rc);
        exit(1);
    }
}

static void s_catch_signals(int fd)
{
    s_fd = fd;

    struct sigaction action;
    action.sa_handler = s_signal_handler;
    //  Doesn't matter if SA_RESTART set because self-pipe will wake up zmq_poll
    //  But setting to 0 will allow zmq_read to be interrupted.
    action.sa_flags = 0;
    sigemptyset(&action.sa_mask);
    sigaction(SIGINT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);
}

void interrupt()
{
    ssize_t rc = -1;

    void *context = zmq_ctx_new();
    void *socket = zmq_socket(context, ZMQ_REP);
    zmq_bind(socket, "tcp://*:5555");

    int pipefds[2];
    rc = pipe(pipefds);
    if (rc != 0)
    {
        perror("Creating self-pipe");
        exit(1);
    }
    for (int i = 0; i < 2; i++)
    {
        int flags = fcntl(pipefds[i], F_GETFL, 0);
        if (flags < 0)
        {
            perror("fcntl(F_GETFL)");
            exit(1);
        }
        rc = fcntl(pipefds[i], F_SETFL, flags | O_NONBLOCK);
        if (rc != 0)
        {
            perror("fcntl(F_SETFL)");
            exit(1);
        }
    }

    s_catch_signals(pipefds[1]);

    zmq_pollitem_t items[] = {
        {0,      pipefds[0], ZMQ_POLLIN, 0},
        {socket, 0,          ZMQ_POLLIN, 0}
    };

    while (1)
    {
        rc = zmq_poll(items, 2, -1);
        if (rc == 0)
        {
            continue;
        }
        else if (rc < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            perror("zmq_poll");
            exit(1);
        }

        // Signal pipe FD
        if (items[0].revents & ZMQ_POLLIN)
        {
            char buffer[1];
            rc = read(pipefds[0], buffer, 1); // clear notifying byte
            printf("write rc[%zd]\n", rc);
            printf("W: interrupt received, killing server...\n");
            break;
        }

        // Read socket
        if (items[1].revents & ZMQ_POLLIN)
        {
            char buffer[255];
            // Use non-blocking so we can continue to check self-pipe via zmq_poll
            rc = zmq_recv(socket, buffer, 255, ZMQ_DONTWAIT);
            if (rc < 0)
            {
                if (errno == EAGAIN)
                {
                    continue;
                }
                if (errno == EINTR)
                {
                    continue;
                }
                perror("recv");
                exit(1);
            }
            printf("W: recv\n");

            // Now send message back.
            // ...
        }
    }

    printf("W: cleaning up\n");
    zmq_close(socket);
    zmq_ctx_destroy(context);
}

static void *step1(void *context)
{
    //  Connect to step2 and tell it we're ready
    void *xmitter = zmq_socket(context, ZMQ_PAIR);
    zmq_connect(xmitter, "inproc://step2");
    printf("Step 1 ready, signaling step 2\n");
    s_send(xmitter, "READY");
    zmq_close(xmitter);

    return NULL;
}

static void *step2(void *context)
{
    //  Bind inproc socket before starting step1
    void *receiver = zmq_socket(context, ZMQ_PAIR);
    zmq_bind(receiver, "inproc://step2");
    pthread_t thread;
    pthread_create(&thread, NULL, step1, context);

    //  Wait for signal and pass it on
    char *string = s_recv(receiver);
    free(string);
    zmq_close(receiver);

    //  Connect to step3 and tell it we're ready
    void *xmitter = zmq_socket(context, ZMQ_PAIR);
    zmq_connect(xmitter, "inproc://step3");
    printf("Step 2 ready, signaling step 3\n");
    s_send(xmitter, "READY");
    zmq_close(xmitter);

    return NULL;
}

void mtrelay()
{
    void *context = zmq_ctx_new();

    //  Bind inproc socket before starting step2
    void *receiver = zmq_socket(context, ZMQ_PAIR);
    zmq_bind(receiver, "inproc://step3");
    pthread_t thread;
    pthread_create(&thread, NULL, step2, context);

    //  Wait for signal
    char *string = s_recv(receiver);
    free(string);
    zmq_close(receiver);

    printf("Test successful!\n");
    zmq_ctx_destroy(context);
}
