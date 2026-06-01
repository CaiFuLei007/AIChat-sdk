
#pragma once

/*
    - 对 Session 和 用户 进行管理
    - 成员 :    
            1) hash表 : 所有的用户ID, 以及用户对应的信息
            2) hash表 : 所有的 ssid ,已经 session 对应的信息
            3) DataManager 对数据库进行管理
            4) 定时器

    - 接口 : 
            1) 用户相关操作 
                    1) 创建新用户 (email , password)
                    2) 获取用户的信息 (email) 
                    3) 删除用户 (uid)
            2) 会话相关操作
                    1) 创建新会话 (uid , model_name)
                    2) 删除会话 (ssid)
                    3) 获取一个会话中的所有相关信息 (ssid)
                    4) 获取一个用户的所有会话 (uid)
            3) 消息相关操作
                    1) 插入新消息 (ssid) 
                    2) 获取一个会话中的所有消息信息 (ssid)

*/

#include "data_manager.h"
#include "base/common.h"
#include "base/timerwheel.h"
#include <unordered_map>
#include <memory>
#include <mutex>

namespace ai_sdk
{

class SessionManager
{
private:
        std::unordered_map<std::string , std::shared_ptr<UserInfo> > user_table_;
        std::unordered_map<std::string , std::shared_ptr<Session> > session_table_;
        std::unique_ptr<DataManager> data_manager_;
        std::unique_ptr<TimerWheel> timer_wheel_;

        std::mutex mutex_;
private:
        std::string Now();
        std::string GetssionId(const std::string& uid);
        std::string Getmid(const std::string& ssid);
        std::string CreateUserId();

        std::shared_ptr<Session> GetSessionUnLock(const std::string& ssid);
public:
        SessionManager(const std::string& db_name);

        std::string InsertNewUser(const std::string &email , const std::string& password);
        std::shared_ptr<UserInfo> GetUserInfo(const std::string& email);

        std::string CreateSession(const std::string &uid , const std::string& model_name);
        bool RemoveSession(const std::string& ssid);
        std::shared_ptr<Session> GetSession(const std::string& ssid);
        std::vector<Session> GetUserAllSession(const std::string &uid);
        
        bool CreateNewMessage(const std::string &ssid , const std::string& role , const std::string& content);
        std::vector<Message> GetSessionAllMessage(const std::string& ssid);

        bool HasUserName(const std::string& email);
        bool HasSession(const std::string& ssid);
};

} // end ai_sdk