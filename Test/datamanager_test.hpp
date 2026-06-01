
#pragma once

#include <gtest/gtest.h>
#include "data_manager.h"
#include "base/util/mylog.h"
#include <filesystem>
#include <thread>
#include <vector>

class DataManagerTest : public ::testing::Test
{
protected:
    std::string test_db_path_;
    ai_sdk::DataManager* manager_;

    static void SetUpTestSuite()
    {
        ai_sdk::Logger::initLogger("test", "stdout", spdlog::level::debug);
    }

    void SetUp() override
    {
        // 使用临时数据库文件
        test_db_path_ = "test_database.db";

        // 删除可能存在的旧测试数据库
        if (std::filesystem::exists(test_db_path_)) {
            std::filesystem::remove(test_db_path_);
        }

        manager_ = new ai_sdk::DataManager(test_db_path_);
        ASSERT_TRUE(manager_->Init()) << "数据库初始化失败";
    }

    void TearDown() override
    {
        delete manager_;
        manager_ = nullptr;

        // 清理测试数据库文件
        if (std::filesystem::exists(test_db_path_)) {
            std::filesystem::remove(test_db_path_);
        }
    }

    // 辅助函数：创建测试用户
    ai_sdk::UserInfo CreateTestUser(const std::string& uid, const std::string& email)
    {
        ai_sdk::UserInfo user;
        user.uid = uid;
        user.email = email;
        user.password = "test_password_" + uid;
        user.create_time = time(nullptr);
        return user;
    }

    // 辅助函数：创建测试会话
    ai_sdk::Session CreateTestSession(const std::string& uid, const std::string& ssid, const std::string& model)
    {
        ai_sdk::Session session;
        session.uid = uid;
        session.session_id = ssid;
        session.model_name = model;
        session.create_time = time(nullptr);
        session.update_time = time(nullptr);
        return session;
    }

    // 辅助函数：创建测试消息
    ai_sdk::Message CreateTestMessage(const std::string& ssid, const std::string& mid, const std::string& role, const std::string& content)
    {
        ai_sdk::Message message;
        message.ssid = ssid;
        message.mid = mid;
        message.role = role;
        message.content = content;
        message.create_time = time(nullptr);
        return message;
    }
};

// =================================================================
//                         初始化测试
// =================================================================

TEST_F(DataManagerTest, InitCreatesDatabase)
{
    EXPECT_TRUE(std::filesystem::exists(test_db_path_));
}

TEST_F(DataManagerTest, InitCanBeCalledMultipleTimes)
{
    EXPECT_TRUE(manager_->Init());
    EXPECT_TRUE(manager_->Init());
}

// =================================================================
//                         用户操作测试
// =================================================================

TEST_F(DataManagerTest, InsertUserSuccess)
{
    auto user = CreateTestUser("user-001", "Alice");
    EXPECT_TRUE(manager_->InsertUser(user));
}

TEST_F(DataManagerTest, InsertUserWithDuplicateUidFails)
{
    auto user1 = CreateTestUser("user-001", "Alice");
    auto user2 = CreateTestUser("user-001", "Bob");

    EXPECT_TRUE(manager_->InsertUser(user1));
    EXPECT_FALSE(manager_->InsertUser(user2)) << "重复的UID应该插入失败";
}

TEST_F(DataManagerTest, GetUserSuccess)
{
    auto user = CreateTestUser("user-001", "Alice");
    ASSERT_TRUE(manager_->InsertUser(user));

    auto retrieved = manager_->GetUser("Alice");
    EXPECT_EQ(retrieved.uid, "user-001");
    EXPECT_EQ(retrieved.email, "Alice");
    EXPECT_EQ(retrieved.password, "test_password_user-001");
    EXPECT_GT(retrieved.create_time, 0);
}

TEST_F(DataManagerTest, GetNonExistentUserReturnsEmpty)
{
    auto user = manager_->GetUser("non-existent-uid");
    EXPECT_TRUE(user.uid.empty());
    EXPECT_TRUE(user.email.empty());
}

TEST_F(DataManagerTest, RemoveUserSuccess)
{
    auto user = CreateTestUser("user-001", "Alice");
    ASSERT_TRUE(manager_->InsertUser(user));

    EXPECT_TRUE(manager_->RemoveUser("user-001"));

    auto retrieved = manager_->GetUser("user-001");
    EXPECT_TRUE(retrieved.uid.empty()) << "删除后应该查询不到用户";
}

