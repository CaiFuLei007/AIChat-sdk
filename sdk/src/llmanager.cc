
#include "llmmanager.h"
#include "base/util/mylog.h"

namespace ai_sdk
{

bool LLManager::RegisterProvider(const std::string &model , std::unique_ptr<Provider> provider)
{
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
    size_t sz = model_info_.size();
    std::vector<ModelInfo> models(sz);
    int i = 0;
    for(auto& [model , model_info] : model_info_)
    {
        models[i++] = model_info;
    }
    return models;
}

}   // end ai_sdk