### 记录编程中遇到的坑

1. 日志中%s打印int变量, 导致程序coredump;
2. 有参函数, 调用时不传参数, 编译器只会报警告;
3. 默认func()的意思是可以传任意参数;

#### socket

- any `errno=22 EINVAL Invalid argument`, 检查函数入参;
- bind, listen `errno=98 EADDRINUSE Address already in use`;
    1. 设置端口复用;
    2. unix socket还可以利用这个特性来实现进程仅启动一次;
- send `errno=32 EPIPE Broken pipe` 关闭本端 socket;
- recv `errno=4 EINTR Interrupted system call` 重试;
- 非阻塞connect `errno=115 EINPROGRESS Operation now in progress` 需配合 select/poll/epoll + getsockopt(sockfd, SOL_SOCKET, SO_ERROR, ...) 等待完成;
- connect `errno=111 ECONNREFUSED Connection refused` 重试;
- tcp 关闭socket丢数据解决办法:
    1. 添加 SO_LINGER 选项(> 0), write返回后, 先 shutdown(SHUT_WR) 告诉对方“我不发了”;然后再使用ioctl(fd, SIOCOUTQ)获取未发送的数据字节数, 如果大于0, 则等待, 否则循环继续; 然后阻塞socket用 read() 等待对方关闭连接(收到 EOF或者收到协议规定的响应),这个过程阻塞/非阻塞socket都可以用 select/poll/epoll 来避免阻塞. 最后close;
- `lsof -p <pid> | wc -l` 获取当前进程打开的文件数;

#### 实时性

问题1: 回调串行阻塞(即单线程executor执行多个回调, 必须等待前一个执行完才能执行下一个)
问题2: 多线程竞争(多个回调中加相同的锁, 此时控制逻辑的延迟不可预知, 取决于线程切换/回调执行耗时等)
问题3: 误认为callback group自动隔离(线程池, 多个任务被放在同一个线程的工作队列中, 此时也会造成问题1)

操作系统
- PREEMPT_RT
- 线程调度优先级 99 + SCHED_FIFO;
- CPU绑核
- 锁定内存 mlockall
- 关闭C-state

内存和数据结构
- 预分配, 无动态分配
- 无锁结构
- 环形缓冲区

### 功能集锦

- 不同系统导致的文件时间错乱问题 `find . -type f -exec touch {} +`
- 编码转换 
    * 文件转换 `iconv -f GB2312 -t UTF-8 input.txt -o output.txt`
    * 命令行转换 `cat gb2312_file.txt | iconv -f GB2312 -t UTF-8`

#### 编译问题

1. 静态库循环依赖导致符号未定义; 解决方案: `-Wl,--start-group -lA -lB -Wl,--end-group`;