TEST_F(DataManagerTest, RemoveUserAlsoRemovesSessions)
{
    // 创建用户和会话
    auto user = CreateTestUser("user-001", "Alice");
    ASSERT_TRUE(manager_->InsertUser(user));

    auto session = CreateTestSession("user-001", "session-001", "gpt-4");
    ASSERT_TRUE(manager_->InsertSession(session));

    // 删除用户
    EXPECT_TRUE(manager_->RemoveUser("user-001"));

    // 验证会话也被删除
    auto sessions = manager_->GetUserAllSessions("user-001");
    EXPECT_TRUE(sessions.empty()) << "删除用户后，其会话也应该被删除";
}

// =================================================================
//                         会话操作测试
// =================================================================

TEST_F(DataManagerTest, InsertSessionSuccess)
{
    auto user = CreateTestUser("user-001", "Alice");
    ASSERT_TRUE(manager_->InsertUser(user));

    auto session = CreateTestSession("user-001", "session-001", "gpt-4");
    EXPECT_TRUE(manager_->InsertSession(session));
}

TEST_F(DataManagerTest, InsertSessionWithDuplicateSsidFails)
{
    auto user = CreateTestUser("user-001", "Alice");
    ASSERT_TRUE(manager_->InsertUser(user));

    auto session1 = CreateTestSession("user-001", "session-001", "gpt-4");
    auto session2 = CreateTestSession("user-001", "session-001", "gpt-3.5");

    EXPECT_TRUE(manager_->InsertSession(session1));
    EXPECT_FALSE(manager_->InsertSession(session2)) << "重复的SSID应该插入失败";
}

TEST_F(DataManagerTest, InsertMultipleSessionsForSameUser)
{
    auto user = CreateTestUser("user-001", "Alice");
    ASSERT_TRUE(manager_->InsertUser(user));

    auto session1 = CreateTestSession("user-001", "session-001", "gpt-4");
    auto session2 = CreateTestSession("user-001", "session-002", "claude-3");

    EXPECT_TRUE(manager_->InsertSession(session1));
    EXPECT_TRUE(manager_->InsertSession(session2));
}

TEST_F(DataManagerTest, GetAllSessionsSuccess)
{
    auto user = CreateTestUser("user-001", "Alice");
    ASSERT_TRUE(manager_->InsertUser(user));

    auto session1 = CreateTestSession("user-001", "session-001", "gpt-4");
    auto session2 = CreateTestSession("user-001", "session-002", "claude-3");
    auto session3 = CreateTestSession("user-001", "session-003", "gemini");

    ASSERT_TRUE(manager_->InsertSession(session1));
    ASSERT_TRUE(manager_->InsertSession(session2));
    ASSERT_TRUE(manager_->InsertSession(session3));

    auto sessions = manager_->GetUserAllSessions("user-001");
    EXPECT_EQ(sessions.size(), 3);

    // 验证返回的session id
    EXPECT_NE(std::find(sessions.begin(), sessions.end(), "session-001"), sessions.end());
    EXPECT_NE(std::find(sessions.begin(), sessions.end(), "session-002"), sessions.end());
    EXPECT_NE(std::find(sessions.begin(), sessions.end(), "session-003"), sessions.end());
}

TEST_F(DataManagerTest, GetAllSessionsForNonExistentUserReturnsEmpty)
{
    auto sessions = manager_->GetUserAllSessions("non-existent-uid");
    EXPECT_TRUE(sessions.empty());
}

TEST_F(DataManagerTest, UpdateSessionSuccess)
{
    auto user = CreateTestUser("user-001", "Alice");
    ASSERT_TRUE(manager_->InsertUser(user));

    auto session = CreateTestSession("user-001", "session-001", "gpt-4");
    ASSERT_TRUE(manager_->InsertSession(session));

    // 等待一秒确保时间戳不同
    std::this_thread::sleep_for(std::chrono::seconds(1));

    EXPECT_TRUE(manager_->UpdateSession("session-001"));
}

TEST_F(DataManagerTest, RemoveSessionSuccess)
{
    auto user = CreateTestUser("user-001", "Alice");
    ASSERT_TRUE(manager_->InsertUser(user));

    auto session = CreateTestSession("user-001", "session-001", "gpt-4");
    ASSERT_TRUE(manager_->InsertSession(session));

    EXPECT_TRUE(manager_->RemoveSession("session-001"));

    auto sessions = manager_->GetUserAllSessions("user-001");
    EXPECT_TRUE(sessions.empty()) << "删除后应该查询不到会话";
}

