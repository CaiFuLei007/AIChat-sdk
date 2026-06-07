
#pragma once

/*
    - 搭建一个 服务器来提供 AI对话服务
    - 成员 ：
            1) http::server 服务端
            2) ai_sdk 
            3) 两个hash 表存储用户验证码

*/

#include "aichat_sdk.h"
#include "curl_util.hpp"

#include <unordered_map>
#include <iostream>
#include <string>
#include <httplib.h>
#include <memory>
#include <json/json.h>

class AIChatServer
{
private:
    std::mutex mutex_;

    httplib::Server server_;
    ai_sdk::AIChatSdk ai_sdk_;
    std::unordered_map<std::string , std::string> verification_code_;

    std::unique_ptr<util::Curl> curl_;
    std::string web_dir_;
    std::string mount_point_;

private:
    static bool unserialize(const std::string& json ,  ::Json::Value& value);
    static bool serialize(const ::Json::Value& json , std::string& json_str);
    bool SendVerification(const std::string& email);

    void HandleError(const std::string &message ,  httplib::Response& response);

    void HandleGetVerification(const httplib::Request& request, httplib::Response& response);
    void HandleRegister(const httplib::Request& request, httplib::Response& response);
    void HandleLogin(const httplib::Request& request, httplib::Response& response);

private:    
    void HandleGetUserSessions(const httplib::Request& request, httplib::Response& response);
    void HandleGetModels(const httplib::Request& request, httplib::Response& response);
    void HandleCreateSession(const httplib::Request& request, httplib::Response& response);
    void HandleRemoveSession(const httplib::Request& request, httplib::Response& response);
    void HandleGetSessionAllMessage(const httplib::Request& request, httplib::Response& response);
    void HandleSendMessage(const httplib::Request& request, httplib::Response& response);
    void HandleSendMessageStream(const httplib::Request& request, httplib::Response& response);

    void SetRouter();

public:
    AIChatServer(const std::string &db_name , const util::Config_info &config);

    void SetWebRoot(const std::string &mount_point, const std::string &web_dir);
    void RegisterModels(const std::vector<ai_sdk::Config> &configs);
    void Run(const std::string &ip , uint16_t port);

};