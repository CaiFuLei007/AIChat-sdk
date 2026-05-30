
#pragma once

/*
    - 作为每个模型 Provider 的基类 , 抽象类
    - 成员 : 
            1) Config 相关配置信息
            2) is_avaiable 是否可用

    - 接口 : 
            1) 初始化
            2) 获取模型名称 , 描述
            3) 发送消息 , 全量返回 , 流失返回
*/

#include "base/common.h"
#include <functional>


namespace ai_sdk
{

class Provider
{
    using MessageCallback = std::function<void(const std::string& mes , bool finish)>;
private:
    Config config_;
    bool is_avaiable_;
public:
    virtual bool Init() = 0;
    bool IsAvaiable()
    {
        return is_avaiable_;
    }

    std::string ModelName()
    {
        return config_.model_info.model_name;
    }

    std::string ModelDesc()
    {
        return config_.model_info.model_decs;
    }

    std::string SendMessage(const std::vector<std::string> messages);
    std::string SendMessageStream(const std::vector<std::string> messages , MessageCallback message_cb);
};


} // end ai_sdk