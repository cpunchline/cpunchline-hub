#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <fcntl.h>
#include <pthread.h>

#include "zmq.h"
#include "zhelpers.h"

// ZeroMQ的优势
// 它在后台线程中异步处理 I/O.这些应用程序使用无锁数据结构与应用程序线程通信, 因此并发 ZeroMQ 应用程序不需要锁,信号量或其他等待状态.
// 组件可以动态地来来去去, ZeroMQ 会自动重新连接.这意味着您可以按任何顺序启动组件.您可以创建"面向服务的架构"(SOA), 其中服务可以随时加入和离开网络.
// 它会在需要时自动将消息排队.它智能地执行此作, 在将消息排队之前将消息推送到尽可能靠近接收者的位置.
// 它有处理超满队列(称为"高水位线")的方法.当队列已满时, ZeroMQ 会自动阻止发送者或丢弃消息, 具体取决于您正在进行的消息传递类型(所谓的"模式").
// 它允许您的应用程序通过任意传输方式相互通信:TCP,多播,进程内,进程间.您无需更改代码即可使用不同的传输方式.
// 它使用取决于消息收发模式的不同策略安全地处理速度缓慢/阻塞的读取器.
// 它允许您使用各种模式(如 request-reply 和 pub-sub)路由消息.这些模式是您创建拓扑和网络结构的方式.
// 它允许您创建代理, 以便通过单个调用对消息进行排队,转发或捕获消息.代理可以降低网络互连的复杂性.
// 它使用网络上的简单框架, 完全按照发送的方式传递整个消息.如果您编写了一条 10k 的消息, 您将收到一条 10k 的消息.
// 它不会对消息施加任何格式.它们是从 0 到 GB 大小的 blob.当你想表示数据时, 你可以选择一些其他产品, 比如 msgpack,Google 的协议缓冲区等.
// 它通过在合理的情况下自动重试来智能地处理网络错误.
// 它减少了您的碳足迹.用更少的 CPU 做更多的事情意味着您的盒子消耗更少的电量, 并且您可以让旧盒子使用更长时间.Al Gore 会喜欢 ZeroMQ.

// 使用 ZeroMQ 编写令人满意的多线程代码:
/*
在其线程中私下隔离数据,并且从不在多个线程中共享数据.唯一的例外是 ZeroMQ 上下文,它是线程安全的.
远离经典的并发机制,如互斥锁,关键部分,信号量等.这些是 ZeroMQ 应用程序中的反模式.
在进程开始时创建一个 ZeroMQ 上下文,并将其传递给你想通过 inproc 套接字连接的所有线程.
使用附加线程在应用程序中创建结构,并使用 inproc 上的 PAIR 套接字将这些线程连接到它们的父线程.模式是:绑定父 socket,然后创建连接其 socket 的 child thread.
使用分离的线程来模拟具有自己的上下文的独立任务.通过 tcp 连接这些.稍后,您可以将这些进程移动到独立进程,而无需对代码进行重大更改.
线程之间的所有交互都以 ZeroMQ 消息的形式进行,您可以或多或少地正式定义这些消息.
不要在线程之间共享 ZeroMQ 套接字.ZeroMQ 套接字不是线程安全的.从技术上讲,可以将套接字从一个线程迁移到另一个线程,但这需要技巧.在线程之间共享套接字唯一合理的地方是语言绑定,这些绑定需要执行神奇的作,例如对套接字进行垃圾回收.
*/