TEST_F(DataManagerTest, RemoveSessionAlsoRemovesMessages)
{
    auto user = CreateTestUser("user-001", "Alice");
    ASSERT_TRUE(manager_->InsertUser(user));

    auto session = CreateTestSession("user-001", "session-001", "gpt-4");
    ASSERT_TRUE(manager_->InsertSession(session));

    auto message = CreateTestMessage("session-001", "msg-001", "user", "Hello");
    ASSERT_TRUE(manager_->InsertMessage(message));

    EXPECT_TRUE(manager_->RemoveSession("session-001"));

    auto messages = manager_->GetMessages("session-001");
    EXPECT_TRUE(messages.empty()) << "删除会话后，其消息也应该被删除";
}

TEST_F(DataManagerTest, RemoveUserAllSessionSuccess)
{
    auto user = CreateTestUser("user-001", "Alice");
    ASSERT_TRUE(manager_->InsertUser(user));

    auto session1 = CreateTestSession("user-001", "session-001", "gpt-4");
    auto session2 = CreateTestSession("user-001", "session-002", "claude-3");
    ASSERT_TRUE(manager_->InsertSession(session1));
    ASSERT_TRUE(manager_->InsertSession(session2));

    EXPECT_TRUE(manager_->RemoveUserAllSession("user-001"));

    auto sessions = manager_->GetUserAllSessions("user-001");
    EXPECT_TRUE(sessions.empty()) << "删除用户所有会话后应该为空";
}

// =================================================================
//                         消息操作测试
// =================================================================

TEST_F(DataManagerTest, InsertMessageSuccess)
{
    auto user = CreateTestUser("user-001", "Alice");
    ASSERT_TRUE(manager_->InsertUser(user));

    auto session = CreateTestSession("user-001", "session-001", "gpt-4");
    ASSERT_TRUE(manager_->InsertSession(session));

    auto message = CreateTestMessage("session-001", "msg-001", "user", "Hello, AI!");
    EXPECT_TRUE(manager_->InsertMessage(message));
}

TEST_F(DataManagerTest, InsertMessageWithDuplicateMidFails)
{
    auto user = CreateTestUser("user-001", "Alice");
    ASSERT_TRUE(manager_->InsertUser(user));

    auto session = CreateTestSession("user-001", "session-001", "gpt-4");
    ASSERT_TRUE(manager_->InsertSession(session));

    auto message1 = CreateTestMessage("session-001", "msg-001", "user", "Hello");
    auto message2 = CreateTestMessage("session-001", "msg-001", "assistant", "Hi");

    EXPECT_TRUE(manager_->InsertMessage(message1));
    EXPECT_FALSE(manager_->InsertMessage(message2)) << "重复的MID应该插入失败";
}

TEST_F(DataManagerTest, InsertMultipleMessagesInSession)
{
    auto user = CreateTestUser("user-001", "Alice");
    ASSERT_TRUE(manager_->InsertUser(user));

    auto session = CreateTestSession("user-001", "session-001", "gpt-4");
    ASSERT_TRUE(manager_->InsertSession(session));

    auto msg1 = CreateTestMessage("session-001", "msg-001", "user", "Hello");
    auto msg2 = CreateTestMessage("session-001", "msg-002", "assistant", "Hi there!");
    auto msg3 = CreateTestMessage("session-001", "msg-003", "user", "How are you?");

    EXPECT_TRUE(manager_->InsertMessage(msg1));
    EXPECT_TRUE(manager_->InsertMessage(msg2));
    EXPECT_TRUE(manager_->InsertMessage(msg3));
}

TEST_F(DataManagerTest, GetMessagesSuccess)
{
    auto user = CreateTestUser("user-001", "Alice");
    ASSERT_TRUE(manager_->InsertUser(user));

    auto session = CreateTestSession("user-001", "session-001", "gpt-4");
    ASSERT_TRUE(manager_->InsertSession(session));

    auto msg1 = CreateTestMessage("session-001", "msg-001", "user", "Hello");
    auto msg2 = CreateTestMessage("session-001", "msg-002", "assistant", "Hi there!");
    auto msg3 = CreateTestMessage("session-001", "msg-003", "user", "How are you?");

    ASSERT_TRUE(manager_->InsertMessage(msg1));
    ASSERT_TRUE(manager_->InsertMessage(msg2));
    ASSERT_TRUE(manager_->InsertMessage(msg3));

    auto messages = manager_->GetMessages("session-001");
    EXPECT_EQ(messages.size(), 3);

    // 验证消息内容
    EXPECT_EQ(messages[0].mid, "msg-001");
    EXPECT_EQ(messages[0].role, "user");
    EXPECT_EQ(messages[0].content, "Hello");
    EXPECT_EQ(messages[0].ssid, "session-001");

    EXPECT_EQ(messages[1].mid, "msg-002");
    EXPECT_EQ(messages[1].role, "assistant");
    EXPECT_EQ(messages[1].content, "Hi there!");

    EXPECT_EQ(messages[2].mid, "msg-003");
    EXPECT_EQ(messages[2].role, "user");
    EXPECT_EQ(messages[2].content, "How are you?");
}

