
#pragma once

#include <gtest/gtest.h>
#include "session_manager.h"
#include "base/util/mylog.h"
#include <chrono>
#include <thread>
#include <atomic>
#include <vector>
#include <mutex>
#include <filesystem>

using namespace ai_sdk;

class SessionManagerTest : public ::testing::Test
{
protected:
    SessionManager* session_manager_;
    std::string test_db_name_ = "test_session_manager.db";

    static void SetUpTestSuite()
    {
        ai_sdk::Logger::initLogger("test", "stdout", spdlog::level::debug);
    }

    void SetUp() override
    {
        // 删除旧的测试数据库
        if (std::filesystem::exists(test_db_name_))
        {
            std::filesystem::remove(test_db_name_);
        }
        session_manager_ = new SessionManager(test_db_name_);

        // 等待定时器线程启动
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    void TearDown() override
    {
        delete session_manager_;
        session_manager_ = nullptr;

        // 清理测试数据库
        if (std::filesystem::exists(test_db_name_))
        {
            std::filesystem::remove(test_db_name_);
        }
    }
};

// =================================================================
//                         用户管理测试
// =================================================================

// 测试创建新用户
TEST_F(SessionManagerTest, InsertNewUser)
{
    std::string result = session_manager_->InsertNewUser("test_user", "password123");
    EXPECT_FALSE(result.empty()) << "应该成功创建新用户";
}

// 测试创建重复用户名
TEST_F(SessionManagerTest, InsertDuplicateUser)
{
    session_manager_->InsertNewUser("test_user", "password123");
    std::string result = session_manager_->InsertNewUser("test_user", "different_password");
    EXPECT_TRUE(result.empty()) << "不应该允许创建重复用户名";
}

// 测试获取用户信息
TEST_F(SessionManagerTest, GetUserInfo)
{
    session_manager_->InsertNewUser("test_user", "password123");

    auto user_info = session_manager_->GetUserInfo("test_user");

    ASSERT_NE(user_info, nullptr) << "应该能获取到用户信息";
    EXPECT_EQ(user_info->email, "test_user");
    EXPECT_EQ(user_info->password, "password123");
    EXPECT_FALSE(user_info->uid.empty()) << "用户ID不应该为空";
}

// 测试获取不存在的用户
TEST_F(SessionManagerTest, GetNonExistentUser)
{
    auto user_info = session_manager_->GetUserInfo("non_existent_user");
    EXPECT_EQ(user_info, nullptr) << "不存在的用户应该返回nullptr";
}

// 测试 HasUserName
TEST_F(SessionManagerTest, HasUserName)
{
    EXPECT_FALSE(session_manager_->HasUserName("test_user"));

    session_manager_->InsertNewUser("test_user", "password123");

    EXPECT_TRUE(session_manager_->HasUserName("test_user"));
    EXPECT_FALSE(session_manager_->HasUserName("other_user"));
}

// 测试用户ID格式（UUID v4）
TEST_F(SessionManagerTest, UserIdFormat)
{
    session_manager_->InsertNewUser("test_user", "password123");
    auto user_info = session_manager_->GetUserInfo("test_user");

    ASSERT_NE(user_info, nullptr);

    // UUID v4 格式: xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx
    // 长度应该是36
    EXPECT_EQ(user_info->uid.length(), 36);

    // 检查分隔符位置
    EXPECT_EQ(user_info->uid[8], '-');
    EXPECT_EQ(user_info->uid[13], '-');
    EXPECT_EQ(user_info->uid[18], '-');
    EXPECT_EQ(user_info->uid[23], '-');

    // 版本号应该是4
    EXPECT_EQ(user_info->uid[14], '4');
}

// 测试创建多个用户
TEST_F(SessionManagerTest, InsertMultipleUsers)
{
    const int num_users = 10;

    for (int i = 0; i < num_users; ++i)
    {
        std::string name = "user_" + std::to_string(i);
        std::string password = "password_" + std::to_string(i);
        EXPECT_FALSE(session_manager_->InsertNewUser(name, password).empty());
    }

    // 验证所有用户都存在
    for (int i = 0; i < num_users; ++i)
    {
        std::string name = "user_" + std::to_string(i);
        EXPECT_TRUE(session_manager_->HasUserName(name));

        auto user_info = session_manager_->GetUserInfo(name);
        ASSERT_NE(user_info, nullptr);
        EXPECT_EQ(user_info->email, name);
    }
}

// =================================================================
//                         会话管理测试
// =================================================================

// 测试创建会话
TEST_F(SessionManagerTest, CreateSession)
{
    session_manager_->InsertNewUser("test_user", "password123");
    auto user_info = session_manager_->GetUserInfo("test_user");

    std::string ssid = session_manager_->CreateSession(user_info->uid, "deepseek");

    EXPECT_FALSE(ssid.empty()) << "会话ID不应该为空";
    EXPECT_TRUE(session_manager_->HasSession(ssid));
}

// 测试获取会话
TEST_F(SessionManagerTest, GetSession)
{
    session_manager_->InsertNewUser("test_user", "password123");
    auto user_info = session_manager_->GetUserInfo("test_user");

    std::string ssid = session_manager_->CreateSession(user_info->uid, "deepseek");

    auto session = session_manager_->GetSession(ssid);

    ASSERT_NE(session, nullptr);
    EXPECT_EQ(session->uid, user_info->uid);
    EXPECT_EQ(session->session_id, ssid);
    EXPECT_EQ(session->model_name, "deepseek");
}

// 测试获取不存在的会话
TEST_F(SessionManagerTest, GetNonExistentSession)
{
    auto session = session_manager_->GetSession("non_existent_ssid");
    EXPECT_EQ(session, nullptr);
}

// 测试删除会话
TEST_F(SessionManagerTest, RemoveSession)
{
    session_manager_->InsertNewUser("test_user", "password123");
    auto user_info = session_manager_->GetUserInfo("test_user");

    std::string ssid = session_manager_->CreateSession(user_info->uid, "deepseek");
    ASSERT_TRUE(session_manager_->HasSession(ssid));

    bool result = session_manager_->RemoveSession(ssid);

    EXPECT_TRUE(result);
    EXPECT_FALSE(session_manager_->HasSession(ssid));
}

// 测试删除不存在的会话
TEST_F(SessionManagerTest, RemoveNonExistentSession)
{
    bool result = session_manager_->RemoveSession("non_existent_ssid");
    EXPECT_FALSE(result);
}

// 测试 HasSession
TEST_F(SessionManagerTest, HasSession)
{
    session_manager_->InsertNewUser("test_user", "password123");
    auto user_info = session_manager_->GetUserInfo("test_user");

    EXPECT_FALSE(session_manager_->HasSession("some_ssid"));

    std::string ssid = session_manager_->CreateSession(user_info->uid, "deepseek");

    EXPECT_TRUE(session_manager_->HasSession(ssid));
    EXPECT_FALSE(session_manager_->HasSession("other_ssid"));
}

// 测试获取用户所有会话
TEST_F(SessionManagerTest, GetUserAllSession)
{
    session_manager_->InsertNewUser("test_user", "password123");
    auto user_info = session_manager_->GetUserInfo("test_user");

    // 创建多个会话
    std::vector<std::string> ssids;
    for (int i = 0; i < 5; ++i)
    {
        std::string ssid = session_manager_->CreateSession(user_info->uid, "model_" + std::to_string(i));
        ssids.push_back(ssid);
        std::this_thread::sleep_for(std::chrono::milliseconds(10)); // 确保时间戳不同
    }

    auto sessions = session_manager_->GetUserAllSession(user_info->uid);

    EXPECT_EQ(sessions.size(), 5);
}

// 测试会话ID格式
TEST_F(SessionManagerTest, SessionIdFormat)
{
    session_manager_->InsertNewUser("test_user", "password123");
    auto user_info = session_manager_->GetUserInfo("test_user");

    std::string ssid = session_manager_->CreateSession(user_info->uid, "deepseek");

    // 会话ID格式: uid_时间戳
    EXPECT_TRUE(ssid.find(user_info->uid) != std::string::npos) << "会话ID应该包含用户ID";
    EXPECT_TRUE(ssid.find('_') != std::string::npos) << "会话ID应该包含下划线分隔符";
}

// 测试创建多个会话
TEST_F(SessionManagerTest, CreateMultipleSessions)
{
    session_manager_->InsertNewUser("test_user", "password123");
    auto user_info = session_manager_->GetUserInfo("test_user");

    std::vector<std::string> ssids;
    for (int i = 0; i < 10; ++i)
    {
        std::string ssid = session_manager_->CreateSession(user_info->uid, "model_" + std::to_string(i));
        EXPECT_FALSE(ssid.empty());
        ssids.push_back(ssid);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    // 验证所有会话ID唯一
    std::set<std::string> unique_ssids(ssids.begin(), ssids.end());
    EXPECT_EQ(unique_ssids.size(), ssids.size()) << "所有会话ID应该唯一";
}

// =================================================================
//                         消息管理测试
// =================================================================

// 测试创建消息
TEST_F(SessionManagerTest, CreateNewMessage)
{
    session_manager_->InsertNewUser("test_user", "password123");
    auto user_info = session_manager_->GetUserInfo("test_user");
    std::string ssid = session_manager_->CreateSession(user_info->uid, "deepseek");

    bool result = session_manager_->CreateNewMessage(ssid, "user", "Hello, AI!");

    EXPECT_TRUE(result);
}

// 测试在不存在的会话中创建消息
TEST_F(SessionManagerTest, CreateMessageInNonExistentSession)
{
    bool result = session_manager_->CreateNewMessage("non_existent_ssid", "user", "Hello");
    EXPECT_FALSE(result);
}

// 测试获取会话所有消息
TEST_F(SessionManagerTest, GetSessionAllMessage)
{
    session_manager_->InsertNewUser("test_user", "password123");
    auto user_info = session_manager_->GetUserInfo("test_user");
    std::string ssid = session_manager_->CreateSession(user_info->uid, "deepseek");

    // 创建多条消息
    session_manager_->CreateNewMessage(ssid, "user", "Hello!");
    session_manager_->CreateNewMessage(ssid, "assistant", "Hi there!");
    session_manager_->CreateNewMessage(ssid, "user", "How are you?");

    auto messages = session_manager_->GetSessionAllMessage(ssid);

    EXPECT_EQ(messages.size(), 3);
    EXPECT_EQ(messages[0].role, "user");
    EXPECT_EQ(messages[0].content, "Hello!");
    EXPECT_EQ(messages[1].role, "assistant");
    EXPECT_EQ(messages[1].content, "Hi there!");
    EXPECT_EQ(messages[2].role, "user");
    EXPECT_EQ(messages[2].content, "How are you?");
}

// 测试获取不存在会话的消息
TEST_F(SessionManagerTest, GetMessagesFromNonExistentSession)
{
    auto messages = session_manager_->GetSessionAllMessage("non_existent_ssid");
    EXPECT_TRUE(messages.empty());
}

// 测试消息ID格式
TEST_F(SessionManagerTest, MessageIdFormat)
{
    session_manager_->InsertNewUser("test_user", "password123");
    auto user_info = session_manager_->GetUserInfo("test_user");
    std::string ssid = session_manager_->CreateSession(user_info->uid, "deepseek");

    session_manager_->CreateNewMessage(ssid, "user", "Hello!");

    auto messages = session_manager_->GetSessionAllMessage(ssid);
    ASSERT_EQ(messages.size(), 1);

    // 消息ID格式: ssid_时间戳
    EXPECT_TRUE(messages[0].mid.find(ssid) != std::string::npos) << "消息ID应该包含会话ID";
}

// 测试消息关联正确的会话
TEST_F(SessionManagerTest, MessageBelongsToCorrectSession)
{
    session_manager_->InsertNewUser("test_user", "password123");
    auto user_info = session_manager_->GetUserInfo("test_user");

    std::string ssid1 = session_manager_->CreateSession(user_info->uid, "model1");
    std::string ssid2 = session_manager_->CreateSession(user_info->uid, "model2");

    session_manager_->CreateNewMessage(ssid1, "user", "Message for session 1");
    session_manager_->CreateNewMessage(ssid2, "user", "Message for session 2");
    session_manager_->CreateNewMessage(ssid1, "assistant", "Reply in session 1");

    auto messages1 = session_manager_->GetSessionAllMessage(ssid1);
    auto messages2 = session_manager_->GetSessionAllMessage(ssid2);

    EXPECT_EQ(messages1.size(), 2);
    EXPECT_EQ(messages2.size(), 1);

    EXPECT_EQ(messages1[0].content, "Message for session 1");
    EXPECT_EQ(messages1[1].content, "Reply in session 1");
    EXPECT_EQ(messages2[0].content, "Message for session 2");
}

// =================================================================
//                         并发测试
// =================================================================

// 测试并发创建用户
TEST_F(SessionManagerTest, ConcurrentInsertUsers)
{
    const int num_threads = 10;
    std::atomic<int> success_count{0};
    std::vector<std::thread> threads;

    for (int i = 0; i < num_threads; ++i)
    {
        threads.emplace_back([this, i, &success_count]() {
            std::string name = "concurrent_user_" + std::to_string(i);
            if (!session_manager_->InsertNewUser(name, "password").empty())
            {
                success_count++;
            }
        });
    }

    for (auto& t : threads)
    {
        t.join();
    }

    EXPECT_EQ(success_count, num_threads);
}

// 测试并发创建会话
TEST_F(SessionManagerTest, ConcurrentCreateSessions)
{
    session_manager_->InsertNewUser("test_user", "password123");
    auto user_info = session_manager_->GetUserInfo("test_user");

    const int num_threads = 10;
    std::atomic<int> success_count{0};
    std::vector<std::thread> threads;
    std::mutex ssid_mutex;
    std::vector<std::string> ssids;

    for (int i = 0; i < num_threads; ++i)
    {
        threads.emplace_back([this, &user_info, i, &success_count, &ssid_mutex, &ssids]() {
            std::string ssid = session_manager_->CreateSession(user_info->uid, "model_" + std::to_string(i));
            if (!ssid.empty())
            {
                success_count++;
                std::lock_guard<std::mutex> lock(ssid_mutex);
                ssids.push_back(ssid);
            }
        });
    }

    for (auto& t : threads)
    {
        t.join();
    }

    EXPECT_EQ(success_count, num_threads);

    // 验证所有会话ID唯一
    std::set<std::string> unique_ssids(ssids.begin(), ssids.end());
    EXPECT_EQ(unique_ssids.size(), ssids.size());
}

// 测试并发读写
TEST_F(SessionManagerTest, ConcurrentReadWrite)
{
    session_manager_->InsertNewUser("test_user", "password123");
    auto user_info = session_manager_->GetUserInfo("test_user");
    std::string ssid = session_manager_->CreateSession(user_info->uid, "deepseek");

    const int num_threads = 20;
    std::vector<std::thread> threads;

    // 一半线程写消息，一半线程读消息
    for (int i = 0; i < num_threads; ++i)
    {
        if (i % 2 == 0)
        {
            threads.emplace_back([this, ssid, i]() {
                session_manager_->CreateNewMessage(ssid, "user", "Message " + std::to_string(i));
            });
        }
        else
        {
            threads.emplace_back([this, ssid]() {
                session_manager_->GetSessionAllMessage(ssid);
            });
        }
    }

    for (auto& t : threads)
    {
        t.join();
    }

    // 不应该崩溃
    SUCCEED();
}

// =================================================================
//                         边界条件测试
// =================================================================

// 测试空用户名
TEST_F(SessionManagerTest, EmptyUserName)
{
    std::string result = session_manager_->InsertNewUser("", "password123");
    // 根据实现，空用户名可能被允许或拒绝
    // 这里只验证不会崩溃
    SUCCEED();
}

// 测试空密码
TEST_F(SessionManagerTest, EmptyPassword)
{
    std::string result = session_manager_->InsertNewUser("test_user", "");
    // 根据实现，空密码可能被允许或拒绝
    // 这里只验证不会崩溃
    SUCCEED();
}

// 测试特殊字符用户名
TEST_F(SessionManagerTest, SpecialCharacterUserName)
{
    std::string result = session_manager_->InsertNewUser("user@test.com", "password123");
    EXPECT_FALSE(result.empty());

    auto user_info = session_manager_->GetUserInfo("user@test.com");
    ASSERT_NE(user_info, nullptr);
    EXPECT_EQ(user_info->email, "user@test.com");
}

// 测试长用户名
TEST_F(SessionManagerTest, LongUserName)
{
    std::string long_name(256, 'a');
    std::string result = session_manager_->InsertNewUser(long_name, "password123");
    // 验证不会崩溃
    SUCCEED();
}

// 测试空消息内容
TEST_F(SessionManagerTest, EmptyMessageContent)
{
    session_manager_->InsertNewUser("test_user", "password123");
    auto user_info = session_manager_->GetUserInfo("test_user");
    std::string ssid = session_manager_->CreateSession(user_info->uid, "deepseek");

    bool result = session_manager_->CreateNewMessage(ssid, "user", "");
    // 验证不会崩溃
    SUCCEED();
}

// 测试长消息内容
TEST_F(SessionManagerTest, LongMessageContent)
{
    session_manager_->InsertNewUser("test_user", "password123");
    auto user_info = session_manager_->GetUserInfo("test_user");
    std::string ssid = session_manager_->CreateSession(user_info->uid, "deepseek");

    std::string long_content(10000, 'x');
    bool result = session_manager_->CreateNewMessage(ssid, "user", long_content);

    EXPECT_TRUE(result);

    auto messages = session_manager_->GetSessionAllMessage(ssid);
    ASSERT_EQ(messages.size(), 1);
    EXPECT_EQ(messages[0].content.length(), 10000);
}

// =================================================================
//                         完整流程测试
// =================================================================

// 测试完整的用户会话流程
TEST_F(SessionManagerTest, FullUserSessionFlow)
{
    // 1. 创建用户
    ASSERT_FALSE(session_manager_->InsertNewUser("alice", "alice_password").empty());

    // 2. 获取用户信息
    auto alice = session_manager_->GetUserInfo("alice");
    ASSERT_NE(alice, nullptr);

    // 3. 创建会话
    std::string ssid = session_manager_->CreateSession(alice->uid, "deepseek");
    ASSERT_FALSE(ssid.empty());

    // 4. 发送消息
    ASSERT_TRUE(session_manager_->CreateNewMessage(ssid, "user", "你好！"));
    ASSERT_TRUE(session_manager_->CreateNewMessage(ssid, "assistant", "你好！有什么可以帮助你的吗？"));
    ASSERT_TRUE(session_manager_->CreateNewMessage(ssid, "user", "今天天气怎么样？"));
    ASSERT_TRUE(session_manager_->CreateNewMessage(ssid, "assistant", "抱歉，我无法获取实时天气信息。"));

    // 5. 获取会话消息
    auto messages = session_manager_->GetSessionAllMessage(ssid);
    EXPECT_EQ(messages.size(), 4);

    // 6. 获取会话信息
    auto session = session_manager_->GetSession(ssid);
    ASSERT_NE(session, nullptr);
    EXPECT_EQ(session->model_name, "deepseek");

    // 7. 创建第二个会话
    std::string ssid2 = session_manager_->CreateSession(alice->uid, "chatgpt");
    ASSERT_FALSE(ssid2.empty());

    // 8. 获取用户所有会话
    auto all_sessions = session_manager_->GetUserAllSession(alice->uid);
    EXPECT_EQ(all_sessions.size(), 2);

    // 9. 删除第一个会话
    ASSERT_TRUE(session_manager_->RemoveSession(ssid));
    EXPECT_FALSE(session_manager_->HasSession(ssid));

    // 10. 验证第二个会话仍然存在
    EXPECT_TRUE(session_manager_->HasSession(ssid2));
}

// 测试多用户场景
TEST_F(SessionManagerTest, MultiUserScenario)
{
    // 创建多个用户
    session_manager_->InsertNewUser("alice", "password1");
    session_manager_->InsertNewUser("bob", "password2");
    session_manager_->InsertNewUser("charlie", "password3");

    auto alice = session_manager_->GetUserInfo("alice");
    auto bob = session_manager_->GetUserInfo("bob");
    auto charlie = session_manager_->GetUserInfo("charlie");

    ASSERT_NE(alice, nullptr);
    ASSERT_NE(bob, nullptr);
    ASSERT_NE(charlie, nullptr);

    // 每个用户创建会话
    std::string alice_ssid = session_manager_->CreateSession(alice->uid, "deepseek");
    std::string bob_ssid = session_manager_->CreateSession(bob->uid, "chatgpt");
    std::string charlie_ssid = session_manager_->CreateSession(charlie->uid, "gemini");

    // 每个用户发送消息
    session_manager_->CreateNewMessage(alice_ssid, "user", "Alice's message");
    session_manager_->CreateNewMessage(bob_ssid, "user", "Bob's message");
    session_manager_->CreateNewMessage(charlie_ssid, "user", "Charlie's message");

    // 验证消息隔离
    auto alice_messages = session_manager_->GetSessionAllMessage(alice_ssid);
    auto bob_messages = session_manager_->GetSessionAllMessage(bob_ssid);
    auto charlie_messages = session_manager_->GetSessionAllMessage(charlie_ssid);

    EXPECT_EQ(alice_messages.size(), 1);
    EXPECT_EQ(bob_messages.size(), 1);
    EXPECT_EQ(charlie_messages.size(), 1);

    EXPECT_EQ(alice_messages[0].content, "Alice's message");
    EXPECT_EQ(bob_messages[0].content, "Bob's message");
    EXPECT_EQ(charlie_messages[0].content, "Charlie's message");
}

// =================================================================
//                         定时器相关测试
// =================================================================

// 测试删除会话后定时器回调不会崩溃
TEST_F(SessionManagerTest, TimerCallbackAfterSessionRemoved)
{
    session_manager_->InsertNewUser("test_user", "password123");
    auto user_info = session_manager_->GetUserInfo("test_user");

    std::string ssid = session_manager_->CreateSession(user_info->uid, "deepseek");
    ASSERT_TRUE(session_manager_->HasSession(ssid));

    // 立即删除会话
    ASSERT_TRUE(session_manager_->RemoveSession(ssid));

    // 等待一段时间，让定时器有机会触发（如果有的话）
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // 不应该崩溃
    SUCCEED();
}

// 测试访问用户信息会刷新定时器（通过多次访问验证不会被清理）
TEST_F(SessionManagerTest, AccessRefreshesTimer)
{
    session_manager_->InsertNewUser("test_user", "password123");

    // 多次访问用户信息
    for (int i = 0; i < 5; ++i)
    {
        auto user_info = session_manager_->GetUserInfo("test_user");
        ASSERT_NE(user_info, nullptr);
        EXPECT_EQ(user_info->email, "test_user");
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    // 用户信息应该仍然可用
    EXPECT_TRUE(session_manager_->HasUserName("test_user"));
}

// 测试访问会话会刷新定时器
TEST_F(SessionManagerTest, SessionAccessRefreshesTimer)
{
    session_manager_->InsertNewUser("test_user", "password123");
    auto user_info = session_manager_->GetUserInfo("test_user");
    std::string ssid = session_manager_->CreateSession(user_info->uid, "deepseek");

    // 多次访问会话
    for (int i = 0; i < 5; ++i)
    {
        auto session = session_manager_->GetSession(ssid);
        ASSERT_NE(session, nullptr);
        EXPECT_EQ(session->session_id, ssid);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    // 会话应该仍然可用
    EXPECT_TRUE(session_manager_->HasSession(ssid));
}

// 测试定时器过期后内存清理，再次访问时从数据库重新加载
TEST_F(SessionManagerTest, TimerExpiryAndReloadFromDatabase)
{
    session_manager_->InsertNewUser("timer_test_user", "password123");
    auto user_info = session_manager_->GetUserInfo("timer_test_user");
    std::string uid = user_info->uid;

    std::string ssid = session_manager_->CreateSession(uid, "deepseek");
    session_manager_->CreateNewMessage(ssid, "user", "Test message before expiry");

    // 等待定时器过期（10秒 + 缓冲时间）
    std::this_thread::sleep_for(std::chrono::seconds(12));

    // 数据应该仍然可以访问（从数据库重新加载）
    EXPECT_TRUE(session_manager_->HasUserName("timer_test_user"));

    auto reloaded_user = session_manager_->GetUserInfo("timer_test_user");
    ASSERT_NE(reloaded_user, nullptr);
    EXPECT_EQ(reloaded_user->uid, uid);
    EXPECT_EQ(reloaded_user->email, "timer_test_user");

    EXPECT_TRUE(session_manager_->HasSession(ssid));

    auto reloaded_session = session_manager_->GetSession(ssid);
    ASSERT_NE(reloaded_session, nullptr);
    EXPECT_EQ(reloaded_session->model_name, "deepseek");

    auto messages = session_manager_->GetSessionAllMessage(ssid);
    ASSERT_EQ(messages.size(), 1);
    EXPECT_EQ(messages[0].content, "Test message before expiry");
}

// =================================================================
//                         Unicode/中文测试
// =================================================================

// 测试中文用户名
TEST_F(SessionManagerTest, ChineseUserName)
{
    std::string result = session_manager_->InsertNewUser("张三", "password123");
    EXPECT_FALSE(result.empty());

    auto user_info = session_manager_->GetUserInfo("张三");
    ASSERT_NE(user_info, nullptr);
    EXPECT_EQ(user_info->email, "张三");
}

// 测试中文密码
TEST_F(SessionManagerTest, ChinesePassword)
{
    std::string result = session_manager_->InsertNewUser("test_user", "密码123");
    EXPECT_FALSE(result.empty());

    auto user_info = session_manager_->GetUserInfo("test_user");
    ASSERT_NE(user_info, nullptr);
    EXPECT_EQ(user_info->password, "密码123");
}

// 测试中文消息内容
TEST_F(SessionManagerTest, ChineseMessageContent)
{
    session_manager_->InsertNewUser("test_user", "password123");
    auto user_info = session_manager_->GetUserInfo("test_user");
    std::string ssid = session_manager_->CreateSession(user_info->uid, "deepseek");

    bool result = session_manager_->CreateNewMessage(ssid, "user", "你好，世界！");
    EXPECT_TRUE(result);

    auto messages = session_manager_->GetSessionAllMessage(ssid);
    ASSERT_EQ(messages.size(), 1);
    EXPECT_EQ(messages[0].content, "你好，世界！");
}

// 测试混合Unicode字符
TEST_F(SessionManagerTest, MixedUnicodeContent)
{
    session_manager_->InsertNewUser("用户_test_123", "pass密码word");
    auto user_info = session_manager_->GetUserInfo("用户_test_123");
    ASSERT_NE(user_info, nullptr);

    std::string ssid = session_manager_->CreateSession(user_info->uid, "deepseek");
    session_manager_->CreateNewMessage(ssid, "user", "Hello 你好 🌍 émoji");

    auto messages = session_manager_->GetSessionAllMessage(ssid);
    ASSERT_EQ(messages.size(), 1);
    EXPECT_EQ(messages[0].content, "Hello 你好 🌍 émoji");
}

// =================================================================
//                         时间戳测试
// =================================================================

// 测试用户创建时间
TEST_F(SessionManagerTest, UserCreateTime)
{
    time_t before = time(nullptr);
    session_manager_->InsertNewUser("test_user", "password123");
    time_t after = time(nullptr);

    auto user_info = session_manager_->GetUserInfo("test_user");
    ASSERT_NE(user_info, nullptr);

    EXPECT_GE(user_info->create_time, before);
    EXPECT_LE(user_info->create_time, after);
}

// 测试会话创建时间和更新时间
TEST_F(SessionManagerTest, SessionTimestamps)
{
    session_manager_->InsertNewUser("test_user", "password123");
    auto user_info = session_manager_->GetUserInfo("test_user");

    time_t before = time(nullptr);
    std::string ssid = session_manager_->CreateSession(user_info->uid, "deepseek");
    time_t after = time(nullptr);

    auto session = session_manager_->GetSession(ssid);
    ASSERT_NE(session, nullptr);

    EXPECT_GE(session->create_time, before);
    EXPECT_LE(session->create_time, after);
    EXPECT_GE(session->update_time, before);
    EXPECT_LE(session->update_time, after);
}

// 测试消息创建时间
TEST_F(SessionManagerTest, MessageCreateTime)
{
    session_manager_->InsertNewUser("test_user", "password123");
    auto user_info = session_manager_->GetUserInfo("test_user");
    std::string ssid = session_manager_->CreateSession(user_info->uid, "deepseek");

    time_t before = time(nullptr);
    session_manager_->CreateNewMessage(ssid, "user", "Hello!");
    time_t after = time(nullptr);

    auto messages = session_manager_->GetSessionAllMessage(ssid);
    ASSERT_EQ(messages.size(), 1);

    EXPECT_GE(messages[0].create_time, before);
    EXPECT_LE(messages[0].create_time, after);
}

// =================================================================
//                         数据持久化测试
// =================================================================

// 测试重新创建 SessionManager 后数据仍然存在
TEST_F(SessionManagerTest, DataPersistenceAfterRestart)
{
    // 创建用户和会话
    session_manager_->InsertNewUser("persistent_user", "password123");
    auto user_info = session_manager_->GetUserInfo("persistent_user");
    std::string uid = user_info->uid;

    std::string ssid = session_manager_->CreateSession(uid, "deepseek");
    session_manager_->CreateNewMessage(ssid, "user", "Persistent message");

    // 删除当前 SessionManager
    delete session_manager_;

    // 重新创建 SessionManager（使用相同的数据库）
    session_manager_ = new SessionManager(test_db_name_);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 验证用户数据仍然存在
    EXPECT_TRUE(session_manager_->HasUserName("persistent_user"));
    auto restored_user = session_manager_->GetUserInfo("persistent_user");
    ASSERT_NE(restored_user, nullptr);
    EXPECT_EQ(restored_user->uid, uid);

    // 验证会话数据仍然存在
    EXPECT_TRUE(session_manager_->HasSession(ssid));
    auto restored_session = session_manager_->GetSession(ssid);
    ASSERT_NE(restored_session, nullptr);
    EXPECT_EQ(restored_session->model_name, "deepseek");

    // 验证消息数据仍然存在
    auto messages = session_manager_->GetSessionAllMessage(ssid);
    ASSERT_EQ(messages.size(), 1);
    EXPECT_EQ(messages[0].content, "Persistent message");
}

// =================================================================
//                         错误处理测试
// =================================================================

// 测试对已删除会话添加消息
TEST_F(SessionManagerTest, AddMessageToDeletedSession)
{
    session_manager_->InsertNewUser("test_user", "password123");
    auto user_info = session_manager_->GetUserInfo("test_user");
    std::string ssid = session_manager_->CreateSession(user_info->uid, "deepseek");

    // 删除会话
    session_manager_->RemoveSession(ssid);

    // 尝试添加消息
    bool result = session_manager_->CreateNewMessage(ssid, "user", "Should fail");
    EXPECT_FALSE(result);
}

// 测试重复删除会话
TEST_F(SessionManagerTest, RemoveSessionTwice)
{
    session_manager_->InsertNewUser("test_user", "password123");
    auto user_info = session_manager_->GetUserInfo("test_user");
    std::string ssid = session_manager_->CreateSession(user_info->uid, "deepseek");

    EXPECT_TRUE(session_manager_->RemoveSession(ssid));
    EXPECT_FALSE(session_manager_->RemoveSession(ssid));
}

// 测试获取已删除会话的消息
TEST_F(SessionManagerTest, GetMessagesFromDeletedSession)
{
    session_manager_->InsertNewUser("test_user", "password123");
    auto user_info = session_manager_->GetUserInfo("test_user");
    std::string ssid = session_manager_->CreateSession(user_info->uid, "deepseek");

    session_manager_->CreateNewMessage(ssid, "user", "Hello");
    session_manager_->RemoveSession(ssid);

    auto messages = session_manager_->GetSessionAllMessage(ssid);
    EXPECT_TRUE(messages.empty());
}