// Missing Message 问题解决程序
/*
1. 在 SUB 套接字上,使用 zmq_setsockopt() 和 ZMQ_SUBSCRIBE 设置订阅,否则您将不会收到消息.因为您是按前缀订阅消息的,所以如果您订阅 “”(空订阅),您将获得所有内容.
2. 如果在 PUB 套接字开始发送数据后启动 SUB 套接字(即建立与 PUB 套接字的连接),您将丢失在建立连接之前发布的任何内容.如果这是一个问题,请设置您的架构,以便 SUB 套接字首先启动,然后 PUB 套接字开始发布.
3. 即使您同步了 SUB 和 PUB 套接字,您仍然可能会丢失消息.这是因为在实际创建连接之前不会创建内部队列.如果您可以切换 bind/connect 方向,以便 SUB 套接字绑定,并且 PUB 套接字连接,您可能会发现它更符合您的预期.
4. 如果你使用的是 REP 和 REQ 套接字,并且你不遵守同步 send/recv/send/recv 顺序,ZeroMQ 将报告错误,你可以忽略这些错误.然后,您看起来会丢失消息.如果您使用 REQ 或 REP,请遵循 send/recv 顺序,并始终在实际代码中检查 ZeroMQ 调用的错误.
5. 如果您使用的是 PUSH 套接字,您将发现第一个连接的 PULL 套接字将获取不公平的消息份额.仅当成功连接所有 PULL 套接字时,才会准确轮换消息,这可能需要几毫秒.作为 PUSH/PULL 的替代方法,对于较低的数据速率,请考虑使用 ROUTER/DEALER 和负载平衡模式.
6. 如果要跨线程共享套接字,请不要这样做.这将导致随机的怪异和崩溃.
7. 如果你使用的是 inproc,请确保两个 sockets 都位于同一上下文中.否则,连接端实际上会失败.此外,请先绑定,然后再连接.Inproc 不是像 TCP 那样的断开连接的传输.
8. 如果您使用的是 ROUTER 套接字,则很容易通过发送格式不正确的身份框架(或忘记发送身份框架)而意外丢失消息.一般来说,在 ROUTER sockets 上设置 ZMQ_ROUTER_MANDATORY 选项是一个好主意,但也要检查每个 send 调用的返回代码.
9. 最后,如果你真的无法弄清楚出了什么问题,请制作一个重现问题的最小测试用例,并向 ZeroMQ 社区寻求帮助.
*/

// 围绕此模型设计应用程序
// "服务器" 视为拓扑中或多或少绑定到固定端点的静态部分;
// "客户端" 则视为来来去去并连接到这些端点的动态部分;

// ZeroMQ 字符串
// ZeroMQ 字符串是长度指定的, 并且在网络上发送时没有尾部 null(\0)

// ZeroMQ 错误打印
/*
从 POSIX 约定开始:
    创建对象的方法如果失败, 则返回 NULL.
    处理数据的方法可能会返回已处理的字节数, 或者在出现错误或失败时返回 -1.
    其他方法在成功时返回 0, 在错误或失败时返回 -1.
    错误代码在 errno 或 zmq_errno() 中提供.
    日志记录的描述性错误文本由 zmq_strerror() 提供.

您应该将两个主要的异常情况作为非致命情况处理:
    当您的代码收到带有 ZMQ_DONTWAIT 选项的消息并且没有等待数据时, ZeroMQ 将返回 -1 并将 errno 设置为 EAGAIN.
    当一个线程调用 zmq_ctx_destroy() , 而其他线程仍在执行阻塞工作时, zmq_ctx_destroy() 调用将关闭上下文, 并且所有阻塞调用都以 -1 退出, 并且 errno 设置为 ETERM.
*/

// ZeroMQ I/O线程
// 一个 I/O 线程(用于所有套接字)足以满足除最极端应用程序之外的所有应用程序.
// 创建新上下文时, 它从一个 I/O 线程开始.
// 一般的经验法则是每秒每 GB 数据输入或输出允许一个 I/O 线程.
// 如果你只将 ZeroMQ 用于线程间通信(即, 没有外部套接字 I/O 的多线程应用程序), 你可以将 I/O 线程设置为零(只是好奇, 没有什么实质性的性能提升)
// 要增加 I/O 线程的数量, 请在创建任何套接字之前使用 zmq_ctx_set() 调用;
#if 0
int io_threads = 4;
void *context = zmq_ctx_new();
zmq_ctx_set(context, ZMQ_IO_THREADS, io_threads);
assert(zmq_ctx_get(context, ZMQ_IO_THREADS) == io_threads);
#endif

