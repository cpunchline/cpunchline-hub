# Rust 子项目

## 🚀 Rust 环境安装

```bash
# 配置国内镜像源
export RUSTUP_UPDATE_ROOT=https://mirrors.tuna.tsinghua.edu.cn/rustup/rustup
export RUSTUP_DIST_SERVER=https://mirrors.tuna.tsinghua.edu.cn/rustup

# 安装 Rust
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
source $HOME/.cargo/env
```

## 📦 Cargo 常用命令

### 基础操作
```bash
# 项目管理
cargo new <project-name>     # 创建新项目
cargo init                   # 在当前目录初始化项目
cargo build                  # 编译项目
cargo build --release        # 发布模式编译(性能更优)
cargo run                    # 编译并运行
cargo clean                  # 清理构建文件
```

### 开发工具
```bash
# 代码质量
cargo fmt                    # 代码格式化
cargo check                  # 语法检查
cargo clippy                 # 代码优化建议
cargo fix                    # 自动修复问题

cargo doc                    # 生成文档
cargo rustdoc                # 生成文档(带参数)
```

### 测试相关
```bash
cargo test                   # 运行单元测试
cargo bench                  # 运行基准测试
```

### 包管理
```bash
cargo add <crate>            # 添加依赖
cargo remove <crate>         # 移除依赖
cargo update                 # 更新依赖
cargo search <keyword>       # 搜索包
cargo tree                   # 查看依赖树
```

### 发布相关
```bash
cargo package                # 打包项目
cargo publish                # 发布到 crates.io
cargo owner                  # 管理包所有者
cargo yank                   # 撤回已发布版本
```
