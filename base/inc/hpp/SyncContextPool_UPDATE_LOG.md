# SyncContextPool 更新日志

## 版本：v2.0 - 基于 mutex + condition_variable 的可复用实现

### 📅 更新日期
2026-03-23

### 🎯 更新目标
解决 `std::promise` 一次性使用限制，实现真正的对象池可复用性。

---

## 🔄 核心变更

### 1. 数据结构变更

#### ❌ 旧方案（std::promise）
```cpp
template <typename T = void>
struct SyncContext {
    std::mutex mutex;
    DataType data;
    int result = -1;
    std::promise<int> promise;  // ❌ 问题：只能调用一次 get_future()
};
```

#### ✅ 新方案（mutex + condition_variable）
```cpp
template <typename T = void>
struct SyncContext {
    mutable std::mutex mutex;      // ✅ 保护状态
    DataType data;                 // ✅ 用户数据
    int result = -1;               // ✅ 执行结果
    bool ready = false;            // ✅ 就绪标志（新增）
    std::condition_variable cv;    // ✅ 条件变量（可无限次复用）
};
```

### 2. Reset() 方法变更

#### ❌ 旧实现
```cpp
void Reset() {
    std::lock_guard<std::mutex> lock(mutex);
    data = T{};
    result = -1;
    // ❌ 无法真正重置 promise
    promise = std::promise<int>();  // 即使重新创建，仍有问题
}
```

#### ✅ 新实现
```cpp
void Reset() {
    std::lock_guard<std::mutex> lock(mutex);
    data = T{};
    result = -1;
    ready = false;  // ✅ 只需重置标志
    // ✅ condition_variable 是原生对象，无需重置，可无限次使用
}
```

### 3. Wait() 方法变更

#### ❌ 旧实现
```cpp
int Wait(uint32_t timeout_ms) {
    auto future = promise.get_future();  // ❌ 只能调用一次
    if (timeout_ms == 0) {
        future.wait();
    } else {
        future.wait_for(std::chrono::milliseconds(timeout_ms));
    }
    return result;
}
```

#### ✅ 新实现
```cpp
int Wait(uint32_t timeout_ms) {
    std::unique_lock<std::mutex> lock(mutex);
    
    if (timeout_ms == 0) {
        // ✅ 带谓词的 wait，自动处理虚假唤醒
        cv.wait(lock, [this]() { return ready; });
    } else {
        // ✅ 带超时的 wait_for，同样带谓词
        bool success = cv.wait_for(
            lock, 
            std::chrono::milliseconds(timeout_ms),
            [this]() { return ready; }
        );
        if (!success) {
            return -ETIMEDOUT;
        }
    }
    
    return result;
}
```

### 4. SetResult() 方法变更

#### ❌ 旧实现
```cpp
void SetResult(int res) {
    std::lock_guard<std::mutex> lock(mutex);
    result = res;
    promise.set_value(res);  // ❌ 如果 promise 已被 retrieve，会崩溃
}
```

#### ✅ 新实现
```cpp
void SetResult(int res) {
    {
        std::lock_guard<std::mutex> lock(mutex);
        result = res;
        ready = true;  // ✅ 设置就绪标志
    }
    cv.notify_one();  // ✅ 通知等待者（在锁外通知，避免唤醒风暴）
}
```

---

## ✅ 优势对比

| 特性 | 旧方案 (std::promise) | 新方案 (mutex + cv) |
|------|----------------------|---------------------|
| **可复用性** | ❌ 一次性，无法真正复用 | ✅ 真正的无限次复用 |
| **堆内存开销** | ⚠️ shared_ptr 方案有开销 | ✅ 零堆内存分配 |
| **性能** | ⚠️ promise 状态管理开销 | ✅ 直接的条件变量通知 |
| **语义清晰度** | ⚠️ 需要理解 promise/future | ✅ 经典的生产者 - 消费者模式 |
| **虚假唤醒处理** | N/A | ✅ 自动通过谓词处理 |
| **线程安全** | ✅ 线程安全 | ✅ 线程安全 |

---

## 🧪 测试覆盖

### 新增测试用例

1. **test_context_pool_reuse()** - 验证 condition_variable 可重复使用
   - 同一上下文借用 - 归还 - 再借用 3 次
   - 每次都能正确设置和获取结果

2. **test_same_context_multiple_reuse()** - 同一上下文多次复用
   - 只有 1 个上下文的池
   - 连续复用 5 次，验证每次都正常工作

3. **test_reset_state()** - Reset 后状态验证
   - 验证 Reset() 正确重置 data 和 result
   - 验证 ready 标志被重置为 false

4. **test_data_context_reuse()** - 带数据的上下文复用
   - 验证携带自定义数据结构的上下文可以复用
   - 每次复用都能正确设置和清除数据

5. **test_timeout_then_reuse()** - 超时后再次使用
   - 第一次故意超时（不调用 SetResult）
   - 第二次正常使用，验证超时不影响后续复用

### 测试结果

```
========================================
Starting SyncContextPool Tests
Based on mutex + condition_variable (Reusable)
========================================

✓ All 16 tests passed
✓ Pool reuse test passed: condition_variable works correctly after multiple uses
✓ Multiple reuse test passed: all 5 iterations completed successfully
✓ Reset state test passed: data and result properly reset
✓ Data context reuse test completed
✓ Timeout then reuse test completed
```

---

## 📝 应用层影响

### ✅ 零感知升级

应用层代码**无需任何修改**，接口完全兼容：

```cpp
// 原有代码保持不变
auto guard = pool.Borrow();
guard->SetData(my_data);
int result = guard->Wait(1000);
guard->SetResult(0);
// guard 析构时自动归还
```

### ✅ 行为改进

- **更稳定**：不会因为重复使用而崩溃
- **更高效**：零堆内存分配，更好的性能
- **更清晰**：语义更符合经典的同步模式

---

## 🔍 技术细节

### 为什么 condition_variable 可以无限次复用？

`std::condition_variable` 是一个**轻量级的内核对象**，它：
1. 不维护内部状态（除了等待队列）
2. 不需要"重置"操作
3. 每次 `notify_one()` 只唤醒一个等待者
4. 配合 `ready` 标志使用，可以正确处理虚假唤醒

### 虚假唤醒（Spurious Wakeup）处理

```cpp
// ✅ 正确写法：带谓词的 wait
cv.wait(lock, [this]() { return ready; });

// 等价于显式循环：
// while (!ready) {
//     cv.wait(lock);
// }
```

即使操作系统无故唤醒了线程，由于 `ready` 标志仍为 `false`，线程会自动继续等待。

---

## 📚 参考资料

- C++ Standard: [thread.condition.condvar](https://en.cppreference.com/w/cpp/thread/condition_variable)
- POSIX Threads: pthread_cond_wait, pthread_cond_signal
- 《C++ Concurrency in Action》Chapter 4: Synchronizing concurrent operations

---

## 👨‍💻 作者
cpunchline-hub
