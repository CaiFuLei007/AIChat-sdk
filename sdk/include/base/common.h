
#pragma once

/*
    - 基础数据结构 : UserInfo 用户信息 , Message 存储每一条消息 , Session 存储会话信息 , ModelInfo 模型基础信息 , Config 存储模型配置信息
*/

#include <iostream>
#include <string>
#include <vector>

namespace ai_sdk
{

struct UserInfo
{
    int id;
    std::string uuid;
    std::string name;
    std::string password;
};

struct  Message
{
    std::string role;
    std::string content;
    
    time_t create_time;
};

struct Session
{
    std::string uuid;
    std::string session_id;
    std::string model_name;
    std::vector<Message> messages;

    time_t create_time;
    time_t update_time;
};

struct ModelInfo
{
    std::string model_name;
    std::string model_decs;
};

struct Config
{
    ModelInfo model_info;       // 展示给用户的

    std::string end_point;
    std::string apikey;         // API 
    
    std::string path;           // 模型的path
    std::string streampath;     // stream path
    std::string model;          

    double temperature = 1;            // 范围 : 0~2 
    int max_token = 4096;
    double top_p = 1;
    std::string reasoning_effort = "high";  // deepseek
};

} // namespace ai_sdk