TEST_F(DataManagerTest, GetMessagesForNonExistentSessionReturnsEmpty)
{
    auto messages = manager_->GetMessages("non-existent-ssid");
    EXPECT_TRUE(messages.empty());
}

TEST_F(DataManagerTest, RemoveMessageSuccess)
{
    auto user = CreateTestUser("user-001", "Alice");
    ASSERT_TRUE(manager_->InsertUser(user));

    auto session = CreateTestSession("user-001", "session-001", "gpt-4");
    ASSERT_TRUE(manager_->InsertSession(session));

    auto message = CreateTestMessage("session-001", "msg-001", "user", "Hello");
    ASSERT_TRUE(manager_->InsertMessage(message));

    EXPECT_TRUE(manager_->RemoveMessage("msg-001"));

    auto messages = manager_->GetMessages("session-001");
    EXPECT_TRUE(messages.empty()) << "删除后应该查询不到消息";
}

TEST_F(DataManagerTest, RemoveSessionAllMessageSuccess)
{
    auto user = CreateTestUser("user-001", "Alice");
    ASSERT_TRUE(manager_->InsertUser(user));

    auto session = CreateTestSession("user-001", "session-001", "gpt-4");
    ASSERT_TRUE(manager_->InsertSession(session));

    auto msg1 = CreateTestMessage("session-001", "msg-001", "user", "Hello");
    auto msg2 = CreateTestMessage("session-001", "msg-002", "assistant", "Hi");
    auto msg3 = CreateTestMessage("session-001", "msg-003", "user", "Bye");

    ASSERT_TRUE(manager_->InsertMessage(msg1));
    ASSERT_TRUE(manager_->InsertMessage(msg2));
    ASSERT_TRUE(manager_->InsertMessage(msg3));

    EXPECT_TRUE(manager_->RemoveSessionAllMessage("session-001"));

    auto messages = manager_->GetMessages("session-001");
    EXPECT_TRUE(messages.empty()) << "删除会话所有消息后应该为空";
}

// =================================================================
//                         复杂场景测试
// =================================================================

TEST_F(DataManagerTest, CompleteWorkflowTest)
{
    // 1. 创建用户
    auto user = CreateTestUser("user-001", "Alice");
    ASSERT_TRUE(manager_->InsertUser(user));

    // 2. 创建多个会话
    auto session1 = CreateTestSession("user-001", "session-001", "gpt-4");
    auto session2 = CreateTestSession("user-001", "session-002", "claude-3");
    ASSERT_TRUE(manager_->InsertSession(session1));
    ASSERT_TRUE(manager_->InsertSession(session2));

    // 3. 在第一个会话中添加消息
    auto msg1 = CreateTestMessage("session-001", "msg-001", "user", "Hello");
    auto msg2 = CreateTestMessage("session-001", "msg-002", "assistant", "Hi there!");
    ASSERT_TRUE(manager_->InsertMessage(msg1));
    ASSERT_TRUE(manager_->InsertMessage(msg2));

    // 4. 在第二个会话中添加消息
    auto msg3 = CreateTestMessage("session-002", "msg-003", "user", "Test");
    ASSERT_TRUE(manager_->InsertMessage(msg3));

    // 5. 验证数据
    auto sessions = manager_->GetUserAllSessions("user-001");
    EXPECT_EQ(sessions.size(), 2);

    auto messages1 = manager_->GetMessages("session-001");
    EXPECT_EQ(messages1.size(), 2);

    auto messages2 = manager_->GetMessages("session-002");
    EXPECT_EQ(messages2.size(), 1);

    // 6. 删除一个会话
    EXPECT_TRUE(manager_->RemoveSession("session-001"));

    sessions = manager_->GetUserAllSessions("user-001");
    EXPECT_EQ(sessions.size(), 1);

    messages1 = manager_->GetMessages("session-001");
    EXPECT_TRUE(messages1.empty());

    // 7. 删除用户（级联删除所有会话）
    EXPECT_TRUE(manager_->RemoveUser("user-001"));

    sessions = manager_->GetUserAllSessions("user-001");
    EXPECT_TRUE(sessions.empty());
}

