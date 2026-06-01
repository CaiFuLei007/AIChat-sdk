
#include "data_manager.h"
#include "base/util/mylog.h"

namespace ai_sdk
{

DataManager::DataManager(const std::string &db_name)
{
    int ret = sqlite3_open(db_name.c_str(), &sqlite_);
    if(ret != SQLITE_OK )
    {
        ERR("SQLITE3 OPEN FAIL");
    }
}

void DataManager::Begin()
{
    static std::string begin = "BEGIN";

    char* errmsg;
    int rc = sqlite3_exec(sqlite_, begin.c_str(), nullptr, nullptr , &errmsg);
    if( rc != SQLITE_OK ){
        ERR("[{}] EXEC FAIL  : {}" ,begin ,  errmsg);
        sqlite3_free(errmsg);
    }
}

void DataManager::Commit()
{
    static std::string commit = "COMMIT";

    char* errmsg;
    int rc = sqlite3_exec(sqlite_, commit.c_str(), nullptr, nullptr , &errmsg);
    if( rc != SQLITE_OK ){
        ERR("[{}] EXEC FAIL  : {}" ,commit ,  errmsg);
        sqlite3_free(errmsg);
    }
}

void DataManager::Rollback()
{
    static std::string rollback = "ROLLBACK";

    char* errmsg;
    int rc = sqlite3_exec(sqlite_, rollback.c_str(), nullptr, nullptr , &errmsg);
    if( rc != SQLITE_OK ){
        ERR("[{}] EXEC FAIL  : {}" ,rollback ,  errmsg);
        sqlite3_free(errmsg);
    }
}

bool DataManager::ExecSqlCommand(const std::string &sql)
{
    char* errmsg;
    int rc = sqlite3_exec(sqlite_, sql.c_str(), nullptr, nullptr , &errmsg);
    if( rc != SQLITE_OK ){
        ERR("[{}] EXEC FAIL  : {}" ,sql ,  errmsg);
        sqlite3_free(errmsg);
        return false;
    }
    return true;
}


bool DataManager::Init()
{

    // 初始化创建三张表
    static std::string create_user_table = "CREATE TABLE IF NOT EXISTS USER ("\
    "ID             INTEGER PRIMARY KEY    AUTOINCREMENT,"\
    "UID            CHAR(36)            NOT NULL UNIQUE,"\
    "USER_NAME      VARCHAR(64)                 NOT NULL,"\
    "PASSWORD       CHAR(64)            NOT NULL,"\
    "CREATE_TIME    INT            NOT NULL);";

    static std::string create_session_table = "CREATE TABLE IF NOT EXISTS SESSION ("\
    "ID                 INTEGER PRIMARY KEY  AUTOINCREMENT,"\
    "UID                CHAR(36)            NOT NULL,"\
    "SSID               CHAR(36)            NOT NULL UNIQUE,"\
    "MODEL_NAME         VARCHAR(30)           NOT NULL,"\
    "CREATE_TIME        INT                 NOT NULL ,"\
    "UPDATE_TIME        INT                 NOT NULL);";

    static std::string create_message_table = "CREATE TABLE IF NOT EXISTS MESSAGE ("\
    "ID            INTEGER PRIMARY KEY   AUTOINCREMENT,"\
    "MID           CHAR(36)            NOT NULL UNIQUE,"\
    "SSID          CHAR(36)            NOT NULL,"\
    "ROLE          VARCHAR(30)           NOT NULL,"\
    "CONTENT       TEXT                NOT NULL,"
    "CREATE_TIME    INT                 NOT NULL);";

    std::unique_lock<std::mutex> lock(mutex_);
    if(!ExecSqlCommand(create_user_table))
    {
        return false;
    }
    if(!ExecSqlCommand(create_session_table))
    {
        return false;
    }
    if(!ExecSqlCommand(create_message_table))
    {
        return false;
    }
    return true;
}

bool DataManager::InsertUser(const UserInfo &user_info)
{
    std::unique_lock<std::mutex> lock(mutex_);

    static std::string insert_into_user = "INSERT INTO USER (UID , USER_NAME , PASSWORD , CREATE_TIME) "\
    "VALUES(? , ? , ? , ?)";
    
    sqlite3_stmt *stmt;
    int ret = sqlite3_prepare_v2(sqlite_, insert_into_user.c_str(), -1, &stmt, NULL);
    if (ret != SQLITE_OK) {
        WARN("SQLITE3 PREPARE FAIL");
        return false;
    }

    sqlite3_bind_text(stmt, 1, user_info.uid.c_str(), -1, NULL);
    sqlite3_bind_text(stmt, 2, user_info.name.c_str(), -1, NULL);
    sqlite3_bind_text(stmt, 3, user_info.password.c_str(), -1, NULL);
    sqlite3_bind_int64(stmt, 4, user_info.create_time);

    if(sqlite3_step(stmt) != SQLITE_DONE)
    {
        sqlite3_finalize(stmt);
        WARN("SQLITE3 STEP FAIL");
        return false;
    }
    sqlite3_finalize(stmt);
    return true;
}

bool DataManager::RemoveUser(const std::string &uid)
{
    // 1. 从Sessoin 表中移除 用户所有的 Session
    // 2. 从User 表中移除 用户
    std::unique_lock<std::mutex> lock(mutex_);
    Begin();

    if(!RemoveUserAllSessionUnlock(uid))
    {
        Rollback();
        ERR("REMOVE USER ALL SESSION FAIL , UID : {}" , uid);
        return false;
    }
    static std::string remove_from_user = "DELETE FROM USER WHERE UID=?";

    sqlite3_stmt *stmt;
    int ret = sqlite3_prepare_v2(sqlite_, remove_from_user.c_str(), -1, &stmt, NULL);
    if (ret != SQLITE_OK) {
        Rollback();
        WARN("SQLITE3 PREPARE FAIL");
        return false;
    }

    sqlite3_bind_text(stmt, 1, uid.c_str(), -1, NULL);

    if(sqlite3_step(stmt) != SQLITE_DONE)
    {
        Rollback();
        sqlite3_finalize(stmt);
        WARN("SQLITE3 STEP FAIL");
        return false;
    }
    Commit();
    sqlite3_finalize(stmt);
    return true;
}

UserInfo DataManager::GetUser(const std::string& name)
{
    std::unique_lock<std::mutex> lock(mutex_);

    static std::string get_user = "SELECT UID , USER_NAME , PASSWORD , CREATE_TIME FROM USER WHERE USER_NAME=?";
    
    UserInfo user;
    sqlite3_stmt *stmt;
    int ret = sqlite3_prepare_v2(sqlite_, get_user.c_str(), -1, &stmt, NULL);
    if (ret != SQLITE_OK) {
        WARN("SQLITE3 PREPARE FAIL");
        return user;
    }

    sqlite3_bind_text(stmt, 1, name.c_str(), -1, NULL);
    int rc = 0;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        user.uid = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        user.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        user.password = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        user.create_time = sqlite3_column_int64(stmt, 3);
    }

