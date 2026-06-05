    ================================================================
             AIChat Server — HTTP API 接口汇总
    ================================================================
    
    所有接口响应体均为 JSON 格式。
    通用响应结构：
      成功: {"code": 0,  "msg": "success", "data": {...}}
      失败: {"code": -1, "msg": "<错误信息>"}
    
    
    1. 发送验证码
    
    POST /api/verification
    
    请求体 (JSON):
      {
        "email": "string"    // 用户邮箱
      }
    
    响应体 (成功):
      { "code": 0, "msg": "success" }
    
    可能的错误信息:
      "反序列化错误"
      "发送验证码错误"
    
    说明: 向指定邮箱发送 5 位数字验证码，验证码 5 分钟后自动过期。
    
    
    2. 用户注册
    
    POST /api/register
    
    请求体 (JSON):
      {
        "email":    "string",   // 邮箱
        "code":     "string",   // 邮箱验证码
        "password": "string"    // 明文密码（内部 SHA256 哈希后存储）
      }
    
    响应体 (成功):
      {
        "code": 0,
        "msg": "success",
        "data": {
          "uid": "string"       // 用户唯一 ID
        }
      }
    
    可能的错误信息:
      "反序列化错误"
      "验证码超时"
      "验证码错误"
    
    
    3. 用户登录
    
    POST /api/login
    
    请求体 (JSON):
      {
        "email":    "string",   // 邮箱
        "password": "string"    // 明文密码
      }
    
    响应体 (成功):
      {
        "code": 0,
        "msg": "success",
        "data": {
          "uid":         "string",
          "email":       "string",
          "create_time": "string"   // 账号创建时间
        }
      }
    
    可能的错误信息:
      "反序列化错误"
      "用户未进行注册"
      "邮箱或密码错误"
    
    
    4. 获取所有可用模型
    
    GET /api/models
    
    请求体: 无
    
    响应体 (成功):
      {
        "code": 0,
        "msg": "success",
        "data": {
          "models": [
            {
              "model_name": "string",   // 模型名称
              "model_desc": "string"    // 模型描述
            },
            ...
          ]
        }
      }
    
    
    5. 获取用户的所有会话
    
    GET /api/users/:uid/sessions
    
    路径参数:
      uid  — 用户 ID
    
    请求体: 无
    
    响应体 (成功):
      {
        "code": 0,
        "msg": "success",
        "data": {
          "sessions": [
            {
              "session_id":  "string",
              "model_name":  "string",
              "create_time": "string",
              "update_time": "string",
              "last_message": "string"   // 最后一条消息内容（会话为空时无此字段）
            },
            ...
          ]
        }
      }
    
    
    6. 创建会话
    
    POST /api/sessions
    
    请求体 (JSON):
      {
        "uid":        "string",   // 用户 ID
        "model_name": "string"    // 使用的模型名称
      }
    
    响应体 (成功):
      {
        "code": 0,
        "msg": "success",
        "data": {
          "session_id": "string"   // 新创建的会话 ID
        }
      }
    
    可能的错误信息:
      "反序列化错误"
      "uid 为空"
      "模型名称为空"
    
    
    7. 删除会话
    
    DELETE /api/sessions/:ssid
    
    路径参数:
      ssid  — 会话 ID
    
    请求体: 无
    
    响应体 (成功):
      { "code": 0, "msg": "success" }
    
    可能的错误信息:
      "会话不存在"
    
    
    8. 获取会话的所有消息
    
    GET /api/sessions/:ssid/messages
    
    路径参数:
      ssid  — 会话 ID
    
    请求体: 无
    
    响应体 (成功):
      {
        "code": 0,
        "msg": "success",
        "data": {
          "session_id": "string",
          "model_name": "string",
          "messages": [
            {
              "mid":         "string",   // 消息 ID
              "role":        "string",   // 角色 (如 "user" / "assistant")
              "content":     "string",   // 消息内容
              "create_time": "string"    // 消息创建时间
            },
            ...
          ]
        }
      }
    
    可能的错误信息:
      "会话不存在"
    
    
    9. 发送消息 (同步)
    
    POST /api/sessions/:ssid/messages
    
    路径参数:
      ssid  — 会话 ID
    
    请求体 (JSON):
      {
        "content": "string"   // 用户发送的消息内容
      }
    
    响应体 (成功):
      {
        "code": 0,
        "msg": "success",
        "data": {
          "content": "string"   // AI 模型返回的完整回复
        }
      }
    
    可能的错误信息:
      "会话不存在"
      "反序列化错误"
      "消息为空"
      "模型未返回消息"
    
    
    10. 发送消息 (SSE 流式)
    
    POST /api/sessions/:ssid/messages/stream
    
    路径参数:
      ssid  — 会话 ID
    
    请求体 (JSON):
      {
        "content": "string"   // 用户发送的消息内容
      }
    
    响应头:
      Content-Type: text/event-stream
      Cache-Control: no-cache
      Connection: keep-alive
    
    响应体 (SSE 流式):
      每条数据格式为: data: {"content":"string","finish":bool}\n\n
    
      - content: 当前增量文本片段
      - finish:  是否结束
        - false = 还有后续内容
        - true  = 生成完毕，连接将关闭
    
    可能的错误信息 (非流式 JSON，发生在流式开始前):
      "会话不存在"
      "反序列化错误"
    
    ================================================================
    路由汇总表
    ================================================================
    
    | 方法   | 路径                                | 说明                |
    |--------|-------------------------------------|---------------------|
    | POST   | /api/verification                   | 发送邮箱验证码      |
    | POST   | /api/register                       | 用户注册            |
    | POST   | /api/login                          | 用户登录            |
    | GET    | /api/models                         | 获取所有模型        |
    | GET    | /api/users/:uid/sessions            | 获取用户会话列表    |
    | POST   | /api/sessions                       | 创建会话            |
    | DELETE | /api/sessions/:ssid                 | 删除会话            |
    | GET    | /api/sessions/:ssid/messages        | 获取会话全部消息    |
    | POST   | /api/sessions/:ssid/messages        | 发送消息 (同步)     |
    | POST   | /api/sessions/:ssid/messages/stream | 发送消息 (SSE 流式) |
    
