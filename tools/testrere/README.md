# testrere.py - Shell 命令录制与回放测试工具

`testrere.py` 是一个轻量级的命令行测试工具,用于**录制**一组 shell 命令的执行结果,并在后续**回放**时验证其行为是否保持一致.
适用于回归测试,脚本验证和 CI 环境中的行为锁定.

## 功能

- ✅ 录制命令的 stdout,stderr 和退出码
- ✅ 回放时精确比对(支持 unified diff 显示差异)
- ✅ 自包含,仅依赖 Python 标准库
- ✅ 快照以紧凑二进制格式存储(`.bi` 文件)

## 使用

1. 创建测试命令列表
2. 录制快照
3. 回放验证

## 详细说明

### .bi 自定义二进制格式
``` shell
:i count <N>
:b shell <len>
<command bytes>
:i returncode <code>
:b stdout <len>
<output bytes>
:b stderr <len>
<error bytes>
```

### record 模式

读取一个包含多行 shell 命令的 .list 文件, 逐条执行, 并将每条命令的如下内容以二进制格式保存到同名的 .bi 文件中(如 test.list.bi)
- 执行命令本身
- 退出码(return code)
- 标准输出(stdout)
- 标准错误(stderr)

### replay 模式

重新读取 .list 文件中的命令, 逐条执行, 并与 .bi 文件中保存的快照进行比对.
如果任何一项(命令内容,退出码,stdout,stderr)不一致, 则报错并退出;全部一致则输出 OK.