    if(rc != SQLITE_DONE)
    {
        sqlite3_finalize(stmt);
        WARN("SQLITE3 STEP FAIL");
        return user;
    }
    sqlite3_finalize(stmt);
    return user;
}

bool DataManager::InsertSession(const Session& session)
{
    std::unique_lock<std::mutex> lock(mutex_);

    static std::string insert_into_session = "INSERT INTO SESSION (UID , SSID , MODEL_NAME , CREATE_TIME , UPDATE_TIME) "\
    "VALUES(? , ? , ? , ? , ?)";

    sqlite3_stmt *stmt;
    int ret = sqlite3_prepare_v2(sqlite_, insert_into_session.c_str(), -1, &stmt, NULL);
    if (ret != SQLITE_OK) {
        WARN("SQLITE3 PREPARE FAIL");
        return false;
    }

    sqlite3_bind_text(stmt, 1, session.uid.c_str(), -1, NULL);
    sqlite3_bind_text(stmt, 2, session.session_id.c_str(), -1, NULL);
    sqlite3_bind_text(stmt, 3, session.model_name.c_str(), -1, NULL);
    sqlite3_bind_int64(stmt , 4 , session.create_time );
    sqlite3_bind_int64(stmt , 5 , session.update_time );

    if(sqlite3_step(stmt) != SQLITE_DONE)
    {
        sqlite3_finalize(stmt);
        WARN("SQLITE3 STEP FAIL");
        return false;
    }
    sqlite3_finalize(stmt);
    return true;
}
bool DataManager::RemoveUserAllSessionUnlock(const std::string &uid)
{
    static std::string remove_user_allsession = "DELETE FROM SESSION WHERE UID=?";

    sqlite3_stmt *stmt;
    int ret = sqlite3_prepare_v2(sqlite_, remove_user_allsession.c_str(), -1, &stmt, NULL);
    if (ret != SQLITE_OK) {
        WARN("SQLITE3 PREPARE FAIL");
        return false;
    }

    sqlite3_bind_text(stmt, 1, uid.c_str(), -1, NULL);

    if(sqlite3_step(stmt) != SQLITE_DONE)
    {
        sqlite3_finalize(stmt);
        WARN("SQLITE3 STEP FAIL");
        return false;
    }
    sqlite3_finalize(stmt);
    return true;
}

