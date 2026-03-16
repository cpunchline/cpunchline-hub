---
name: coding-standards
description: 通用编码规范. 适用于C/C++/CMake/Shell/Python等语言.
---

# 编码规范 & 最佳实践

适用于所有项目的通用编码规范.

## 启用时机

- 创建一个新工程/新模块时
- 审查代码质量和可维护性时
- 重构现有代码以遵循规范时
- 强制执行命名检查, 格式检查或结构一致性检查时
- 设置linting/格式化/类型检查规则时
- 新贡献者学习编码规范时

## 代码质量原则

### 1. 可读性优先

- 代码被阅读的次数远多于编写的次数
- 使用清晰的变量和函数名
- 首选自文档化代码,而非依赖注释
- 严格的代码格式

### 2. 保持简单

- 采用最简单且可行的解决方案
- 避免过度设计
- 拒绝过早优化
- 易于理解 > 炫技代码

### 3. 拒绝重复

- 将通用逻辑提取为函数
- 创建可复用组件
- 在模块间共享工具函数
- 避免复制粘贴式编程

### 4. 按需执行

- 不要构建当前不需要的功能
- 避免推测性的通用设计
- 仅在必要时增加复杂度
- 从简单开始, 按需重构

## C/C++ 标准

### 变量命名

- 见名知意
- C 以下划线法命名
- C++ 以小驼峰法命名
- 若为续写/重构, 则按原有规范命名

``` C
// ✅ GOOD: Descriptive names
const int type = "int";
const bool is_used = true;
const uint32_t total_num = 1000;

// ❌ BAD: Unclear names
const int t = "int";
bool flag = true;
const uint32_t n = 1000;
```

``` C++
// ✅ GOOD: Descriptive names
const int type = "int";
const bool isUsed = true;
const uint32_t totalNum = 1000;

// ❌ BAD: Unclear names
const int t = "int";
bool flag = true;
const uint32_t n = 1000;
```

### 函数命名

- 见名知意
- C 以下划线法命名
- C++ 以小驼峰法命名
- 若为续写/重构, 则按原有规范命名

``` C
// ✅ GOOD: Verb-noun pattern
int32_t fetch_marketdata(std::string marketId) {}
uint32_t calculate_similarity(std::vector &a, std::vector &b) {}
bool is_validemail(std::string) {}

// ❌ BAD: Unclear or noun-only
int32_t market(std::string marketId) {}
uint32_t similarity(std::vector &a, std::vector &b) {}
bool email(std::string) {}
```

``` C++
// ✅ GOOD: Verb-noun pattern
int32_t fetchMarketData(std::string marketId) {}
uint32_t calculateSimilarity(std::vector &a, std::vector &b) {}
bool isValidEmail(std::string) {}

// ❌ BAD: Unclear or noun-only
int32_t market(std::string marketId) {}
uint32_t similarity(std::vector &a, std::vector &b) {}
bool email(std::string) {}
```

### 类型安全

- 定义变量最好使用`stdint.h`中定义的类型;
- 赋值/调用函数操作类型必须匹配, 不匹配则进行合理强制转换;

### 错误处理

- 不要使用异常捕获语法(try-catch)
- 使用Linux系统调用必须判断返回值, 并打印errno错误码及其描述

## API 设计规范

### 代码文件结构

#### 模块代码文件结构

- 文件命名体现子模块功能

``` txt
├── CMakeLists.txt
├── README.md               库的功能概要介绍
├── API.md                  详细说明每个API的输入/输出/作用
├── images                  文档所需要的图示
├── inc                     可依赖功能进行细分文件
│   ├── modulename_common.h
│   ├── modulename_json.h
│   ├── modulename_logic.h
│   ├── modulename_xxx.h
│   └── module_name.h
└── src                      可依赖功能进行细分文件
│   ├── modulename_json.c
│   ├── modulename_logic.c
│   ├── modulename_xxx.c
    └── modulename.c          主程序实现
```

#### 库代码文件结构

- 文件命名体现子模块功能

``` txt
├── CMakeLists.txt
├── README.md      库的功能概要介绍
├── API.md         详细说明每个API的输入/输出/作用
├── images         文档所需要的图示
├── inc            可依赖功能进行细分目录
│   ├── dsa
│   ├── hpp
│   ├── lib_name_api.h 库API声明
│   ├── stb
│   ├── xxx
│   └── utility
└── src            可依赖功能进行细分目录
    ├── dsa
    └── utility
```

### 注释和文档

#### 触发时机

- 关键操作刨析时
- 出于性能考虑时
- API头文件实现时, 遵循`Doxygen`注释格式
- API库文档`API.md`实现时
- 若为续写/重构, 则按原有规范执行

### 代码反例检查

- 过长函数, 函数实现不能超过100行, 按逻辑拆分
- 深层嵌套, 函数嵌套不能超过5层, 早期返回
- 魔鬼数字, 常量均需通过宏/constexpr定义, 且见名知意

**再次强调**: 代码质量不容妥协, 清晰可读, 可维护的代码能赋能快速开发和优雅重构