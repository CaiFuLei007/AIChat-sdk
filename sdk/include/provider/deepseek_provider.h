
#pragma once

/*
    - 实现 DeepSeek Provider , 继承 Provider 实现内部功能
    - 成员 : 
            1) Config 配置
            2) 是否可用
    - 接口 :    
            1) 初始化
            2) 发送消息  , 全量返回 , 流式返回

*/

#include "provider/provider.h"


namespace ai_sdk
{
class DeepSeekProvider : public Provider
{
private:    
    std::string BuildResponseBody(const std::vector<Message> &messages , bool stream);

public:
    DeepSeekProvider() = default;
    ~DeepSeekProvider() = default;
    
    bool Init(Config config);

    std::string SendMessage(const std::vector<Message> &messages);
    std::string SendMessageStream(const std::vector<Message> &messages , MessageCallback message_cb) ;
};


}; // end ai_sdk