bool DataManager::RemoveUserAllSession(const std::string &uid)
{
    std::unique_lock<std::mutex> lock(mutex_);

    return RemoveUserAllSessionUnlock(uid);
}

bool DataManager::RemoveSession(const std::string &ssid)
{
    std::unique_lock<std::mutex> lock(mutex_);

    Begin();
    // 1. 从 Mesage 表中移除 ssid中的所有消息
    // 2. 从 Session 表中 移除会话
    if(!RemoveSessionAllMessageUnlock(ssid))
    {
        Rollback();
        ERR("REMOVE SESSION ALL MESSAGE FAIL , SSID : {}" , ssid);
        return false;
    }
    static std::string remove_from_session = "DELETE FROM SESSION WHERE SSID=?";
    
    sqlite3_stmt *stmt;
    int ret = sqlite3_prepare_v2(sqlite_, remove_from_session.c_str(), -1, &stmt, NULL);
    if (ret != SQLITE_OK) {
        Rollback();
        WARN("SQLITE3 PREPARE FAIL");
        return false;
    }

    sqlite3_bind_text(stmt, 1, ssid.c_str(), -1, NULL);

    if(sqlite3_step(stmt) != SQLITE_DONE)
    {
        Rollback();
        sqlite3_finalize(stmt);
        WARN("SQLITE3 STEP FAIL");
        return false;
    }
    sqlite3_finalize(stmt);
    Commit();
    return true;
}

Session DataManager::GetSession(const std::string &ssid)
{
    std::unique_lock<std::mutex> lock(mutex_);
    static std::string get_session = "SELECT UID , SSID , MODEL_NAME , CREATE_TIME , UPDATE_TIME FROM SESSION WHERE SSID=?";

    sqlite3_stmt *stmt;
    int ret = sqlite3_prepare_v2(sqlite_, get_session.c_str(), -1, &stmt, NULL);
    if (ret != SQLITE_OK) {
        WARN("SQLITE3 PREPARE FAIL");
        return {};
    }

    sqlite3_bind_text(stmt, 1, ssid.c_str(), -1, NULL);
    Session session;
    int rc = 0;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        session.uid= reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        session.session_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        session.model_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        session.create_time = sqlite3_column_int64(stmt, 3);
        session.update_time = sqlite3_column_int64(stmt, 4);
    }
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
         WARN("SQLITE3 STEP FAIL");
        return {};
    }
    return session;
}

bool DataManager::UpdateSession(const std::string &ssid)
{
    std::unique_lock<std::mutex> lock(mutex_);

    static std::string update_session = "UPDATE SESSION SET UPDATE_TIME=? WHERE SSID=?";

    sqlite3_stmt *stmt;
    int ret = sqlite3_prepare_v2(sqlite_, update_session.c_str(), -1, &stmt, NULL);
    if (ret != SQLITE_OK) {
        WARN("SQLITE3 PREPARE FAIL");
        return false;
    }

    sqlite3_bind_int64(stmt, 1, time(nullptr));
    sqlite3_bind_text(stmt, 2, ssid.c_str(), -1, NULL);
    if(sqlite3_step(stmt) != SQLITE_DONE)
    {
        sqlite3_finalize(stmt);
        WARN("SQLITE3 STEP FAIL");
        return false;
    }
    sqlite3_finalize(stmt);
    return true;
}

