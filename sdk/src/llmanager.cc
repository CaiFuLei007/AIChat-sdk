
#include "llmmanager.h"
#include "base/util/mylog.h"

namespace aichat_sdk
{

bool LLManager::RegisterProvider(const std::string &model , std::unique_ptr<Provider> provider)
{
    // 写锁 : 修改容器
    std::unique_lock<std::shared_mutex> lock(mutex_);

    auto it = providers_.find(model);
    if(it != providers_.end())
    {
        WARN("{} HAS REGISTER", model);
        return false;
    }

    providers_.emplace(model , std::move(provider));
    provider_status_.emplace(model , false);
    return true;
}

bool LLManager::InitProvider(const std::string& model , const Config &config)
{
    // 写锁 : 修改容器
    std::unique_lock<std::shared_mutex> lock(mutex_);

    auto it = providers_.find(model);
    if(it == providers_.end())
    {
        WARN("{} NOT REGISTER", model);
        return false;
    }
    auto ret = it->second->Init(config);
    if(!ret)
    {
        return false;
    }
    provider_status_[model] = true;
    model_info_.emplace(model , config.model_info);
    return true;
}

std::string LLManager::SendMessage(const std::string& model , const std::vector<Message> &messages)
{
    // 读锁 : 容器只读, 发送消息期间持锁保证 provider 生命周期安全, 多个消息可并发发送
    std::shared_lock<std::shared_mutex> lock(mutex_);

    // 发送消息 , 全量返回
    auto it = providers_.find(model);
    if(it == providers_.end())
    {
        WARN("{} NOT REGISTER", model);
        return "";
    }
    if(provider_status_.find(model) == provider_status_.end())
    {
        WARN("{} NOT INIT", model);
        return "";
    }

    return it->second->SendMessage(messages);
}

std::string LLManager::SendMessageStream(const std::string& model , const std::vector<Message> &messages , MessageCallback message_cb)
{
    // 读锁 : 容器只读, 发送消息期间持锁保证 provider 生命周期安全, 多个消息可并发发送
    std::shared_lock<std::shared_mutex> lock(mutex_);

    // 发送消息 , 流式返回
    auto it = providers_.find(model);
    if(it == providers_.end())
    {
        WARN("{} NOT REGISTER", model);
        return "";
    }
    if(provider_status_.find(model) == provider_status_.end())
    {
        WARN("{} NOT INIT", model);
        return "";
    }

    return it->second->SendMessageStream(messages , message_cb);
}

std::vector<ModelInfo> LLManager::GetAllModel()
{
    // 读锁 : 容器只读
    std::shared_lock<std::shared_mutex> lock(mutex_);

    size_t sz = model_info_.size();
    std::vector<ModelInfo> models(sz);
    int i = 0;
    for(auto& [model , model_info] : model_info_)
    {
        models[i++] = model_info;
    }
    return models;
}

}   // end aichat_sdk