// ZeroMQ上下文
// 在进程开始时调用 zmq_ctx_new() 一次, 在结束时调用 zmq_ctx_destroy() 一次
// zmq_ctx_destroy() 函数会一直阻塞, 直到它知道的所有套接字都安全关闭(如果你不关闭某个套接字, 则该函数会一直阻塞).
// 即使你关闭了所有套接字,  zmq_ctx_destroy() 也会默认永远等待是否有待处理的连接或发送.
// 不要尝试从多个线程使用相同的套接字

// ZeroMQ套接字 void *
// zmq_socket();
// zmq_close();
// zmq_setsockopt();
// zmq_getsockopt();
// zmq_bind(); 一个套接字可以绑定多个端点; 对于大多数传输, 不能绑定到同一终端节点两次, 但 ipc 传输确实允许一个进程绑定到第一个进程已使用的端点.它旨在允许进程在崩溃后恢复
// zmq_connect(); 执行后连接就存在, 并且该节点可以开始将消息写入套接字

// ZeroMQ消息
// ZeroMQ消息 = 多个ZeroMQ消息帧(也叫部分消息) - ZeroMQ 消息的基本线路格式 - 指定长度的数据块.长度可以向上为 0
// ZeroMQ 保证传递消息的所有部分(一个或多个), 或者一个都不传递.
// 在处理完收到的消息后, 您必须调用 zmq_msg_close(), 使用的语言不会在范围关闭时自动销毁对象.发送消息后, 你无需调用该方法.

// 当您使用多部分消息时, 每个部分都是一个zmq_msg项.例如, 如果您要发送包含 5 个部分的消息, 则必须构造,发送和销毁 5 个 zmq_msg 项.
// 您可以提前执行此作(并将zmq_msg项存储在数组或其他结构中), 也可以在发送它们时逐个执行此作
/*
有关多部分消息的一些注意事项:
当您发送多部分消息时, 第一部分(以及所有后续部分)仅在您发送最后一部分时实际通过网络发送.
如果你使用的是 zmq_poll(), 当你收到消息的第一部分时, 其余的也都已到达.
您将收到消息的所有部分, 或者根本不收到.
消息的每个部分都是一个单独的zmq_msg项.
无论您是否选中 more 属性, 您都将收到消息的所有部分.
发送时, ZeroMQ 将消息帧在内存中排队, 直到收到最后一个消息帧, 然后发送所有消息帧.
除了关闭套接字之外, 无法取消部分发送的消息.
*/
#if 0
// send
zmq_msg_send(&message, socket, ZMQ_SNDMORE);
zmq_msg_send(&message, socket, ZMQ_SNDMORE);
zmq_msg_send(&message, socket, 0);

// recv
while (1)
{
    zmq_msg_t message;
    zmq_msg_init(&message);
    zmq_msg_recv(&message, socket, 0);

    //  Process the message frame

    zmq_msg_close(&message);
    if (!zmq_msg_more(&message))
    {
        break; //  Last message frame
    }
}
#endif

// 传递 zmq_msg_t(帧)
// zmq_msg_init();
// zmq_msg_init_size();
// zmq_msg_init_data();
// 零拷贝 该消息引用已使用 malloc()或其他分配器分配的数据块,然后将其传递给 zmq_msg_send() 当您创建消息时,您还会传递一个函数,当 ZeroMQ 完成发送消息时,它将调用该函数以释放数据块.
// 在写入时,ZeroMQ 的 multipart 消息与 zero-copy 可以很好地协同工作.在传统消息传递中,您需要将不同的缓冲区封送到一个可以发送的缓冲区中.这意味着复制数据.
// 使用 ZeroMQ,您可以将来自不同来源的多个缓冲区作为单独的消息帧发送.将每个字段作为长度分隔的帧发送.
// 对于应用程序,它看起来像一系列发送和接收调用.但是在内部,多个部分被写入网络并通过单个系统调用读回,因此它非常高效.
// 没有办法在接收时进行零拷贝:ZeroMQ 为您提供了一个缓冲区,您可以根据需要存储多长时间,但它不会将数据直接写入应用程序缓冲区
// 这是最简单的示例,假设 buffer 是在堆上分配的 1,000 字节块:
/*
void my_free (void *data, void *hint)
{
    free (data);
}

//  Send message from buffer, which we allocate and ZeroMQ will free for us
zmq_msg_t message;
zmq_msg_init_data(&message, buffer, 1000, my_free, NULL);
zmq_msg_send(&message, socket, 0);
*/
// zmq_msg_send(); 发送后将清除该消息, 即将大小设置为零; 您不能将同一条消息发送两次, 并且发送后无法访问消息数据.发生成功无需zmq_msg_close();
// zmq_msg_recv();
// zmq_msg_close();
// zmq_msg_data();
// zmq_msg_size();
// zmq_msg_more(); // ZeroMQ API 允许您编写带有 "more" 标志的消息, 当您读取消息时, 它允许您检查是否有 "more" 消息
// zmq_msg_copy(); // 引用; 只有关闭最后一个副本时, 消息才会最终销毁.
// zmq_msg_move();

