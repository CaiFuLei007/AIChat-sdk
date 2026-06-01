
#include "aichat_sdk.h"
#include "base/util/mylog.h"
#include "provider/gemini_provider.h"
#include "provider/chatgpt_provider.h"
#include "provider/deepseek_provider.h"

namespace ai_sdk
{
   
bool AIChatSdk::RegisterModel(const Config &config)
{
    std::unique_ptr<Provider> provider;
    switch (config.model_type)
    {
    case ModelType::DEEPSEEK:
        provider = std::make_unique<DeepSeekProvider>();
        break;
    
    case ModelType::GEMINI:
        provider = std::make_unique<GeminiProvider>();
        break;

    case ModelType::CHATGPT:
        provider = std::make_unique<ChatGPTProvider>();
        break;
        
    default:
        WARN("CAN'T REGISTER MODEL : {}" , config.model);
        return false;
        break;
    }
    
    if(!llmanager_->RegisterProvider(config.model , std::move(provider)))
    {
        return false;
    }
    if(!llmanager_->InitProvider(config.model , config))
    {
        return false;
    }
    return true;
}

std::vector<ModelInfo> AIChatSdk::GetAllModels()
{
    return llmanager_->GetAllModel();
}

bool AIChatSdk::HasUser(const std::string& name)
{
    return session_manager_->HasUserName(name);
}

std::shared_ptr<UserInfo> AIChatSdk::GetUser(const std::string& name , const std::string& password)
{   
    std::shared_ptr<UserInfo> user_info = session_manager_->GetUserInfo(name);
    if(!user_info || user_info->password != password)
    {
        return nullptr;
    }
    return user_info;
}
std::string AIChatSdk::CreateUser(const std::string& name , const std::string& password)
{
    return session_manager_->InsertNewUser(name , password);
}
std::string AIChatSdk::SendMessage(const std::string& ssid , const std::string& message)
{
    // 1. 先将本次信息加到 数据库中
    // 2. 发送数据
    // 3. 将结果加到数据库中
    // 4. 返回数据

    session_manager_->CreateNewMessage(ssid , "user" , message);
    std::vector<Message> messages = session_manager_->GetSessionAllMessage(ssid);
    std::shared_ptr<Session> session_info = session_manager_->GetSession(ssid);
    if(!session_info)
    {
        return "";
    }
    std::string ret = llmanager_->SendMessage(session_info->model_name , messages);
    if(ret.empty())
    {
        return ret;
    }
    session_manager_->CreateNewMessage(ssid , "assistant" , ret);

    return ret;
}
std::string AIChatSdk::SendMessageStream(const std::string& ssid , const std::string& message , MessageCallback callback)
{
    // 1. 先将本次信息加到 数据库中
    // 2. 发送数据
    // 3. 将结果加到数据库中
    // 4. 返回数据

    session_manager_->CreateNewMessage(ssid , "user" , message);
    std::vector<Message> messages = session_manager_->GetSessionAllMessage(ssid);
    std::shared_ptr<Session> session_info = session_manager_->GetSession(ssid);
    if(!session_info)
    {
        return "";
    }

    std::string ret = llmanager_->SendMessageStream(session_info->model_name , messages ,callback);
    if(ret.empty())
    {
        return ret;
    }
    session_manager_->CreateNewMessage(ssid , "assistant" , ret);

    return ret;
}
std::string AIChatSdk::CreateSession(const std::string& uid , const std::string &model_name)
{
    return session_manager_->CreateSession(uid , model_name);
}
std::vector<Session> AIChatSdk::GetUserAllSession(const std::string &uid)
{
    return session_manager_->GetUserAllSession(uid);
}
bool AIChatSdk::RemoveSession(const std::string& ssid)
{
    return session_manager_->RemoveSession(ssid);
}

} // end ai_sdk