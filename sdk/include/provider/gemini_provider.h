
#pragma once

/*
    - 实现 Gemini Provider , 继承 Provider 实现内部功能
    - 成员 : 
            1) Config 配置
            2) 是否可用
    - 接口 :    
            1) 初始化
            2) 发送消息  , 全量返回 , 流式返回

*/

// 使用相对路径包含, 避免搜索路径顺序问题命中系统 include 根目录下的同名残留头文件
#include "provider.h"


namespace aichat_sdk
{
class GeminiProvider : public Provider
{
private:    
    std::string BuildResponseBody(const std::vector<Message> &messages);

public:
    GeminiProvider() = default;
    ~GeminiProvider() = default;
    
    bool Init(Config config);

    std::string SendMessage(const std::vector<Message> &messages);
    std::string SendMessageStream(const std::vector<Message> &messages , MessageCallback message_cb) ;
};


}; // end aichat_sdk