// 传递字节数组
// zmq_send();
// zmq_recv();

// ZeroMQ 协议
// 单播
// inproc 服务器必须在任何客户端发出连接之前发出 bind.此问题已在 ZeroMQ v4.0 及更高版本中修复
// ipc
// tcp

// 多播 epgm 和 pgm

// ZeroMQ 消息传输模式
// REQ and REP
// PUB and SUB
// PUSH and PULL
// REQ and ROUTER (take care, REQ inserts an extra null frame)
// DEALER and REP (take care, REP assumes a null frame)
// DEALER and ROUTER
// DEALER and DEALER
// ROUTER and ROUTER
// PAIR and PAIR
// XPUB and XSUB
// 其他组合不可靠

/*
ZeroMQ 使用 HWM(高水位线)的概念来定义其内部管道的容量.
每个从套接字出来或进入套接字的连接都有自己的管道,以及用于发送和/或接收的 HWM,具体取决于套接字类型.
某些套接字 (PUB,PUSH) 只有发送缓冲区.
一些 (SUB, PULL, REQ, REP) 只有接收缓冲区.
一些 (DEALER, ROUTER, PAIR) 同时具有发送和接收缓冲区.

在 ZeroMQ v2.x 中,HWM 默认是无限的.
这很容易,但对于大批量出版商来说通常是致命的.
在 ZeroMQ v3.x 中,它默认设置为 1000,这更合理.
如果您仍在使用 ZeroMQ v2.x,则应始终在套接字上设置 HWM,无论是 1,000 以匹配 ZeroMQ v3.x,还是考虑到您的消息大小和预期订阅者性能的其他数字.

当您的套接字达到其 HWM 时,它将阻止或删除数据,具体取决于套接字类型.
如果 PUB 和 ROUTER 套接字到达其 HWM,它们将丢弃数据,而其他套接字类型将阻塞.
在 inproc 传输中,发送方和接收方共享相同的缓冲区,因此实际 HWM 是双方设置的 HWM 之和.

最后,HWM 并不精确;虽然默认情况下最多可以收到 1,000 条消息,但由于 libzmq 实现其队列的方式,实际缓冲区大小可能要小得多(只有一半).
*/

