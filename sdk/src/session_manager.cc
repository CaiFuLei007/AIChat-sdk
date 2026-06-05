
#include "session_manager.h"
#include <iostream>
#include <string>
#include <random>

namespace ai_sdk
{


bool SessionManager::HasUserName(const std::string& email)
{
    auto it = user_table_.find(email);
    if(it != user_table_.end())
    {
        return true;
    }
    // 在数据库中进行查找
    auto info = data_manager_->GetUser(email);
    if(info.email.empty())
    {
        return false;
    }
    user_table_.emplace(email , std::make_shared<UserInfo>(info));
    return true;
}

bool SessionManager::HasSession(const std::string& ssid)
{
    auto it = session_table_.find(ssid);
    if(it != session_table_.end())
    {
        return true;
    }
    // 在数据库中进行查找
    auto session = data_manager_->GetSession(ssid);
    if(session.session_id.empty())
    {
        return false;
    }
    session_table_.emplace(ssid , std::make_shared<Session>(session));
    return true;
}

SessionManager::SessionManager(const std::string& db_name)
:data_manager_(std::make_unique<DataManager>(db_name)) , 
timer_wheel_(std::make_unique<TimerWheel>()) 
{
    data_manager_->Init();
    timer_wheel_->Ready();
}

std::string SessionManager::CreateUserId()
{
    // 1. 线程安全的随机数生成器
    thread_local std::random_device rd;
    thread_local std::mt19937 gen(rd());
    
    // 只需要生成 0-15 和 8-11 的随机数
    std::uniform_int_distribution<> dis(0, 15);
    std::uniform_int_distribution<> dis2(8, 11);

    // 2. 静态查表，避免任何进制转换的计算开销
    static const char* hex_chars = "0123456789abcdef";

    // 3. 预分配 36 字节长度的字符串，并默认填充为 '-'
    // 这样就彻底省去了拼接 '-' 的操作和动态扩容的开销
    std::string uuid(36, '-');

    // 4. 直接通过索引填入随机的十六进制字符 (8-4-4-4-12)
    for (int i = 0; i < 8; ++i)  uuid[i] = hex_chars[dis(gen)];
    for (int i = 9; i < 13; ++i) uuid[i] = hex_chars[dis(gen)];
    
    // 第 14 位固定为版本号 4
    uuid[14] = '4';
    for (int i = 15; i < 18; ++i) uuid[i] = hex_chars[dis(gen)];
    
    // 第 19 位为变体号，只能是 8, 9, a, b
    uuid[19] = hex_chars[dis2(gen)];
    for (int i = 20; i < 23; ++i) uuid[i] = hex_chars[dis(gen)];
    for (int i = 24; i < 36; ++i) uuid[i] = hex_chars[dis(gen)];

    return uuid;
}

std::string SessionManager::InsertNewUser(const std::string &email , const std::string& password)
{
    std::unique_lock<std::mutex> lock(mutex_);

    // 向数据库中插入新用户
    if(HasUserName(email))
    {
        return "";
    }
    UserInfo info = {
        .uid = CreateUserId() , 
        .email = email , 
        .password = password , 
        .create_time = time(nullptr) 
    };
    if(!data_manager_->InsertUser(info))
    {
        return "";
    }

    user_table_.emplace(email  , std::make_shared<UserInfo>(info));
    timer_wheel_->SetTask(info.uid ,10 , [this,email](){         // 用户10分钟没有登录将其信息仅在磁盘上进行存储
        std::unique_lock<std::mutex> lock(mutex_);
        
        auto it = user_table_.find(email);
        if(it == user_table_.end())
        {
            return ;
        }
        it->second = nullptr;
    });

    return info.uid;
}

std::shared_ptr<UserInfo> SessionManager::GetUserInfo(const std::string& email)
{
    std::unique_lock<std::mutex> lock(mutex_);

    if(!HasUserName(email))
    {
        return nullptr;
    }

    // 1. 在内存中进行查找
    // 2. 在磁盘中进行查找
    auto it = user_table_.find(email);
    if(it != user_table_.end() && it->second)
    {
        // 更新时间
        timer_wheel_->UpdateTask(it->second->uid);
        return it->second;
    }
    else
    {
        auto info = data_manager_->GetUser(email);
        auto info_ptr = std::make_shared<UserInfo>(info);
        it->second = info_ptr;

        timer_wheel_->SetTask(info.uid ,10 , [this,email](){         // 用户10分钟没有登录将其信息仅在磁盘上进行存储
            std::unique_lock<std::mutex> lock(mutex_);

            auto it = user_table_.find(email);
            if(it == user_table_.end())
            {
                return ;
            }
            it->second = nullptr;
        });
        return info_ptr;
    }
}

std::string SessionManager::Now()
{
    auto now = std::chrono::system_clock::now();
    auto microsec = std::chrono::time_point_cast<std::chrono::microseconds>(now);
    auto valuemicroS = microsec.time_since_epoch().count();
    return std::to_string(valuemicroS);
}

std::string SessionManager::GetssionId(const std::string& uid)
{
    // ssid 格式 : uid_时间戳
    return uid + "_" + Now();
}

std::string SessionManager::Getmid(const std::string& ssid)
{
    // 格式 : ssid_时间戳
    return ssid + "_" + Now();
}

std::string SessionManager::CreateSession(const std::string &uid , const std::string& model_name)
{
    std::unique_lock<std::mutex> lock(mutex_);

    Session session = {
        .uid =  uid, 
        .session_id = GetssionId(uid) , 
        .model_name = model_name , 
        .messages = {} , 
        .create_time = time(nullptr) , 
        .update_time = time(nullptr)
    };
    if(!data_manager_->InsertSession(session))
    {
        return "";
    }
    session_table_.emplace(session.session_id , std::make_shared<Session>(session));

    std::string ssid = session.session_id;
    timer_wheel_->SetTask(session.session_id ,10 , [this,ssid](){         // 用户10分钟没有登录将其信息仅在磁盘上进行存储
        std::unique_lock<std::mutex> lock(mutex_);

        auto it = session_table_.find(ssid);
        if(it == session_table_.end())
        {
            return ;
        }
        it->second = nullptr;
    });

    return session.session_id;
}

bool SessionManager::RemoveSession(const std::string& ssid)
{
    std::unique_lock<std::mutex> lock(mutex_);

    if(!HasSession(ssid))
    {
        return false;
    }
    // 1. 移除数据库中的
    // 2. 移除内存中的
    if(!data_manager_->RemoveSession(ssid))
    {
        return false;
    }
    session_table_.erase(ssid);

    return true;
}

std::shared_ptr<Session> SessionManager::GetSessionUnLock(const std::string& ssid)
{
    if(!HasSession(ssid))
    {
        return nullptr;
    }

    // 1. 在内存中进行查找
    // 2. 在磁盘中进行查找
    auto it = session_table_.find(ssid);
    if(it != session_table_.end() && it->second)
    {
        if(it->second->messages.empty())
        {
            it->second->messages = GetSessionAllMessageUnLock(ssid);
        }
        timer_wheel_->UpdateTask(it->second->session_id);
        return it->second;
    }
    else
    {
        auto session = data_manager_->GetSession(ssid);
        auto session_ptr = std::make_shared<Session>(session);
        session_ptr->messages = GetSessionAllMessageUnLock(ssid);

        it->second = session_ptr;
        timer_wheel_->SetTask(session.session_id ,10 , [this , ssid](){         // 用户10分钟没有登录将其信息仅在磁盘上进行存储
            std::unique_lock<std::mutex> lock(mutex_);

            auto it = session_table_.find(ssid);
            if(it == session_table_.end())
            {
                return ;
            }
            it->second = nullptr;
        });
        return session_ptr;
    }
}

std::shared_ptr<Session> SessionManager::GetSession(const std::string& ssid)
{
    std::unique_lock<std::mutex> lock(mutex_);
    return GetSessionUnLock(ssid);
}
std::vector<Session> SessionManager::GetUserAllSession(const std::string &uid)
{
    std::unique_lock<std::mutex> lock(mutex_);

    // 从数据库中拿
    std::vector<std::string> session_id = data_manager_->GetUserAllSessions(uid);
    std::vector<Session> sessions(session_id.size());
    int i = 0;
    for(auto& ssid : session_id)
    {
        sessions[i++] = *(GetSessionUnLock(ssid));
    }
    return sessions;
} 

bool SessionManager::CreateNewMessage(const std::string &ssid , const std::string& role , const std::string& content)
{
    std::unique_lock<std::mutex> lock(mutex_);

    if(!HasSession(ssid))
    {
        return false;
    }

    Message message = {
        .ssid = ssid ,
        .mid = Getmid(ssid) ,
        .role = role ,
        .content = content ,
        .create_time = time(nullptr)
    };
    if(!data_manager_->InsertMessage(message))
    {
        return false;
    }
    auto sessoin_ptr = GetSessionUnLock(ssid);
    sessoin_ptr->messages.push_back(message);
    return true;
}

std::vector<Message> SessionManager::GetSessionAllMessageUnLock(const std::string& ssid)
{
    if(!HasSession(ssid))
    {
        return {};
    }

    // 1. 在内存中进行查找
    // 2. 在磁盘中进行查找
    auto it = session_table_.find(ssid);
    if(it->second)
    {
        if(!it->second->messages.empty())
        {
            return it->second->messages;
        }
        else
        {
            auto messages = data_manager_->GetMessages(ssid);
            it->second->messages = messages;
            return messages;
        }
    }
    else
    {
        it->second = GetSessionUnLock(ssid);
        auto messages = data_manager_->GetMessages(ssid);
        it->second->messages = messages;
        return messages;
    }
}

std::vector<Message> SessionManager::GetSessionAllMessage(const std::string& ssid)
{
    std::unique_lock<std::mutex> lock(mutex_);
    return GetSessionAllMessageUnLock(ssid);
}

void SessionManager::AddTimerTask(const std::string& id, int timeout , Task task)
{   
    timer_wheel_->SetTask(id , timeout , task);
}
void SessionManager::RemoveTask(const std::string &id)
{
    timer_wheel_->CancelTask(id);
}

}; // end ai_sdk