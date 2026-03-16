# USBIPD-WIN 在 WSL2 中使用 USB 设备完整指南

## 一,前置条件

### Windows 端需要安装
1. **usbipd-win**
   - 安装命令:`winget install usbipd`
   - 或从 GitHub 下载:https://github.com/dorssel/usbipd-win/releases

2. **WSL2**(已启用并运行)

### Linux (WSL) 端需要安装
```bash
sudo apt update
sudo apt install google-android-platform-tools-installer
```

---

## 二,使用流程(WSL 中使用 USB 设备)

### 步骤 1:在 Windows 中查看 USB 设备
以**管理员身份**打开 PowerShell,运行:
```powershell
usbipd list
```

输出示例:
```
BUSID  VID:PID    DEVICE                                                      STATE
1-5    305a:1402  GosuncnWelink Diagnostics Interface (COM236)                Not shared
1-6    046d:c53f  LIGHTSPEED Receiver, USB 输入设备                          Not shared
```

### 步骤 2:绑定设备(bind)
```powershell
usbipd bind --busid=1-5
```

> 如果设备被 Windows 占用,会报错 "Device busy",此时使用强制绑定:
> ```powershell
> usbipd bind --busid=1-5 --force
> ```

绑定成功后,`usbipd list` 显示 STATE 为 **Shared**

### 步骤 3:连接到 WSL(attach)
```powershell
usbipd attach --wsl --busid=1-5
```

成功后会显示类似:
```
usbipd: info: Using WSL distribution 'Ubuntu' to attach; the device will be available in all WSL 2 distributions.
usbipd: info: Detected networking mode 'nat'.
usbipd: info: Using IP address 172.23.96.1 to reach the host.
```

### 步骤 4:在 WSL 中验证设备
打开 WSL 终端,运行:
```bash
# 查看 USB 设备列表
adb devices

# 示例输出
# Bus 002 Device 002: ID 305a:1402 GosuncnWelink Diagnostics Interface
# /dev/ttyUSB0
```

---

## 三,恢复流程(返回 Windows 使用)

### 步骤 1:在 WSL 中断开设备(可选)
如果设备正在使用,先关闭相关程序

### 步骤 2:在 Windows 中解除绑定
以**管理员身份**打开 PowerShell,运行:
```powershell
usbipd unbind --busid=1-5
```

或解除所有绑定:
```powershell
usbipd unbind --all
```

### 步骤 3:验证 Windows 已恢复设备
```powershell
usbipd list
```

STATE 应显示为 **Not shared**,设备管理器中 COM 端口恢复正常

---

## 四,常用命令速查

| 命令 | 说明 |
|------|------|
| `usbipd list` | 列出所有 USB 设备及其状态 |
| `usbipd bind --busid=1-5` | 绑定设备(准备共享) |
| `usbipd bind --busid=1-5 --force` | 强制绑定(设备被占用时) |
| `usbipd attach --wsl --busid=1-5` | 连接到 WSL |
| `usbipd detach --wsl --busid=1-5` | 从 WSL 断开(但保持绑定) |
| `usbipd unbind --busid=1-5` | 解除绑定(还给 Windows) |
| `usbipd unbind --all` | 解除所有绑定 |

---

## 五,常见问题

### 1. "Device busy (exported)" 错误
**原因**:设备被 Windows 占用  
**解决**:使用 `--force` 强制绑定
```powershell
usbipd bind --busid=1-5 --force
```

### 2. WSL 中看不到 /dev/ttyUSB0
**原因**:可能缺少 usbip 工具或内核模块  
**解决**:
```bash
# 检查 usbip 是否安装
which usbip

# 如果没有,重新安装
sudo apt install linux-tools-generic hwdata
sudo update-alternatives --install /usr/local/bin/usbip usbip /usr/lib/linux-tools/*-generic/usbip 20
```

### 3. attach 成功但 WSL 中 lsusb 看不到
**原因**:WSL 内核可能不支持该设备  
**解决**:更新 WSL 内核
```powershell
wsl --update
```

---

## 六,完整操作示例

```powershell
# ===== Windows PowerShell (管理员) =====

# 1. 查看设备
usbipd list

# 2. 绑定设备(强制)
usbipd bind --busid=1-5 --force

# 3. 连接到 WSL
usbipd attach --wsl --busid=1-5

# 4. 验证状态
usbipd list
# STATE 应显示为 Attached

# ===== 使用完毕后 =====

# 5. 解除绑定,还给 Windows
usbipd unbind --busid=1-5

# 6. 验证已恢复
usbipd list
# STATE 应显示为 Not shared
```

```bash
# ===== WSL 终端 =====

# 连接后验证
lsusb
ls /dev/ttyUSB*

# 使用设备(例如 minicom)
minicom -D /dev/ttyUSB0 -b 115200
```

---

## 参考
- [usbipd-win GitHub](https://github.com/dorssel/usbipd-win)
- [Microsoft 官方文档 - 连接 USB 设备](https://docs.microsoft.com/zh-cn/windows/wsl/connect-usb)