/* ROUTER套接字
身份和地址
ZeroMQ 中的身份概念特指 ROUTER 套接字以及它们如何识别它们与其他套接字的连接.
更广泛地说,身份被用作回复信封中的地址.在大多数情况下,身份是任意的,并且是 ROUTER 套接字的本地身份:它是哈希表中的查找键.
独立地,对等体可以具有物理地址(如“tcp://192.168.55.117:5670”)或逻辑地址(UUID 或电子邮件地址或其他唯一密钥).

使用 ROUTER 套接字与特定对等方通信的应用程序如果已构建必要的哈希表,则可以将逻辑地址转换为身份.
因为 ROUTER 套接字仅在该对等体发送消息时宣布连接的身份(到特定的对等体),所以您只能真正回复消息,而不能自发地与对等体通信.
即使您翻转规则并使 ROUTER 连接到对等体而不是等待对等体连接到 ROUTER,也是如此.
但是,您可以强制 ROUTER 套接字使用逻辑地址来代替其标识.zmq_setsockopt参考页面将此设置称为套接字标识.它的工作原理如下:

对等应用程序在绑定或连接之前设置其对等套接字(DEALER 或 REQ)的 ZMQ_IDENTITY 选项.
通常,对等体然后连接到已经绑定的 ROUTER 套接字.但是 ROUTER 也可以连接到对等体.
在连接时,对等套接字告诉路由器套接字,“请使用此身份进行此连接”.
如果对等套接字没有这么说,路由器就会为连接生成它通常的任意随机身份.
ROUTER 套接字现在将此逻辑地址提供给应用程序,作为来自该对等体的任何消息的前缀标识帧.
ROUTER 还期望逻辑地址作为任何传出消息的前缀标识帧.

ROUTER 错误处理
ROUTER 套接字确实有一种有点残酷的方式来处理它们无法发送到任何地方的消息:它们会静默地丢弃它们.
这种态度在工作代码中是有意义的,但它使调试变得困难.
“将身份作为第一帧发送”的方法已经足够棘手了,以至于我们在学习时经常会犯这个错误,而当我们搞砸时,ROUTER 的沉默并不是很有建设性.

从 ZeroMQ v3.2 开始,你可以设置一个 socket 选项来捕获这个错误:ZMQ_ROUTER_MANDATORY.
在 ROUTER 套接字上设置该标识,然后当您在 send 调用上提供不可路由的身份时,该套接字将发出 EHOSTUNREACH 错误信号.
*/

// 中介和代理 broker - 解决动态发现问题

void get_zmq_lib_version();

// 请求-响应
// REQ-REP 单客户端-单服务器
// 客户端发送请求.服务读取请求并发送回复.然后, 客户端读取回复;
// 如果客户端或服务尝试执行任何其他动作(例如, 连续发送两个请求而不等待响应), 则它们将收到错误
// 问题1: 如果终止服务器 (Ctrl-C) 并重新启动它, 客户端将无法正常恢复
void hwserver(); // 客户端向服务器发送 "Hello"
void hwclient(); // 服务器回复 "World"
// Lazy Pirate Pattern
/*
轮询 REQ 套接字,并仅在确定回复已到达时从它接收.
如果在超时期限内未收到回复,请重新发送请求.
如果多次请求后仍未回复,则放弃事务.

如果您尝试以严格的发送/接收方式以外的任何方式使用 REQ 套接字,您将收到错误(从技术上讲,REQ 套接字实现了一个小型有限状态机来强制发送/接收乒乓球,因此错误代码称为“EFSM”).
当我们想以盗版模式使用 REQ 时,这有点烦人,因为我们可能会在得到回复之前发送多个请求.
出现错误后: 客户端使用 REQ 套接字,并执行暴力关闭/重新打开,因为 REQ 套接字强制执行严格的发送/接收周期.
*/
void lpserver();
void lpclient();