std::vector<std::string> DataManager::GetUserAllSessions(const std::string &uid)
{
    std::unique_lock<std::mutex> lock(mutex_);

    static std::string get_allsessions = "SELECT SSID FROM SESSION WHERE UID=?";

    sqlite3_stmt *stmt;
    int ret = sqlite3_prepare_v2(sqlite_, get_allsessions.c_str(), -1, &stmt, NULL);
    if (ret != SQLITE_OK) {
        WARN("SQLITE3 PREPARE FAIL");
        return {};
    }

    sqlite3_bind_text(stmt, 1, uid.c_str(), -1, NULL);
    std::vector<std::string > sessions;
    int rc = 0;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        const unsigned char* ssid_ptr = sqlite3_column_text(stmt, 0);
        sessions.emplace_back(reinterpret_cast<const char*>(ssid_ptr));
    }
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
         WARN("SQLITE3 STEP FAIL");
        return {};
    }
    return sessions;
}

bool DataManager::InsertMessage(const Message& message)
{
    std::unique_lock<std::mutex> lock(mutex_);

    static std::string insert_into_message = "INSERT INTO MESSAGE (SSID , MID , ROLE , CONTENT , CREATE_TIME) "\
    "VALUES(? , ? , ? ,? , ?)";

    sqlite3_stmt *stmt;
    int ret = sqlite3_prepare_v2(sqlite_, insert_into_message.c_str(), -1, &stmt, NULL);
    if (ret != SQLITE_OK) {
        WARN("SQLITE3 PREPARE FAIL");
        return false;
    }

    sqlite3_bind_text(stmt, 1, message.ssid.c_str(), -1, NULL);
    sqlite3_bind_text(stmt, 2, message.mid.c_str(), -1, NULL);
    sqlite3_bind_text(stmt, 3, message.role.c_str(), -1, NULL);
    sqlite3_bind_text(stmt, 4, message.content.c_str(), -1, NULL);
    sqlite3_bind_int64(stmt , 5 , message.create_time);
    
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        WARN("SQLITE3 STEP FAIL");
        return false;
    }
    sqlite3_finalize(stmt);
    return true;
}
bool DataManager::RemoveMessage(const std::string& mid)
{
    std::unique_lock<std::mutex> lock(mutex_);

    static std::string remove_from_message = "DELETE FROM MESSAGE WHERE MID=?";

    sqlite3_stmt *stmt;
    int ret = sqlite3_prepare_v2(sqlite_, remove_from_message.c_str(), -1, &stmt, NULL);
    if (ret != SQLITE_OK) {
        WARN("SQLITE3 PREPARE FAIL");
        return false;
    }

    sqlite3_bind_text(stmt, 1, mid.c_str(), -1, NULL);
    
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        WARN("SQLITE3 STEP FAIL");
        return false;
    }
    sqlite3_finalize(stmt);
    return true;
}
std::vector<Message> DataManager::GetMessages(const std::string& ssid)
{
    std::unique_lock<std::mutex> lock(mutex_);

    static std::string get_messages = "SELECT MID , ROLE , CONTENT , CREATE_TIME FROM MESSAGE WHERE SSID=?";
    
    sqlite3_stmt *stmt;
    int ret = sqlite3_prepare_v2(sqlite_, get_messages.c_str(), -1, &stmt, NULL);
    if (ret != SQLITE_OK) {
        WARN("SQLITE3 PREPARE FAIL");
        return {};
    }

    sqlite3_bind_text(stmt, 1, ssid.c_str(), -1, NULL);
    std::vector<Message > messages;
    int rc = 0;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        Message tmp;
        tmp.mid = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        tmp.role = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        tmp.content = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        tmp.create_time = sqlite3_column_int64(stmt, 3);
        tmp.ssid = ssid;
        
        messages.emplace_back(std::move(tmp));
    }
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
         WARN("SQLITE3 STEP FAIL");
        return {};
    }
    return messages;
}

bool DataManager::RemoveSessionAllMessageUnlock(const std::string &ssid)
{
    static std::string remove_session_allmessage = "DELETE FROM MESSAGE WHERE SSID=?";
    sqlite3_stmt *stmt;
    int ret = sqlite3_prepare_v2(sqlite_, remove_session_allmessage.c_str(), -1, &stmt, NULL);
    if (ret != SQLITE_OK) {
        WARN("SQLITE3 PREPARE FAIL");
        return false;
    }

    sqlite3_bind_text(stmt, 1, ssid.c_str(), -1, NULL);
    
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        WARN("SQLITE3 STEP FAIL");
        return false;
    }
    sqlite3_finalize(stmt);
    return true;
}


bool DataManager::RemoveSessionAllMessage(const std::string &ssid)
{
    std::unique_lock<std::mutex> lock(mutex_);

    return RemoveSessionAllMessageUnlock(ssid);
}

}// end ai_sdk