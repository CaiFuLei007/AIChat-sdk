# 核心模块详解

本文档详细介绍 AIChat-sdk 各核心模块的设计、数据结构、接口和实现细节。

## 目录

- [架构总览](#架构总览)
- [基础数据结构](#基础数据结构)
- [AIChatSdk — 总入口](#aichatsdk--总入口)
- [LLManager — 多模型管理器](#llmanager--多模型管理器)
- [Provider — 模型适配层](#provider--模型适配层)
  - [Provider 抽象基类](#provider-抽象基类)
  - [DeepSeekProvider](#deepseekprovider)
  - [ChatGPTProvider](#chatgptprovider)
  - [GeminiProvider](#geminiprovider)
- [SessionManager — 会话/用户管理](#sessionmanager--会话用户管理)
- [DataManager — SQLite 数据持久化](#datamanager--sqlite-数据持久化)
- [TimerWheel — 时间轮定时器](#timerwheel--时间轮定时器)
- [Poller — epoll 封装](#poller--epoll-封装)
- [Logger — 日志模块](#logger--日志模块)
- [JsonUtil — JSON 工具](#jsonutil--json-工具)
- [Curl — SMTP 邮件发送](#curl--smtp-邮件发送)
- [AIChatServer — HTTP 服务层](#aichatserver--http-服务层)

---

## 架构总览

项目分为两层：**SDK 层** 和 **Server 层**。

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

- **AIChatSdk**：总入口，组合 LLManager 和 SessionManager
- **LLManager**：管理所有 LLM Provider，负责消息的全量/流式发送
- **Provider**：每个大模型的具体适配器（DeepSeek / ChatGPT / Gemini）
- **SessionManager**：管理用户和会话的内存缓存 + 定时淘汰
- **DataManager**：SQLite 数据持久化，管理 user / session / message 三张表
- **TimerWheel**：基于 timerfd + epoll 的时间轮，用于缓存过期、验证码超时等
- **Poller**：epoll 的轻量封装

---

## 基础数据结构

定义在 `sdk/include/base/common.h`，命名空间 `ai_sdk`。

### UserInfo

```cpp
struct UserInfo {
    std::string uid;          // UUID v4 格式，36 字符 (如 "a1b2c3d4-e5f6-7890-abcd-ef1234567890")
    std::string email;        // 用户邮箱，作为登录凭证
    std::string password;     // SHA256 哈希后的密码，64 字符十六进制字符串
    time_t create_time;       // 注册时间，Unix 时间戳
};
```

### Message

```cpp
struct Message {
    std::string ssid;         // 所属会话 ID
    std::string mid;          // 消息 ID，格式: "{ssid}_{微秒时间戳}"
    std::string role;         // 角色: "user" 或 "assistant"
    std::string content;      // 消息内容
    time_t create_time;       // 创建时间，Unix 时间戳
};
```

### Session

```cpp
struct Session {
    std::string uid;                  // 所属用户 ID
    std::string session_id;           // 会话 ID，格式: "{uid}_{微秒时间戳}"
    std::string model_name;           // 使用的模型名称 (如 "deepseek-v4-flash")
    std::vector<Message> messages;    // 会话内的所有消息（懒加载，从 DB 读取后缓存）
    time_t create_time;               // 创建时间
    time_t update_time;               // 最后更新时间
};
```

### ModelInfo

```cpp
struct ModelInfo {
    std::string model_name;   // 模型名称，如 "deepseek-v4-flash"
    std::string model_decs;   // 模型描述，如 "DeepSeek AI 模型"
};
```

### ModelType（枚举）

```cpp
enum class ModelType {
    DEEPSEEK,
    GEMINI,
    CHATGPT
};
```

### Config

```cpp
struct Config {
    ModelType model_type;             // 模型类型枚举
    ModelInfo model_info;             // 展示给用户的模型信息

    std::string end_point;            // API 端点 (如 "https://api.deepseek.com")
    std::string apikey;               // API 密钥

    std::string path;                 // 全量请求路径 (如 "/chat/completions")
    std::string streampath;           // 流式请求路径 (如 "/v1beta/models/...:streamGenerateContent?alt=sse")
    std::string model;                // 模型标识符 (如 "deepseek-v4-flash")

    double temperature = 1;           // 温度参数，范围 0~2
    int max_token = 4096;             // 最大输出 token 数
    double top_p = 1;                 // Top-P 采样参数
    std::string reasoning_effort = "high";  // DeepSeek 专用：推理强度

    struct ProxyConfig {
        bool set_proxy = false;       // 是否启用代理
        std::string proxy_ip;         // 代理 IP
        uint16_t proxy_port = 0;      // 代理端口
    } proxy;
};
```

---

## AIChatSdk — 总入口

**文件**：`sdk/include/aichat_sdk.h` + `sdk/src/aichat_sdk.cc`

组合 `LLManager` 和 `SessionManager`，提供面向业务的统一接口。

### 成员

```cpp
class AIChatSdk {
    std::unique_ptr<LLManager> llmanager_;          // LLM 模型管理
    std::unique_ptr<SessionManager> session_manager_; // 会话/用户管理
};
```

### 构造函数

```cpp
AIChatSdk(const std::string &db_name);
```

- 创建 LLManager 和 SessionManager
- SessionManager 构造时会初始化 SQLite 数据库（创建三张表）并启动时间轮

### 接口详解

#### RegisterModel

```cpp
bool RegisterModel(const Config &config);
```

注册一个 LLM 模型。内部流程：
1. 根据 `config.model_type` 创建对应的 Provider 实例（`DeepSeekProvider` / `ChatGPTProvider` / `GeminiProvider`）
2. 调用 `LLManager::RegisterProvider()` 将 Provider 注册到哈希表
3. 调用 `LLManager::InitProvider()` 初始化 Provider（设置 endpoint、apikey 等）

#### GetAllModels

```cpp
std::vector<ModelInfo> GetAllModels();
```

返回所有已注册且初始化成功的模型信息列表。

#### 用户相关

```cpp
bool HasUser(const std::string& email);
// 检查用户是否存在，先查内存缓存，再查 SQLite

std::shared_ptr<UserInfo> GetUser(const std::string& email, const std::string& password);
// 获取用户信息，密码不匹配返回 nullptr

std::string CreateUser(const std::string& email, const std::string& password);
// 创建用户，返回 uid；邮箱已存在则返回空字符串
// 内部会设置 10 分钟定时任务，超时后清除内存缓存
```

#### 会话相关

```cpp
std::string CreateSession(const std::string& uid, const std::string &model_name);
// 创建新会话，返回 session_id
// 内部会设置 10 分钟定时任务，超时后清除内存缓存

std::vector<Session> GetUserAllSession(const std::string &uid);
// 获取用户的所有会话列表

std::shared_ptr<Session> GetSession(const std::string &ssid);
// 获取单个会话详情（含消息列表）

bool RemoveSession(const std::string& ssid);
// 删除会话（同时删除 SQLite 中的会话和所有消息）
```

#### SendMessage — 全量返回

```cpp
std::string SendMessage(const std::string& ssid, const std::string& message);
```

发送消息并等待完整回复。内部流程：
1. 将用户消息写入 SQLite（`CreateNewMessage(ssid, "user", message)`）
2. 从缓存/DB 加载该会话的所有历史消息
3. 根据会话绑定的模型名称，调用对应 Provider 的 `SendMessage()`
4. 将 AI 回复写入 SQLite（`CreateNewMessage(ssid, "assistant", ret)`）
5. 返回 AI 回复内容

#### SendMessageStream — 流式返回

```cpp
std::string SendMessageStream(const std::string& ssid, const std::string& message, MessageCallback callback);
```

发送消息并流式接收回复。流程与全量返回相同，但步骤 3 调用 Provider 的 `SendMessageStream()`，每收到一个 token 都会触发 `callback(content, false)`，收到结束信号时触发 `callback("", true)`。最终将完整回复拼接后写入 SQLite。

#### 定时任务

```cpp
void AddTimerTask(const std::string& id, int timeout, Task task);
// 添加定时任务，timeout 单位为秒

void RemoveTask(const std::string &id);
// 取消定时任务
```

---

## LLManager — 多模型管理器

**文件**：`sdk/include/llmmanager.h` + `sdk/src/llmanager.cc`

管理所有 LLM Provider 的注册、初始化和消息发送。

### 成员

```cpp
class LLManager {
    std::unordered_map<std::string, std::unique_ptr<Provider>> providers_;    // 模型名 → Provider 实例
    std::unordered_map<std::string, bool> provider_status_;                   // 模型名 → 是否已初始化
    std::unordered_map<std::string, ModelInfo> model_info_;                   // 模型名 → 模型信息
};
```

三个哈希表分别存储：
- `providers_`：模型名称到 Provider 实例的映射，拥有 Provider 的所有权（`unique_ptr`）
- `provider_status_`：模型是否已成功初始化（`Init()` 返回 true 后置为 true）
- `model_info_`：模型的展示信息（名称 + 描述），用于 `GetAllModel()` 返回给前端

### 接口

#### RegisterProvider

```cpp
bool RegisterProvider(const std::string &model, std::unique_ptr<Provider> provider);
```

注册一个 Provider 实例。如果同名模型已注册，打印警告并返回 false。注册后 `provider_status_` 默认为 false（未初始化）。

#### InitProvider

```cpp
bool InitProvider(const std::string& model, const Config &config);
```

初始化已注册的 Provider。调用 Provider 的 `Init(config)` 方法，成功后将 `provider_status_` 置为 true，并记录 `model_info_`。

#### SendMessage / SendMessageStream

```cpp
std::string SendMessage(const std::string& model, const std::vector<Message> &messages);
std::string SendMessageStream(const std::string& model, const std::vector<Message> &messages, MessageCallback message_cb);
```

根据模型名查找 Provider，检查已注册且已初始化后，委托给对应 Provider 执行。

#### GetAllModel

```cpp
std::vector<ModelInfo> GetAllModel();
```

遍历 `model_info_` 哈希表，返回所有已注册模型的信息列表。

---

## Provider — 模型适配层

### Provider 抽象基类

**文件**：`sdk/include/provider/provider.h`

```cpp
class Provider {
protected:
    Config config_;           // 模型配置
    bool is_avaiable_ = false; // 是否已初始化成功

public:
    virtual bool Init(Config config) = 0;                    // 初始化
    virtual std::string SendMessage(const std::vector<Message> &messages) = 0;              // 全量返回
    virtual std::string SendMessageStream(const std::vector<Message> &messages, MessageCallback message_cb) = 0; // 流式返回

    bool IsAvaiable();        // 是否可用
    std::string ModelName();  // 模型名称
    std::string ModelDesc();  // 模型描述
};
```

所有 Provider 继承此基类，实现三个纯虚函数。`MessageCallback` 类型为：
```cpp
using MessageCallback = std::function<void(const std::string& mes, bool finish)>;
```

### DeepSeekProvider

**文件**：`sdk/include/provider/deepseek_provider.h` + `sdk/src/provider/deepseek_provider.cc`

**默认配置**：
- `end_point`：`https://api.deepseek.com`
- `model`：`deepseek-v4-flash`
- `path`：`/chat/completions`

**Init()**：校验 apikey 非空，填充默认值，设置 `is_avaiable_ = true`。

**请求体构建**（`BuildResponseBody`）：
```json
{
    "messages": [{"content": "...", "role": "user"}, ...],
    "model": "deepseek-v4-flash",
    "max_tokens": 4096,
    "reasoning_effort": "high",
    "stream": false,
    "temperature": 1.0,
    "top_p": 1.0
}
```

**全量返回**（`SendMessage`）：
1. 构建请求体，设置 `stream: false`
2. 创建 `httplib::Client`，设置请求头 `Authorization: Bearer {apikey}`
3. 如果配置了代理，调用 `client.set_proxy()`
4. `client.Post(path, headers, body, "application/json")`
5. 解析响应：`response["choices"][0]["message"]["content"]`

**流式返回**（`SendMessageStream`）：
1. 构建请求体，设置 `stream: true`
2. 使用 `httplib::Request` 的 `content_receiver` 回调逐块接收
3. 手动缓冲数据，按 `\n\n` 分割 SSE 事件
4. 每个事件格式：`data: {json}`，解析 `choices[0]["delta"]["content"]`
5. 收到 `data: [DONE]` 时触发 `message_cb("", true)` 结束
6. 拼接所有 delta 返回完整消息

**SSE 解析细节**：
- 去除 `\r` 字符，只保留 `\n`
- 按 `\n\n` 分割事件块
- 每块必须以 `data: ` 前缀开头
- `[DONE]` 表示流结束

### ChatGPTProvider

**文件**：`sdk/include/provider/chatgpt_provider.h` + `sdk/src/provider/chatgpt_provider.cc`

**默认配置**：
- `end_point`：`https://api.openai.com`
- `model`：`gpt-5.4`
- `path`：`/v1/responses`

**请求体构建**：
```json
{
    "input": [{"content": "...", "role": "user"}, ...],
    "model": "gpt-5.4",
    "max_output_tokens": 4096,
    "stream": false,
    "temperature": 1.0,
    "top_p": 1.0
}
```

注意：使用 OpenAI 的 Responses API（`/v1/responses`），而非 Chat Completions API。请求体字段名不同：
- 消息用 `input` 而非 `messages`
- 最大 token 用 `max_output_tokens` 而非 `max_tokens`

**全量返回响应解析**：`response["output"][0]["content"][0]["text"]`

**流式返回 SSE 解析**：
- ChatGPT 的 SSE 格式与 DeepSeek 不同，包含 `event:` 行和 `data:` 行
- 事件格式：
  ```
  event: response.output_text.delta
  data: {"delta": "你好"}
  ```
- 关注的事件类型：
  - `response.output_text.delta`：增量文本，解析 `delta` 字段
  - `response.output_text.done`：文本完成，解析 `text` 字段获取完整文本
  - `response.completed`：整个响应完成，触发 `message_cb("", true)`
- 其他事件类型（如 `response.created`、`response.in_progress` 等）直接跳过

### GeminiProvider

**文件**：`sdk/include/provider/gemini_provider.h` + `sdk/src/provider/gemini_provider.cc`

**默认配置**：
- `end_point`：`https://generativelanguage.googleapis.com`
- `model`：`gemini-3.5-flash`
- `path`：`/v1beta/models/gemini-3.5-flash:generateContent`
- `streampath`：`/v1beta/models/gemini-3.5-flash:streamGenerateContent?alt=sse`

**请求体构建**：
```json
{
    "contents": [
        {
            "parts": [{"text": "..."}],
            "role": "user"
        }
    ],
    "generationConfig": {
        "maxOutputTokens": 4096,
        "temperature": 1.0,
        "topP": 1.0
    }
}
```

注意 Gemini 的消息格式与其他模型不同：
- 用 `contents` 而非 `messages`
- 每条消息的文本放在 `parts` 数组中（`parts[0].text`）
- 参数放在 `generationConfig` 对象中，字段名使用 camelCase（`maxOutputTokens`、`topP`）

**API 认证**：使用 `x-goog-api-key` 请求头，而非 `Authorization: Bearer`。

**全量返回响应解析**：`response["candidates"][0]["content"]["parts"][0]["text"]`

**流式返回 SSE 解析**：
- 与 DeepSeek 类似，使用 `data: ` 前缀
- 解析路径：`candidates[0]["content"]["parts"][0]["text"]`
- 结束判断：当 `candidates[0]` 中包含 `finishReason` 字段时，表示流结束
- 注意：结束时的事件可能没有 `content` 字段（只有 `finishReason`），需要特殊处理

**Gemini 独有**：全量和流式使用不同的路径（`path` vs `streampath`），其他模型全量/流式使用同一路径，通过请求体中的 `stream` 字段区分。

### 三个 Provider 的对比

| 特性 | DeepSeek | ChatGPT | Gemini |
|------|----------|---------|--------|
| 端点 | `api.deepseek.com` | `api.openai.com` | `generativelanguage.googleapis.com` |
| API 风格 | Chat Completions | Responses API | generateContent |
| 消息字段 | `messages` | `input` | `contents` |
| 文本字段 | `message.content` | `content[0].text` | `parts[0].text` |
| 认证方式 | `Authorization: Bearer` | `Authorization: Bearer` | `x-goog-api-key` |
| 流式结束标志 | `data: [DONE]` | `event: response.completed` | `finishReason` 字段 |
| 流式路径 | 同全量路径 | 同全量路径 | 独立 `streampath` |
| 代理支持 | 有 | 有 | 有 |
| 特殊参数 | `reasoning_effort` | — | `generationConfig` |

---

## SessionManager — 会话/用户管理

**文件**：`sdk/include/session_manager.h` + `sdk/src/session_manager.cc`

管理用户和会话的内存缓存，配合 TimerWheel 实现缓存自动淘汰，底层通过 DataManager 持久化到 SQLite。

### 成员

```cpp
class SessionManager {
    std::unordered_map<std::string, std::shared_ptr<UserInfo>> user_table_;     // email → 用户信息
    std::unordered_map<std::string, std::shared_ptr<Session>> session_table_;   // ssid → 会话信息
    std::unique_ptr<DataManager> data_manager_;   // SQLite 数据管理
    std::unique_ptr<TimerWheel> timer_wheel_;     // 时间轮定时器
    std::mutex mutex_;                            // 保护所有共享数据
};
```

### 缓存淘汰机制

SessionManager 采用 **内存缓存 + 定时淘汰 + 磁盘兜底** 的策略：

1. 用户/会话首次访问时从 SQLite 加载到内存哈希表
2. 同时设置一个 **10 分钟** 的定时任务（通过 TimerWheel）
3. 每次访问时更新定时任务（`UpdateTask`），重置倒计时
4. 10 分钟无访问，定时任务触发，将哈希表中的指针置为 `nullptr`（释放内存）
5. 下次访问时发现指针为空，重新从 SQLite 加载

```cpp
// 设置定时任务的典型用法
timer_wheel_->SetTask(uid, 10, [this, email]() {
    std::unique_lock<std::mutex> lock(mutex_);
    auto it = user_table_.find(email);
    if (it == user_table_.end()) return;
    it->second = nullptr;  // 释放内存，不清除哈希表条目
});
```

这种设计的好处：
- 热数据常驻内存，避免频繁 DB 查询
- 冷数据自动释放，控制内存占用
- 哈希表条目保留（value 为 nullptr），下次访问时可以快速定位并重新加载

### ID 生成

#### 用户 ID（UUID v4）

```cpp
std::string CreateUserId();
```

手写的 UUID v4 生成器，格式 `xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx`：
- 使用 `std::mt19937` 随机引擎（`thread_local`，线程安全）
- 第 14 位固定为 `4`（版本号）
- 第 19 位只能是 `8/9/a/b`（变体号）
- 预分配 36 字节字符串，默认填充 `-`，避免动态拼接

#### 会话 ID

```cpp
std::string GetssionId(const std::string& uid);
// 格式: "{uid}_{微秒时间戳}"
```

#### 消息 ID

```cpp
std::string Getmid(const std::string& ssid);
// 格式: "{ssid}_{微秒时间戳}"
```

时间戳使用 `std::chrono::microseconds` 精度，保证同一会话内消息 ID 的唯一性和有序性。

### 接口详解

#### InsertNewUser

```cpp
std::string InsertNewUser(const std::string &email, const std::string& password);
```

1. 加锁
2. 检查邮箱是否已存在（`HasUserName`）
3. 生成 UUID，创建 `UserInfo` 结构体
4. 写入 SQLite
5. 加入内存缓存 `user_table_`
6. 设置 10 分钟淘汰定时任务
7. 返回 uid

#### HasUserName

```cpp
bool HasUserName(const std::string& email);
```

先查 `user_table_` 内存缓存，未命中再查 SQLite。如果在 DB 中找到，会自动加载到内存缓存。

#### GetUserInfo

```cpp
std::shared_ptr<UserInfo> GetUserInfo(const std::string& email);
```

1. 先查内存缓存
2. 如果缓存中有且指针非空，更新定时任务（续期 10 分钟）并返回
3. 如果缓存中指针为空（已被淘汰），从 SQLite 重新加载，重建定时任务

#### CreateSession

```cpp
std::string CreateSession(const std::string &uid, const std::string& model_name);
```

1. 生成 session_id（`uid_微秒时间戳`）
2. 写入 SQLite
3. 加入内存缓存
4. 设置 10 分钟淘汰定时任务

#### CreateNewMessage

```cpp
bool CreateNewMessage(const std::string &ssid, const std::string& role, const std::string& content);
```

1. 生成消息 ID（`ssid_微秒时间戳`）
2. **先**调用 `GetSessionUnLock()` 确保 messages 缓存已加载（避免后续从 DB 读到刚插入的消息造成重复）
3. 写入 SQLite
4. 追加到内存中 Session 的 messages 向量

#### RemoveSession

```cpp
bool RemoveSession(const std::string& ssid);
```

1. 从 SQLite 删除会话及其所有消息（DataManager 内部用事务保证原子性）
2. 从内存缓存中移除

---

## DataManager — SQLite 数据持久化

**文件**：`sdk/include/data_manager.h` + `sdk/src/data_manager.cc`

SQLite 数据库管理，负责 user / session / message 三张表的 CRUD 操作。

### 数据库表结构

#### USER 表

| 列名 | 类型 | 约束 | 说明 |
|------|------|------|------|
| ID | INTEGER | PRIMARY KEY AUTOINCREMENT | 自增主键 |
| UID | CHAR(36) | NOT NULL UNIQUE | 用户 UUID |
| USER_NAME | VARCHAR(64) | NOT NULL | 用户邮箱 |
| PASSWORD | CHAR(64) | NOT NULL | SHA256 哈希密码 |
| CREATE_TIME | INT | NOT NULL | 注册时间戳 |

#### SESSION 表

| 列名 | 类型 | 约束 | 说明 |
|------|------|------|------|
| ID | INTEGER | PRIMARY KEY AUTOINCREMENT | 自增主键 |
| UID | CHAR(36) | NOT NULL | 所属用户 UUID |
| SSID | CHAR(36) | NOT NULL UNIQUE | 会话 ID |
| MODEL_NAME | VARCHAR(30) | NOT NULL | 模型名称 |
| CREATE_TIME | INT | NOT NULL | 创建时间戳 |
| UPDATE_TIME | INT | NOT NULL | 最后更新时间戳 |

#### MESSAGE 表

| 列名 | 类型 | 约束 | 说明 |
|------|------|------|------|
| ID | INTEGER | PRIMARY KEY AUTOINCREMENT | 自增主键 |
| MID | CHAR(36) | NOT NULL UNIQUE | 消息 ID |
| SSID | CHAR(36) | NOT NULL | 所属会话 ID |
| ROLE | VARCHAR(30) | NOT NULL | 角色 ("user"/"assistant") |
| CONTENT | TEXT | NOT NULL | 消息内容 |
| CREATE_TIME | INT | NOT NULL | 创建时间戳 |

### 线程安全

每个公共方法内部都通过 `std::unique_lock<std::mutex>` 加锁。所有 SQLite 操作使用 **预编译语句**（`sqlite3_prepare_v2` + `sqlite3_bind_*` + `sqlite3_step` + `sqlite3_finalize`），避免 SQL 注入。

### 事务支持

删除用户和删除会话时使用事务（`BEGIN` / `COMMIT` / `ROLLBACK`）保证原子性：

- **删除用户**（`RemoveUser`）：先删除该用户的所有 Session（及其 Message），再删除 User 记录。任何步骤失败都会回滚。
- **删除会话**（`RemoveSession`）：先删除该会话的所有 Message，再删除 Session 记录。任何步骤失败都会回滚。

### 接口列表

```cpp
// 初始化：创建三张表
bool Init();

// 用户操作
bool InsertUser(const UserInfo &user_info);
bool RemoveUser(const std::string &uid);
UserInfo GetUser(const std::string& email);

// 会话操作
bool InsertSession(const Session& session);
bool RemoveSession(const std::string &ssid);
bool RemoveUserAllSession(const std::string &uid);
bool UpdateSession(const std::string &ssid);
Session GetSession(const std::string &ssid);
std::vector<std::string> GetUserAllSessions(const std::string &uid);

// 消息操作
bool InsertMessage(const Message& message);
bool RemoveMessage(const std::string& mid);
std::vector<Message> GetMessages(const std::string& ssid);
bool RemoveSessionAllMessage(const std::string &ssid);

// 查询所有
std::vector<std::string> GetAllSessions();
std::vector<std::string> GetAllUserName();
```

---

## TimerWheel — 时间轮定时器

**文件**：`sdk/include/base/timerwheel.h` + `sdk/src/base/timerwheel.cc`

基于 Linux `timerfd` + `epoll` 实现的时间轮定时器，用于管理定时任务（缓存淘汰、验证码过期等）。

### 原理

时间轮是一个 60 槽的环形数组，每个槽存放一组 `TimeTask` 共享指针。一个 `tick_` 指针每隔 1 秒前进一格，到达某个槽时，该槽中所有任务的引用被释放（`swap` 清空），触发 `TimeTask` 析构函数中的回调。

```
  槽 0   槽 1   槽 2  ...  槽 59
[task]  [    ]  [task]    [task]
              ↑ tick_ (当前指针)
```

### TimeTask

```cpp
class TimeTask {
    std::string id_;          // 任务 ID
    size_t timeout_;          // 超时时间（秒）
    Task task_;               // 回调函数
    bool is_cancel_;          // 是否已取消

public:
    TimeTask(const std::string& id, size_t timeout, Task task);
    ~TimeTask();              // 析构时：如果未取消，执行回调
    void Cancel();            // 取消任务（设置 is_cancel_ = true）
    size_t Timeout();         // 获取超时时间
};
```

关键设计：**任务的执行依赖于 `shared_ptr` 的引用计数**。当时间轮槽中的 `shared_ptr` 被清除时，如果 `tasks_` 哈希表中的 `weak_ptr` 也已失效（即没有其他地方持有该任务），`TimeTask` 析构函数被触发，执行回调。

### TimerWheel

```cpp
class TimerWheel {
    std::unordered_map<std::string, std::weak_ptr<TimeTask>> tasks_;   // id → 弱引用
    std::vector<std::vector<std::shared_ptr<TimeTask>>> wheel_;        // 60 槽时间轮
    size_t tick_;                        // 当前指针位置
    int timerfd_;                        // timerfd 文件描述符
    std::atomic<bool> running_;          // 运行标志
    std::thread worker_thread_;          // 工作线程
    std::mutex mutex_;                   // 保护所有共享数据
    std::unique_ptr<Poller> poller_;     // epoll 封装
};
```

#### 构造函数

```cpp
TimerWheel();
```

- 创建 60 槽的时间轮（`wheel_(60)`）
- 创建 `timerfd`（`CLOCK_MONOTONIC`, `TFD_NONBLOCK | TFD_CLOEXEC`）
- 创建 Poller（epoll 实例）

#### Ready()

```cpp
void Ready();
```

启动定时器：
1. 设置 `timerfd` 每 60 秒触发一次（`it_interval = 60s`）
2. 将 `timerfd` 注册到 epoll
3. 启动工作线程，运行 `ThreadCallback()`

#### ThreadCallback() — 工作线程

```cpp
void ThreadCallback();
```

循环调用 `poller_->EpollWait(1000)`，等待 `timerfd` 就绪事件。每次 `timerfd` 触发时，读取过期次数 `expirations`，然后推进 `tick_` 指针相应次数，清理对应槽的任务。

注意：`timerfd` 的间隔是 60 秒，而时间轮有 60 槽，所以每个槽恰好代表 1 秒的精度。但 `SetTask` 中的 `timeout` 参数单位是"槽位数"（即秒数），任务被放到 `(tick_ + timeout) % 60` 的位置。

#### SetTask

```cpp
void SetTask(const std::string& id, size_t timeout, Task task);
```

1. 加锁
2. 如果同 ID 的旧任务存在，取消它
3. 创建新的 `TimeTask`，用 `shared_ptr` 持有
4. 计算目标槽位：`(tick_ + timeout) % 60`
5. 将 `shared_ptr` 放入目标槽
6. 在 `tasks_` 中保存 `weak_ptr`
7. 包装回调：任务触发时先从 `tasks_` 中清除自身，再执行用户回调

#### UpdateTask

```cpp
void UpdateTask(const std::string& id);
```

续期任务：找到 `tasks_` 中的 `weak_ptr`，如果未失效，将 `shared_ptr` 复制一份放到新的槽位 `(tick_ + timeout) % 60`。旧槽位中的 `shared_ptr` 会在时间轮推进时自动释放。

注意：UpdateTask 不会取消旧槽位中的 `shared_ptr`，只是在新槽位增加一个引用。当旧槽位被清理时，如果新槽位仍持有引用，`TimeTask` 不会被析构。

#### CancelTask

```cpp
void CancelTask(const std::string& id);
```

取消任务：调用 `task->Cancel()` 设置取消标志，从 `tasks_` 中移除 `weak_ptr`。

---

## Poller — epoll 封装

**文件**：`sdk/include/base/poller.h` + `sdk/src/base/poller.cc`

对 Linux epoll 的轻量封装，被 TimerWheel 使用。

### 成员

```cpp
class Poller {
    int epollfd_;                     // epoll 文件描述符
    std::unordered_set<int> fds_;     // 已注册的 fd 集合（用于判断 ADD 还是 MOD）
};
```

### 接口

```cpp
// 构造：epoll_create(1)
Poller();

// 析构：close(epollfd_)
~Poller();

// 获取 epoll fd
int Fd();

// 添加或更新事件
// 如果 fd 未注册 → EPOLL_CTL_ADD
// 如果 fd 已注册 → EPOLL_CTL_MOD
int UpdateEvent(int fd, int events);

// 移除事件
// EPOLL_CTL_DEL + 从 fds_ 集合中移除
int RemoveEvent(int fd);

// 等待事件，返回 {(fd, events), ...}
// 自动处理 EINTR（被信号中断时重试）
// 最多返回 1024 个事件
std::vector<std::pair<int, int>> EpollWait(int wait_time);
```

---

## Logger — 日志模块

**文件**：`sdk/include/base/util/mylog.h` + `sdk/src/base/util/mylog.cc`

对 spdlog 的封装，提供全局单例日志器和便捷宏。

### 使用方式

```cpp
// 初始化（程序启动时调用一次）
ai_sdk::Logger::initLogger("aichat_server", "stdout", spdlog::level::debug);

// 使用宏记录日志（自动附带文件名和行号）
TRACE("详细跟踪信息");
DBG("调试信息");
INFO("普通信息");
WARN("警告信息");
ERR("错误信息");
CRIT("严重错误");
```

### 宏展开

```cpp
#define INFO(format, ...) ai_sdk::Logger::getLogger()->info("[{:>10s}:{:<4d}]" format, __FILE__, __LINE__, ##__VA_ARGS__)
```

输出示例：
```
[   main.cc:118] 已加载配置文件: config.json
[   main.cc:187] 已注册模型: deepseek
```

---

## JsonUtil — JSON 工具

**文件**：`sdk/include/base/util/json_util.h` + `sdk/src/base/util/json_util.cc`

对 jsoncpp 的简单封装。

```cpp
class JsonUtil {
public:
    // JSON 字符串 → Json::Value
    static void unserialize(const std::string& json, ::Json::Value& value);

    // Json::Value → JSON 字符串（紧凑格式，无缩进）
    static void serialize(const ::Json::Value& json, std::string& json_str);
};
```

内部使用 `Json::CharReaderBuilder` 和 `Json::StreamWriterBuilder`，设置 `indentation = ""` 生成紧凑 JSON。

---

## Curl — SMTP 邮件发送

**文件**：`aichat_server/src/curl_util.hpp`

基于 libcurl 实现的 SMTP 邮件发送工具，用于发送邮箱验证码。

### Curl_Base（抽象基类）

```cpp
class Curl_Base {
protected:
    Config_info config_;    // SMTP 配置（url, from, password）
public:
    virtual int Send(const std::vector<std::string> &clients, const std::string &message) = 0;
};
```

### Curl（实现类）

```cpp
class Curl : public Curl_Base {
public:
    Curl(const Config_info &config);  // 构造时调用 curl_global_init()
    ~Curl();                          // 析构时调用 curl_global_cleanup()

    int Send(const std::vector<std::string> &clients, const std::string &message) override;
};
```

#### Send() 流程

1. `curl_easy_init()` 创建句柄
2. 设置 SMTP URL（`CURLOPT_URL`）
3. 设置用户名/密码（`CURLOPT_USERNAME` / `CURLOPT_PASSWORD`）
4. 启用上传模式（`CURLOPT_UPLOAD`）
5. 设置发件人（`CURLOPT_MAIL_FROM`）
6. 设置收件人列表（`CURLOPT_MAIL_RCPT`）
7. 设置读取回调（`CURLOPT_READFUNCTION`），从 `stringstream` 读取邮件内容
8. `curl_easy_perform()` 发送
9. 清理资源

#### Config_info

```cpp
struct Config_info {
    std::string username;     // 用户名（未使用）
    std::string password;     // SMTP 授权码
    std::string url;          // SMTP 服务器地址（如 "smtp.qq.com:465"）
    std::string from;         // 发件人邮箱
};
```

---

## AIChatServer — HTTP 服务层

**文件**：`aichat_server/src/aichat_server.h` + `aichat_server/src/aichat_server.cc` + `aichat_server/src/main.cc`

基于 cpp-httplib 的 HTTP 服务器，提供 RESTful API 和静态文件服务。

### 成员

```cpp
class AIChatServer {
    std::mutex mutex_;                              // 保护验证码哈希表
    httplib::Server server_;                        // httplib 服务器实例
    ai_sdk::AIChatSdk ai_sdk_;                      // SDK 实例
    std::unordered_map<std::string, std::string> verification_code_;  // email → 验证码
    std::unique_ptr<util::Curl> curl_;              // SMTP 邮件发送
    std::string web_dir_;                           // 静态文件目录
    std::string mount_point_;                       // 挂载点（默认 "/"）
};
```

### 邮箱验证码流程

```
用户请求发送验证码 → 生成 5 位随机数 → 通过 SMTP 发送 → 存入 verification_code_ 哈希表
                                                           ↓
                                              设置 5 分钟定时任务（到期自动删除）
                                                           ↓
用户提交注册 → 比对验证码 → 通过则创建用户 → 从哈希表中删除验证码
```

### 密码安全

注册和登录时都对密码进行 SHA256 哈希处理：

```cpp
unsigned char hash_password[SHA256_DIGEST_LENGTH];
SHA256((const unsigned char *)password.c_str(), password.size(), hash_password);

// 二进制哈希 → 十六进制字符串（避免 null 字节截断）
static const char hex_chars[] = "0123456789abcdef";
std::string hex_password;
hex_password.reserve(SHA256_DIGEST_LENGTH * 2);  // 64 字符
for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
    hex_password.push_back(hex_chars[(hash_password[i] >> 4) & 0x0F]);
    hex_password.push_back(hex_chars[hash_password[i] & 0x0F]);
}
```

### SSE 流式响应

`HandleSendMessageStream` 使用 httplib 的 `set_chunked_content_provider` 实现 SSE（Server-Sent Events）：

```cpp
response.set_header("Content-Type", "text/event-stream");
response.set_header("Cache-Control", "no-cache");
response.set_header("Connection", "keep-alive");

response.set_chunked_content_provider("text/event-stream",
    [&](size_t offset, httplib::DataSink &sink) {
        // 通过 message_callback 将每个 token 写入 SSE
        // 格式: "data: {json}\n\n"
        // 完成时调用 sink.done()
    });
```

每条 SSE 数据的 JSON 格式：
```json
{"content": "你", "finish": false}
{"content": "好", "finish": false}
{"content": "", "finish": true}
```

### 配置加载（main.cc）

支持 **gflags 命令行参数** + **JSON 配置文件** 双模式，命令行优先：

```bash
# 方式 1：配置文件
./aichat_server --config config.json

# 方式 2：命令行参数
./aichat_server --port 9090 --deepseek_apikey=sk-xxx

# 方式 3：混合使用（命令行覆盖配置文件）
./aichat_server --config config.json --port 9090
```

加载流程：
1. `gflags::ParseCommandLineFlags()` 解析命令行
2. 如果指定了 `--config`，加载 JSON 配置文件
3. 对每个配置项：如果命令行值为空/0，则从配置文件中读取
4. 设置默认值（ip: `0.0.0.0`, port: `8080`, db_name: `aichat.db`）
5. 根据 API Key 是否存在，动态注册模型
6. 设置静态文件目录（默认为可执行文件的 `../../web`）
7. 启动服务器

### 静态文件服务

```cpp
server_.set_mount_point("/", web_dir);
```

在 API 路由注册之后挂载，确保 API 路由优先匹配。如果静态文件目录不存在，以纯 API 模式运行。

### 路由列表

| 方法 | 路径 | Handler | 说明 |
|------|------|---------|------|
| POST | `/api/verification` | `HandleGetVerification` | 发送邮箱验证码 |
| POST | `/api/register` | `HandleRegister` | 用户注册 |
| POST | `/api/login` | `HandleLogin` | 用户登录 |
| GET | `/api/models` | `HandleGetModels` | 获取模型列表 |
| GET | `/api/users/:uid/sessions` | `HandleGetUserSessions` | 获取用户会话列表 |
| POST | `/api/sessions` | `HandleCreateSession` | 创建会话 |
| DELETE | `/api/sessions/:ssid` | `HandleRemoveSession` | 删除会话 |
| GET | `/api/sessions/:ssid/messages` | `HandleGetSessionAllMessage` | 获取会话消息 |
| POST | `/api/sessions/:ssid/messages` | `HandleSendMessage` | 发送消息（全量） |
| POST | `/api/sessions/:ssid/messages/stream` | `HandleSendMessageStream` | 发送消息（流式） |