/*
每个请求和每个回复实际上是两个帧,一个空帧,然后是正文

ROUTER 套接字跟踪它拥有的每个连接,并告诉调用者这些连接.
接收消息时,ZMQ_ROUTER套接字应在将消息传递给应用程序之前,在消息前面加上一个消息部分,其中包含原始对等体的身份.收到的消息在所有连接的对等方之间公平排队.
发送消息时,ZMQ_ROUTER套接字应删除消息的第一部分,并使用它来确定消息应路由到的对等节点的身份.
ZeroMQ v2.2 及更早版本使用 UUID 作为身份.ZeroMQ v3.0 及更高版本默认生成 5 字节的身份(0 + 随机 32 位整数)
ROUTER 套接字为其工作的每个连接发明一个随机身份.如果有三个 REQ 套接字连接到 ROUTER 套接字,它将发明三个随机身份,每个 REQ 套接字一个.

REQ 套接字将消息数据前面的空分隔符帧发送到网络.REQ 套接字是同步的.REQ 套接字总是发送一个请求,然后等待一个回复.REQ 套接字一次与一个 peer 通信.如果您将 REQ 套接字连接到多个对等体,则请求将一次一个轮次分发到每个对等体,并期望从每个对等体获得回复.
REP 套接字读取并保存所有身份帧,直到空分隔符(包括空分隔符),然后将以下帧或帧传递给调用方.REP 套接字是同步的,一次与一个 peer 通信.如果将 REP 套接字连接到多个对等体,则会以公平的方式从对等体读取请求,并且回复始终发送到发出最后一个请求的同一对等体.
DEALER 套接字对回复信封不知情,并像处理任何多部分消息一样处理此消息.DEALER 套接字是异步的,类似于 PUSH 和 PULL 的组合.它们在所有连接之间分发已发送的消息,并从所有连接中公平排队接收的消息.
ROUTER 套接字与回复信封无关,如 DEALER.它为其连接创建身份,并将这些身份作为任何接收消息中的第一帧传递给调用方.相反,当调用方发送消息时,它使用第一个消息帧作为身份来查找要发送到的连接.路由器是异步的.

DEALER 就像一个异步的 REQ 套接字,而 ROUTER 就像一个异步的 REP 套接字.
当我们使用 REQ 套接字时,我们可以使用 DEALER; 我们只需要自己阅读和写入信封.
在使用 REP 套接字的地方,我们可以插入 ROUTER;我们只需要自己管理身份.
将 REQ 和 DEALER 套接字视为“客户端”,将 REP 和 ROUTER 套接字视为“服务器”.
大多数情况下,您需要绑定 REP 和 ROUTER 套接字,并将 REQ 和 DEALER 套接字连接到它们.这并不总是这么简单,但这是一个干净而难忘的起点.
*/

// DEALER-REP 可以与多个 REP 服务器通信的异步客户端
/*
如果我们使用 DEALER 重写 “Hello World” 客户端,我们将能够发送任意数量的 “Hello” 请求,而无需等待回复.
当我们使用 DEALER 与 REP 套接字通信时,我们必须准确模拟 REQ 套接字将发送的信封,否则 REP 套接字将丢弃该消息作为无效消息.因此,为了发送消息,我们:

发送设置了 MORE 标志的空消息帧;然后
发送邮件正文.
当我们收到消息时,我们会:

接收第一帧,如果它不为空,则丢弃整个消息;
接收下一帧并将其传递给应用程序.
*/
void hwserver2();
void hwclient2();

// REQ-ROUTER 一个可以同时与多个 REQ 客户端通信的异步服务器
/*
我们可以通过两种不同的方式使用 ROUTER:
    作为在前端和后端套接字之间切换消息的代理.见mtserver();
    作为读取消息并对其执行作的应用程序.
在第一种情况下,ROUTER 只是读取所有帧,包括人工身份帧,并盲目地传递它们.
在第二种情况下,ROUTER 必须知道它所发送的回复信封的格式.
由于另一个对等体是 REQ 套接字,因此 ROUTER 将获取身份帧,空帧,然后获取数据帧.
*/
void hwserver3();
void hwclient3();
void identity(); // 两个连接到 ROUTER 套接字的 REQ 对等体
void rtreq();    // ROUTER Broker与一组 REQ worker 通信的负载平衡模式

// DEALER-ROUTER 提供了异步客户端与异步服务器通信,其中双方都可以完全控制消息格式
void hwserver4();
void hwclient4();

// DEALER-ROUTER 多客户端-多服务器
// 在第 8 章 - 分布式计算框架中,看到用于点对点工作的 DEALER 到 ROUTER 的替代设计
// 消息队列代理
void rrworker();
void rrclient();
void rrbroker(); // 手写的代理 IO多路复用
void msgqueue(); // zmq内置代理 zmq_proxy()

