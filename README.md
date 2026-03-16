## CPUNCHLINE 仓库

### 🔖 cpunchline 是谁

👨‍🎓 读书 | 西安 ` 计算机科学与技术 ` 本科

👨‍💻 工作 | 西安 ` 嵌入式软件开发

🎃 爱好 | 研究技术,吃吃喝喝,撸啊撸 ~

🏷️ 标签 | 00后boy卷王仅会给手的街舞观众话痨

📥 邮箱 | cpunchline@foxmail.com

### ✊ 人生名言

* 所有看上去是天才的人, 都少不了勤勉的练习.
* 所有看起来的幸运, 都源自坚持不懈的努力.
* 所有的惊艳, 都来自长久的准备.

### 📁 项目结构

``` shell
📁 cpunchline-hub/
├── application/     # 核心应用示例
├── base/           # 基础数据结构库
├── cmake/          # CMake配置脚本
├── docs/           # 技术文档
├── include/        # 公共头文件
├── kernel/         # Linux内核学习
├── lib/            # 封装库组件
├── rust-project/   # Rust子项目
├── scripts/        # 构建脚本
├── simulator/      # 测试模拟器
├── tests/          # 单元测试
├── thirdparty/     # 第三方依赖
├── tools/          # 开发工具
└── 配置文件         # 开发环境配置
```

### 中间件

- 数据结构: 队列queue, 链表list, 栈stack, 堆(优先级队列)heap, 哈希表hash, 红黑树rbtree, 跳表, 循环缓冲区RingBuffer等;
- 池化技术: 定时器池(时间轮, 最小堆, 哈希表, 跳表, 红黑树组织); 内存池(链表); 线程池(队列); 连接池(socket连接/数据库连接);
- Reactor 模式:
    - 单线程模型: one-acceptor-one-thread
        - 所有 I/O 操作(包括连接建立,数据读写,事件分发等),业务处理,都是由一个线程完成的;
    - 多线程模型: multi-acceptor-threads
        - 所有 I/O 操作(包括连接建立,数据读写,事件分发等),业务处理由线程池完成的;
    - 主从多线程模型: one-acceptor-multi-workers
        - 主 Reactor 处理 新建立的连接; 一个线程;
        - 从 Reactor IO读写事件/事件分发; 线程池;
        - 例: ipc_hv
- 消息中间件:
    - zmq 例: ipc_zmq
    - nng 例: ipc_nng/ipc_nng_long
- 三方库学习
    - openssl
    - concurrentqueue
    - sqlite
