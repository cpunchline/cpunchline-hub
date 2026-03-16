# ZeroMQ 学习示例

## 内存泄漏检测

使用 Valgrind 检测 ZMQ 程序内存泄漏:

```bash
valgrind --tool=memcheck --leak-check=full --suppressions=vg.supp someprog
```
