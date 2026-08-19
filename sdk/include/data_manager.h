
#pragma once 

/*
    - 对sqlite 数据库进行管理
    - 成员 :        
            1) sqlite3 句柄
            2) 锁
    - 接口 :
            1) 初始化 : 创建三张表 user_info session message
            2) 用户操作 : 插入新用户  , 获取用户信息
            3) session 操作 : 插入新 Session , 更新 Session时间 , 删除 Session  , 获取用户的所有SessionId , 删除用户所有的session
            4) message 操作 : 插入新的 Message , 删除 Message , 获取 sessoin 所有message, 删除 Sessoin 所有 Message

*/

#include "base/common.h"

#include <sqlite3.h>
#include <string>
#include <mutex>

namespace aichat_sdk
{

class DataManager
{
private:
    sqlite3 *sqlite_;
    std::mutex mutex_;
private:
    bool ExecSqlCommand(const std::string &sql);
    void Begin();
    void Commit();
    void Rollback();
    bool RemoveSessionAllMessageUnlock(const std::string &ssid);
    bool RemoveUserAllSessionUnlock(const std::string &uid);

public:
    DataManager(const std::string &db_name);

    ~DataManager()
    {
        sqlite3_close(sqlite_);
    }

    bool Init();

    bool InsertUser(const UserInfo &user_info);
    bool RemoveUser(const std::string &uid);
    UserInfo GetUser(const std::string& email);

    bool InsertSession(const Session& session);
    bool RemoveSession(const std::string &ssid);
    bool RemoveUserAllSession(const std::string &uid);
    bool UpdateSession(const std::string &ssid);
    Session GetSession(const std::string &ssid);
    std::vector<std::string> GetUserAllSessions(const std::string &uid);

    bool InsertMessage(const Message& message);
    bool RemoveMessage(const std::string& mid);
    std::vector<Message> GetMessages(const std::string& ssid);
    bool RemoveSessionAllMessage(const std::string &ssid);

    std::vector<std::string> GetAllSessions();
    std::vector<std::string> GetAllUserName();
};

} // namespace aichat_sdk
