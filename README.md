# AIChat-sdk — 多模型 AI 对话服务

**开发环境**：Ubuntu / VS Code  
**编译器**：g++  
**编程语言**：C++20  
**构建工具**：CMake 3.18+  
**开源协议**：Apache License 2.0

## 项目简介

基于 C++20 开发的多模型 AI 对话服务，包含 **SDK 层** 和 **Server 层** 两部分。SDK 层封装了多 LLM 模型管理（DeepSeek / ChatGPT / Gemini）、会话管理、用户管理及 SQLite 数据持久化；Server 层基于 cpp-httplib 提供 RESTful API，支持用户注册（邮箱验证码）、会话 CRUD、消息发送（全量/流式），并内置前端静态文件服务。


## 代码统计

| 项目 | 数值 |
|------|------|
| 总行数 | ~8,055 |
| SDK 核心 (sdk/) | 2,488 行 |
| HTTP 服务 (aichat_server/src/) | 1,039 行 |
| 核心库合计 (src + include) | 3,968 行 |
| 单元测试 (Test/) | 4,087 （部分单元测试由 AI 辅助编写）|
| 源文件数 | 28（不含测试） |

## 核心模块

> :book: **详细文档**：[核心模块]([docs/core_modules.md](https://github.com/CaiFuLei007/AIChat-sdk/wiki)) — 包含每个模块的数据结构、接口、实现细节和交互关系

### 架构总览

```
┌─────────────────────────────────────────────────────────┐
│                    AIChatServer                          │
│  (httplib HTTP 路由 + 邮箱验证码 + 静态文件服务)         │
└────────────────────────┬────────────────────────────────┘
                         │ 调用
┌────────────────────────▼────────────────────────────────┐
│                    AIChatSdk                             │
│  ┌──────────────┐          ┌──────────────────┐         │
│  │  LLManager   │          │  SessionManager   │         │
│  │ ┌──────────┐ │          │ ┌──────────────┐ │         │
│  │ │ DeepSeek │ │          │ │ DataManager   │ │         │
│  │ ├──────────┤ │          │ │ (SQLite)      │ │         │
│  │ │ ChatGPT  │ │          │ ├──────────────┤ │         │
│  │ ├──────────┤ │          │ │ TimerWheel   │ │         │
│  │ │ Gemini   │ │          │ │ (定时器)      │ │         │
│  │ └──────────┘ │          │ └──────────────┘ │         │
│  └──────────────┘          └──────────────────┘         │
└─────────────────────────────────────────────────────────┘
```

### 模块列表

| 模块 | 文件 | 职责 |
|------|------|------|
| **AIChatSdk** | `sdk/include/aichat_sdk.h` | 总入口，组合 LLManager + SessionManager |
| **LLManager** | `sdk/include/llmmanager.h` | 多模型 Provider 注册、初始化、消息路由 |
| **Provider** | `sdk/include/provider/provider.h` | 模型适配抽象基类 |
| **DeepSeekProvider** | `sdk/include/provider/deepseek_provider.h` | DeepSeek API 适配（Chat Completions 风格） |
| **ChatGPTProvider** | `sdk/include/provider/chatgpt_provider.h` | OpenAI Responses API 适配 |
| **GeminiProvider** | `sdk/include/provider/gemini_provider.h` | Google Gemini API 适配 |
| **SessionManager** | `sdk/include/session_manager.h` | 用户/会话内存缓存 + 10 分钟定时淘汰 |
| **DataManager** | `sdk/include/data_manager.h` | SQLite 持久化（user/session/message 三表） |
| **TimerWheel** | `sdk/include/base/timerwheel.h` | 60 槽时间轮，基于 timerfd + epoll |
| **Poller** | `sdk/include/base/poller.h` | epoll 轻量封装 |
| **Logger** | `sdk/include/base/util/mylog.h` | spdlog 日志封装 |
| **Curl** | `aichat_server/src/curl_util.hpp` | SMTP 邮件发送 |
| **AIChatServer** | `aichat_server/src/aichat_server.h` | HTTP 路由 + 验证码 + SSE 流式响应 |

## 项目编译

### 环境要求

- CMake 3.18+
- g++ 支持 C++20
- OpenSSL、CURL、SQLite3（系统库）

```bash
# Ubuntu 安装依赖
sudo apt install cmake g++ libssl-dev libcurl4-openssl-dev libsqlite3-dev libgtest-dev
```

### 编译服务端

```bash
cd aichat_server
mkdir build && cd build
cmake ..
make -j$(nproc)

# 运行（从 CMakeList 同级目录）
./aichat_server --config ./config.json
```

### 编译单元测试

```bash
cd Test
mkdir build && cd build
cmake ..
make -j$(nproc)

# 运行测试
./bin/debug/all_test
```

> **注意**：SDK 依赖（httplib、jsoncpp、spdlog、fmt、gflags）通过 CMake FetchContent 自动下载，无需手动安装。首次编译需要联网。

## 配置说明

服务端支持两种配置方式：**命令行参数** 和 **JSON 配置文件**。命令行参数优先级高于配置文件。

### JSON 配置文件

```json
{
    "server": {
        "ip": "0.0.0.0",
        "port": 8080,
        "db_name": "aichat.db",
        "web_dir": ""
    },
    "smtp": {
        "url": "smtp.qq.com:465",
        "from": "your_email@qq.com",
        "password": "your_smtp_authorization_code"
    },
    "models": {
        "deepseek_apikey": "sk-xxx",
        "chatgpt_apikey": "sk-xxx",
        "gemini_apikey": "xxx"
    },
    "proxy": {
        "set_proxy": false,
        "proxy_ip": "127.0.0.1",
        "proxy_port": 7890
    }
}
```

### 命令行参数

```bash
./aichat_server --config config.json                    # 使用配置文件
./aichat_server --port 9090 --deepseek_apikey=sk-xxx    # 命令行指定
./aichat_server --set_proxy --proxy_ip=127.0.0.1 --proxy_port=7890  # 启用代理
```

| 参数 | 说明 | 默认值 |
|------|------|--------|
| `--config` | JSON 配置文件路径 | 无 |
| `--ip` | 监听 IP | 0.0.0.0 |
| `--port` | 监听端口 | 8080 |
| `--db_name` | SQLite 数据库文件名 | aichat.db |
| `--web_dir` | 前端静态文件目录 | 自动检测 |
| `--smtp_url` | SMTP 服务器地址 | 无 |
| `--smtp_from` | 发件人邮箱 | 无 |
| `--smtp_password` | SMTP 授权码 | 无 |
| `--deepseek_apikey` | DeepSeek API Key | 无 |
| `--chatgpt_apikey` | ChatGPT API Key | 无 |
| `--gemini_apikey` | Gemini API Key | 无 |
| `--set_proxy` | 是否启用代理 | false |
| `--proxy_ip` | 代理 IP | 无 |
| `--proxy_port` | 代理端口 | 0 |


## 设计特点

- **Provider 抽象**：统一的模型适配接口，新增模型只需继承 `Provider` 实现三个方法
- **数据持久化**：SQLite 存储用户/会话/消息，服务重启不丢数据
- **时间轮定时器**：O(1) 复杂度管理验证码过期等定时任务
- **gflags + JSON 双配置**：命令行参数优先，配置文件兜底，灵活部署
- **FetchContent 自动依赖**：SDK 层依赖通过 CMake 自动下载，开箱即用
- **SSE 流式响应**：支持大模型流式输出，前端实时展示

## 已知限制

- 单线程 HTTP 服务（cpp-httplib 默认），高并发场景需要反向代理
- 无 TLS 终结，生产环境建议 Nginx 反代
- 密码仅做 SHA256 哈希，未加盐，生产环境建议使用 bcrypt/argon2 等抗彩虹表算法
- 无连接数限制和请求速率限制
- 时间轮精度为 1 秒，不适用于毫秒级超时

## 致谢

- [cpp-httplib](https://github.com/yhirose/cpp-httplib) — 轻量级 HTTP 库
- [jsoncpp](https://github.com/open-source-parsers/jsoncpp) — JSON 解析
- [spdlog](https://github.com/gabime/spdlog) — 高性能日志库
- [fmt](https://github.com/fmtlib/fmt) — 格式化库
- [gflags](https://github.com/gflags/gflags) — 命令行参数解析
- [Google Test](https://github.com/google/googletest) — 单元测试框架
