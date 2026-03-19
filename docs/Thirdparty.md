### 采用CMake submodule机制, 子模块也有依赖, 则递归地拉取代码
  - 添加子模块
``` shell

# all
git submodule add https://bgithub.xyz/boostorg/boost.git thirdparty/boost
git submodule add https://gitee.com/tboox/tbox.git thirdparty/tbox

# event/bus/rpc/soa
git submodule add https://bgithub.xyz/chriskohlhoff/asio.git thirdparty/asio
git submodule add https://bgithub.xyz/ithewei/libhv.git thirdparty/libhv
git submodule add https://bgithub.xyz/openwrt/libubox.git thirdparty/libubox
git submodule add https://bgithub.xyz/openwrt/ubus.git thirdparty/ubus
git submodule add https://bgithub.xyz/eclipse-iceoryx/iceoryx.git thirdparty/iceoryx

# msg queue
git submodule add https://bgithub.xyz/nanomsg/nng.git thirdparty/nng
git submodule add https://bgithub.xyz/zeromq/libzmq.git thirdparty/libzmq
git submodule add https://bgithub.xyz/zeromq/czmq.git thirdparty/czmq
git submodule add https://bgithub.xyz/zeromq/cppzmq.git thirdparty/cppzmq

# lockfree queue
git submodule add https://bgithub.xyz/cameron314/readerwriterqueue.git thirdparty/readerwriterqueue
git submodule add https://bgithub.xyz/cameron314/concurrentqueue.git thirdparty/concurrentqueue
git submodule add https://bgithub.xyz/erez-strauss/lockfree_mpmc_queue.git thirdparty/lockfree_mpmc_queue
git submodule add  https://bgithub.xyz/sarinaditya/message_queue.git thirdparty/message_queue

# tests
git submodule add https://bgithub.xyz/google/googletest.git thirdparty/googletest

# struct
git submodule add https://bgithub.xyz/troydhanson/uthash.git thirdparty/uthash

# config/db
git submodule add https://bgithub.xyz/hyperrealm/libconfig.git thirdparty/libconfig
git submodule add https://bgithub.xyz/nlohmann/json.git thirdparty/json
git submodule add https://bgithub.xyz/redis/redis.git thirdparty/redis
git submodule add https://bgithub.xyz/nanopb/nanopb.git thirdparty/nanopb
# https://sqlite.org/download.html
git submodule add https://bgithub.xyz/LMDB/lmdb.git thirdparty/lmdb

# log and trace
git submodule add https://bgithub.xyz/Neargye/nameof.git thirdparty/nameof
git submodule add https://bgithub.xyz/gabime/spdlog.git thirdparty/spdlog
git submodule add https://bgithub.xyz/bombela/backward-cpp.git thirdparty/backward-cpp

# thread-pool
git submodule add https://bgithub.xyz/bshoshany/thread-pool.git thirdparty/thread-pool

# C macro
git submodule add https://bgithub.xyz/Azure/macro-utils-c.git thirdparty/macro-utils-c

# sh
git submodule add https://bgithub.xyz/dylanaraps/pure-bash-bible.git thirdparty/pure-bash-bible

# continues
# https://dunkels.com/adam/pt/

# mosquitto
git submodule add https://bgithub.xyz/eclipse-mosquitto/mosquitto.git thirdparty/mosquitto

# shm_container https://bgithub.xyz/mikewei/blogs/blob/master/2017-02-04-shm_container_intro.md
git submodule add https://bgithub.xyz/mikewei/shm_container.git thirdparty/shm_container

# ccbase
git submodule add https://bgithub.xyz/mikewei/ccbase.git thirdparty/ccbase

# AI
git submodule add https://bgithub.xyz/affaan-m/everything-claude-code thirdparty/everything-claude-code

```
  - 查看子模块
    - `git submodule`
  - 同步子模块
    - `git submodule sync`
  - 拉取子模块
    - `git submodule update --init --recursive` 拉取没带上述选项, 则执行该命令; 所有层层嵌套的子模块
  - 更新子模块: 进入子模块目录查看和拉取即可; 为了直接从子模块的当前分支的远程追踪分支获取最新变更, 不加则是默认从父项目的 SHA-1 记录中获取变更
    - `git submodule update --remote --merge` 父模块目录执行
  - 移除子模块
    - `git submodule deinit -f /path/to/submodule`
    - `git rm -f /path/to/submodule`
    - `rm -rf .git/modules/path/to/submodule`
  - 移除所有子模块
    - `git submodule deinit -f --all`
    - `rm -rf .git/modules/*`

### skills

`mkdir -p .roo && ln -s ../.github/skills .roo/skills`
`ln -s ../thirdparty/everything-claude-code/docs/zh-CN/agents .github/agents`
`ln -s ../thirdparty/everything-claude-code/docs/zh-CN/commands .github/commands`
`ln -s ../thirdparty/everything-claude-code/docs/zh-CN/rules .github/rules`
`ln -s ../thirdparty/everything-claude-code/docs/zh-CN/skills .github/skills`

`ln -s ../thirdparty/everything-claude-code/docs/zh-CN/agents .roo/agents`
`ln -s ../thirdparty/everything-claude-code/docs/zh-CN/commands .roo/commands`
`ln -s ../thirdparty/everything-claude-code/docs/zh-CN/rules .roo/rules`
`ln -s ../thirdparty/everything-claude-code/docs/zh-CN/skills .roo/skills`

### 三方应用

1. pigz 并行化压缩工具
2. jq json处理器