TEST_F(DataManagerTest, MultipleUsersAndSessions)
{
    // 创建多个用户
    auto user1 = CreateTestUser("user-001", "Alice");
    auto user2 = CreateTestUser("user-002", "Bob");
    ASSERT_TRUE(manager_->InsertUser(user1));
    ASSERT_TRUE(manager_->InsertUser(user2));

    // 每个用户创建会话
    auto session1 = CreateTestSession("user-001", "session-001", "gpt-4");
    auto session2 = CreateTestSession("user-002", "session-002", "claude-3");
    ASSERT_TRUE(manager_->InsertSession(session1));
    ASSERT_TRUE(manager_->InsertSession(session2));

    // 验证每个用户只能看到自己的会话
    auto sessions1 = manager_->GetUserAllSessions("user-001");
    EXPECT_EQ(sessions1.size(), 1);
    EXPECT_EQ(sessions1[0], "session-001");

    auto sessions2 = manager_->GetUserAllSessions("user-002");
    EXPECT_EQ(sessions2.size(), 1);
    EXPECT_EQ(sessions2[0], "session-002");
}

// =================================================================
//                         线程安全测试
// =================================================================

TEST_F(DataManagerTest, ConcurrentInsertUsers)
{
    const int num_threads = 10;
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([this, i, &success_count]() {
            auto user = CreateTestUser("user-" + std::to_string(i), "User" + std::to_string(i));
            if (manager_->InsertUser(user)) {
                success_count++;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(success_count, num_threads) << "所有并发插入都应该成功";
}

TEST_F(DataManagerTest, ConcurrentReadWrite)
{
    // 先插入一些数据
    auto user = CreateTestUser("user-001", "Alice");
    ASSERT_TRUE(manager_->InsertUser(user));

    auto session = CreateTestSession("user-001", "session-001", "gpt-4");
    ASSERT_TRUE(manager_->InsertSession(session));

    const int num_threads = 5;
    std::vector<std::thread> threads;

    // 一些线程读取，一些线程写入
    for (int i = 0; i < num_threads; ++i) {
        if (i % 2 == 0) {
            // 读取线程
            threads.emplace_back([this]() {
                for (int j = 0; j < 10; ++j) {
                    auto sessions = manager_->GetUserAllSessions("user-001");
                    EXPECT_GE(sessions.size(), 1);
                }
            });
        } else {
            // 写入线程
            threads.emplace_back([this, i]() {
                for (int j = 0; j < 10; ++j) {
                    auto msg = CreateTestMessage("session-001",
                                                "msg-" + std::to_string(i) + "-" + std::to_string(j),
                                                "user",
                                                "Message " + std::to_string(j));
                    manager_->InsertMessage(msg);
                }
            });
        }
    }

    for (auto& t : threads) {
        t.join();
    }

    // 验证数据一致性
    auto messages = manager_->GetMessages("session-001");
    EXPECT_GT(messages.size(), 0);
}

// =================================================================
//                         边界条件测试
// =================================================================

TEST_F(DataManagerTest, EmptyStringHandling)
{
    auto user = CreateTestUser("", "");
    // 空UID应该能插入（虽然不推荐）
    EXPECT_TRUE(manager_->InsertUser(user));
}

TEST_F(DataManagerTest, LongContentHandling)
{
    auto user = CreateTestUser("user-001", "Alice");
    ASSERT_TRUE(manager_->InsertUser(user));

    auto session = CreateTestSession("user-001", "session-001", "gpt-4");
    ASSERT_TRUE(manager_->InsertSession(session));

    // 创建一个很长的消息内容
    std::string long_content(10000, 'A');
    auto message = CreateTestMessage("session-001", "msg-001", "user", long_content);
    EXPECT_TRUE(manager_->InsertMessage(message));

    auto messages = manager_->GetMessages("session-001");
    ASSERT_EQ(messages.size(), 1);
    EXPECT_EQ(messages[0].content.length(), 10000);
}

TEST_F(DataManagerTest, SpecialCharactersInContent)
{
    auto user = CreateTestUser("user-001", "Alice");
    ASSERT_TRUE(manager_->InsertUser(user));

    auto session = CreateTestSession("user-001", "session-001", "gpt-4");
    ASSERT_TRUE(manager_->InsertSession(session));

    // 包含特殊字符的内容
    std::string special_content = "Hello 'world' \"test\" \n\t\r \\ ; -- /* */";
    auto message = CreateTestMessage("session-001", "msg-001", "user", special_content);
    EXPECT_TRUE(manager_->InsertMessage(message));

    auto messages = manager_->GetMessages("session-001");
    ASSERT_EQ(messages.size(), 1);
    EXPECT_EQ(messages[0].content, special_content);
}