// DEALER-ROUTER 多客户端-多线程服务器; 将 broker 和 worker 折叠到一个进程中
// 请求-回复链是 REQ-ROUTER-queue-DEALER-REP 的.
/*
服务器启动一组 worker 线程.每个 worker 线程创建一个 REP 套接字,然后处理此套接字上的请求.
Worker 线程就像单线程服务器一样.唯一的区别是传输(inproc 而不是 tcp)和 bind-connect 方向.

服务器创建一个 ROUTER 套接字来与 Client 端通信,并将其绑定到其外部接口(通过 tcp).
服务器创建一个 DEALER 套接字来与 worker 通信,并将其绑定到其内部接口(通过 inproc).

服务器启动连接两个套接字的代理.代理从所有客户端公平地提取传入请求,并将这些请求分发给 worker.它还会将回复路由回其来源.
*/
void mtserver();
void rtdealer(); // ROUTER Broker 和与一组 DEALER worker 通信的负载平衡模式
void lbbroker(); // ROUTER Broker 和 ROUTER broker 负载均衡代理
// czmq风格         lbbroker2();
// czmq风格+reactor lbbroker3();
/*
现在让我们看看负载均衡算法.
它要求 Client 端和 worker 都使用 REQ 套接字,并且 worker 正确存储和重放他们收到的消息的信封.算法为:
    创建一个始终轮询后端的 pollset,并且仅在有一个或多个 worker 可用时轮询前端.
    轮询具有无限超时的活动.
    如果后端有活动,我们要么有 “ready” 消息,要么有 Client 端的回复.无论哪种情况,我们都将 worker 地址(第一部分)存储在 worker 队列中,如果其余部分是客户端回复,则通过前端将其发送回该客户端.
    如果前端有活动,我们接受客户端请求,弹出下一个 worker(这是最后一个使用的),然后将请求发送到后端.这意味着发送 worker 地址,空部分,然后是客户端请求的三个部分.
您现在应该看到,您可以根据 worker 在其初始 “ready” 消息中提供的信息,使用变体来重用和扩展负载均衡算法.例如,工作人员可能会启动并进行性能自检,然后告诉代理他们的速度有多快.然后,broker 可以选择最快的可用 worker 而不是最旧的 worker.
*/

// DEALER-DEALER 如果 DEALER 正在与一个且只有一个对等体通信,您也可以将 REP 与 DEALER 交换
// 当您将 REP 替换为 DEALER 时,您的 worker 可能会突然完全异步,从而发回任意数量的回复.代价是您必须自己管理回复信封,并正确处理它们,否则根本不会奏效.

// ROUTER-ROUTER Freelance 模式;非常适合 N 到 N 连接,但它是最难使用的组合

// PUB-SUB 发布-订阅
// 问题1: 如何做到在订阅者真正连接并准备就绪之前不要开始发布数据?
// 单向数据分发, 其中服务器将更新推送到一组客户端.
void wuserver(); // 服务器 推送由邮政编码,温度和相对湿度组成的天气更新.我们将生成随机值, 就像真实的气象站一样
void wuclient(); // 客户端 订阅其选择的邮政编码并收集该邮政编码的 100 个更新, 当客户端收集了一百个更新后, 它会计算平均值, 打印它, 然后退出

// PUB-SUB 同步的发布-订阅
/*
发布者提前知道它期望的订阅者数量.
发布者启动并等待所有订阅者连接.这是节点协调部分.每个订阅者订阅,然后通过另一个套接字告诉发布者它已准备就绪.
当发布服务器连接了所有订阅服务器时,它将开始发布数据.

// 问题: 我们不能假设 SUB 连接将在 REQ/REP 对话框完成时完成.
如果您使用除 inproc 之外的任何传输方式,则无法保证出站连接将以任何顺序完成.因此,该示例在订阅和发送 REQ/REP 同步之间执行一秒的暴力休眠.

更稳健的模型可以是:
Publisher 打开 PUB 套接字并开始发送 “Hello” 消息(不是数据).
订阅者连接 SUB 套接字,当他们收到 Hello 消息时,他们通过 REQ/REP 套接字对告诉发布者.
当发布者获得所有必要的确认后,它开始发送真实数据.
*/
void syncsub();
void syncpub();

// PUB-SUB 发布-订阅 消息信封: 将 前缀匹配 拆分为一个单独的消息帧,我们称之为 envelope
void psenvsub();
void psenvpub();

