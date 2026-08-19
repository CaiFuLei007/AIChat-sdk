
#pragma once

/*
    - 对所有 Provider 进行管理
    - 成员
            1) 哈希表 : 模型名称 与之对应的 Provider
            2) 哈希表 : 模型名称 与之对应的 ModelInfo
            3) 哈希表 : 模型名称 , 模型是否可用
    - 接口 : 
            1) 注册 Provider
            2) 初始化 Provider
            3) 获取所有可用的 Provider 的 ModelInfo
            4) 发送消息 : 全量返回 , 流式返回

*/

#include "provider/provider.h"
#include <unordered_map>
#include <memory>

namespace aichat_sdk
{

class LLManager
{
    using MessageCallback = std::function<void(const std::string& data , bool finish)>;
private:
    std::unordered_map<std::string , std::unique_ptr<Provider> > providers_;
    std::unordered_map<std::string , bool> provider_status_;
    std::unordered_map<std::string , ModelInfo > model_info_;
public:
    LLManager() = default;

    bool RegisterProvider(const std::string &model , std::unique_ptr<Provider> provider);
    bool InitProvider(const std::string& model , const Config &config);

    std::string SendMessage(const std::string& model , const std::vector<Message> &messages);
    std::string SendMessageStream(const std::string& model , const std::vector<Message> &messages , MessageCallback message_cb);

    std::vector<ModelInfo> GetAllModel();

};


} // end aichat_sdk