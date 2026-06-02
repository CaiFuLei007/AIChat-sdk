
#pragma once

/*
    - 对 所有工具进行组合
    - 成员 : 
            1) llmmanager 对大模型进行管理
            2) data_manager 对数据库进行管理
    - 接口 : 
            1) 添加模型
            2) 创建用户
            3) 获取用户信息
            4) 用户是否存在
            5) 创建 session
            6) 发送消息 , 全量返回 , 流失返回
            7) 创建会话
            8) 获取用户所有会话列表
            9) 获取会话详细信息
*/


#include "llmmanager.h"
#include "session_manager.h"
#include "base/common.h"

namespace ai_sdk
{

class AIChatSdk
{
    using Task = std::function<void()>;
    using MessageCallback = std::function<void(const std::string& message , bool finish)>;
private:
    std::unique_ptr<LLManager> llmanager_;
    std::unique_ptr<SessionManager> session_manager_;
public:
    AIChatSdk(const std::string &db_name)
    :llmanager_(std::make_unique<LLManager>()) , 
    session_manager_(std::make_unique<SessionManager>(db_name))
    {}

    bool RegisterModel(const Config &config);

    std::vector<ModelInfo> GetAllModels();

    bool HasUser(const std::string& email);
    std::shared_ptr<UserInfo> GetUser(const std::string& email , const std::string& password);
    std::string CreateUser(const std::string& email , const std::string& password);

    std::string SendMessage(const std::string& ssid , const std::string& message);
    std::string SendMessageStream(const std::string& ssid , const std::string& message , MessageCallback callback);

    std::string CreateSession(const std::string& uid, const std::string &model_name);
    std::vector<Session> GetUserAllSession(const std::string &uid);
    std::shared_ptr<Session> GetSession(const std::string & ssid);

    bool RemoveSession(const std::string& ssid);

    void AddTimerTask(const std::string& id, int timeout , Task task);
    void RemoveTask(const std::string &id);
};


} // end ai_sdk