// XPUB-XSUB
// 添加 pub-sub 代理解决了我们示例中的动态发现问题.即发布订阅需要增加发布端, 此时就需要新增发布端, 且还要修改订阅端订阅代码;
// 我们将代理设置在网络的 "中间" 位置.代理打开一个 XSUB 套接字和一个 XPUB 套接字, 并将每个套接字绑定到已知的 IP 地址和端口.
// 然后, 所有其他进程都连接到代理, 而不是彼此连接.添加更多订阅者或发布者变得微不足道;
// ZeroMQ 执行从订阅者到发布者的订阅转发
// XSUB 和 XPUB 与 SUB 和 PUB 完全相同, 只是它们将订阅作为特殊消息公开.
// 代理必须将这些订阅消息从订阅者端转发到发布者端, 方法是从 XPUB 套接字读取这些消息并将其写入 XSUB 套接字.这是 XSUB 和 XPUB 的主要用例.

// 前端套接字 (SUB) 面向天气服务器所在的内部网络, 后端 (PUB) 面向外部网络上的订阅者.
// 它在前端套接字上订阅 weather 服务, 并在后端套接字上重新发布其数据
void wuproxy();

// PUSH-PULL 推-拉(管道)
// 问题1: 如何负载均衡;
// 使用 PUSH 和 PULL, 并且其中一个 worker 获得的消息比其他 worker 多得多, 这是因为该 PULL 套接字比其他 PULL 套接字加入得更快, 并在其他 worker 设法连接之前获取了大量消息
void taskvent(); // 一台可以同时完成任务的呼吸机
void taskwork(); // 一组处理任务的工作程序worker
void tasksink(); // 从 worker 进程收集返回结果的 sink
// 如果我们在后台启动了大量 worker, 我们现在想在批处理完成时杀死它们.让我们通过向 worker 发送 kill 消息来做到这一点
// 执行此作的最佳位置是 sink, 因为它确实知道批处理何时完成
// 使用 pub-sub 模型向 worker 发送 kill 消息:
/*
    sink 在新端点上创建一个 PUB 套接字.
    Worker 将其 input 套接字连接到此终端节点.
    当 sink 检测到批处理的末尾时, 它会向其 PUB 套接字发送 kill .
    当 worker 检测到此 kill 消息时, 它会退出.
*/
void taskwork2();
void tasksink2();

// 处理多个套接字
void msreader(); // 1. 轮询方式(不推荐)
void mspoller(); // 2. IO多路复用方式 zmq_poll()

// 处理中断信号
/*
该程序提供了 s_catch_signals(), 用于捕获 Ctrl-C (SIGINT) 和 SIGTERM.
当这些信号中的任何一个到达时, s_catch_signals() 处理程序将全局变量设置为 s_interrupted.
多亏了您的信号处理程序, 您的应用程序不会自动死亡.
相反, 您有机会优雅地清理和退出.
现在, 您必须显式检查中断并正确处理它.
通过在主代码的开头调用 s_catch_signals() (从 interrupt.c 复制此内容) 来执行此作.
这将设置信号处理.中断将对 ZeroMQ 调用产生如下影响:
    如果您的代码在阻塞调用(发送消息,接收消息或轮询)中阻塞, 则当信号到达时, 调用将返回 EINTR.
    如果 s_recv() 等包装器被中断, 则返回 NULL.

因此, 请检查 EINTR 返回代码,NULL 返回和/或 s_interrupted.

下面是一个典型的代码片段:
s_catch_signals ();
client = zmq_socket (...);
while (!s_interrupted)
{
    char *message = s_recv (client);
    if (!message)
        break; //  Ctrl-C used
}
zmq_close (client);
*/
void interrupt();

// 线程之间的信号(PAIR 套接字)
// 如果远程节点消失并返回,PAIR 套接字不会自动重新连接.
/*
接力赛 - 两个线程使用共享上下文通过 inproc 进行通信.
父线程创建一个套接字,将其绑定到 inproc:@<*>@ 端点,然后 *then// 启动子线程,将上下文传递给它.
子线程创建第二个套接字,将其连接到该 inproc:@<*>@ 端点,然后向父线程发出信号,表明它已准备就绪.
*/
void mtrelay();
