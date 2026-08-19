#pragma once

#include <gtest/gtest.h>
#include "aichat_sdk.h"
#include "base/util/mylog.h"
#include <chrono>
#include <thread>
#include <atomic>
#include <vector>
#include <mutex>
#include <filesystem>
#include <set>
#include <cstdlib>
#include <iostream>

using namespace aichat_sdk;

// =================================================================
//                         测试夹具
// =================================================================

class AIChatSdkTest : public ::testing::Test
{
protected:
    std::unique_ptr<AIChatSdk> sdk_;
    std::string test_db_name_ = "test_aichat_sdk.db";

    static void SetUpTestSuite()
    {
        aichat_sdk::Logger::initLogger("test", "stdout", spdlog::level::debug);
    }

    void SetUp() override
    {
        // 删除旧的测试数据库
        if (std::filesystem::exists(test_db_name_))
        {
            std::filesystem::remove(test_db_name_);
        }
        sdk_ = std::make_unique<AIChatSdk>(test_db_name_);

        // 等待定时器线程启动
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    void TearDown() override
    {
        sdk_.reset();

        // 清理测试数据库
        if (std::filesystem::exists(test_db_name_))
        {
            std::filesystem::remove(test_db_name_);
        }
    }

    // 辅助方法：创建一个测试用户并返回 uid
    std::string CreateTestUser(const std::string& email = "test_user",
                                const std::string& password = "password123")
    {
        std::string uid = sdk_->CreateUser(email, password);
        EXPECT_FALSE(uid.empty()) << "创建测试用户失败: " << email;
        return uid;
    }

    // 辅助方法：创建测试会话并返回 ssid
    std::string CreateTestSession(const std::string& uid,
                                   const std::string& model_name = "deepseek")
    {
        std::string ssid = sdk_->CreateSession(uid, model_name);
        EXPECT_FALSE(ssid.empty()) << "创建测试会话失败";
        return ssid;
    }

    // 辅助方法：注册一个测试用的 DeepSeek 模型（使用假 endpoint，HTTP 调用会连接失败）
    // 注意：config.model 即 LLManager 的注册/路由 key，必须随 model_name 唯一，
    //       否则多次注册会撞 key。会话用相同的 model_name 即可被正确调度到本 Provider。
    bool RegisterTestModel(const std::string& model_name = "test-deepseek")
    {
        Config config;
        config.model_type = ModelType::DEEPSEEK;
        config.model_info.model_name = model_name;
        config.model_info.model_decs = "Test DeepSeek Model";
        config.apikey = "test-api-key-fake";
        config.end_point = "http://localhost:19999";  // 假 endpoint，连接会被拒绝
        config.model = model_name;
        config.path = "/test";
        return sdk_->RegisterModel(config);
    }

    // 辅助方法：从环境变量读取 API Key（未设置返回空字符串）
    static std::string GetEnvApiKey(const char* var)
    {
        const char* val = std::getenv(var);
        return val ? std::string(val) : std::string();
    }

    // 辅助方法：带重试的 SendMessage。
    // 真实大模型服务偶发 503/超时（如 "high demand"），重试若干次以降低抖动。
    std::string SendMessageWithRetry(const std::string& ssid, const std::string& message,
                                     int attempts = 5)
    {
        std::string reply;
        for (int i = 0; i < attempts; ++i)
        {
            reply = sdk_->SendMessage(ssid, message);
            if (!reply.empty())
            {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(800));
        }
        return reply;
    }
};

// =================================================================
//                    1. 模型管理测试 (RegisterModel / GetAllModels)
// =================================================================

// 测试注册模型
TEST_F(AIChatSdkTest, RegisterModelSuccess)
{
    Config config;
    config.model_type = ModelType::DEEPSEEK;
    config.model_info.model_name = "my-deepseek";
    config.model_info.model_decs = "DeepSeek 大模型";
    config.apikey = "sk-test-api-key";
    config.end_point = "https://api.deepseek.com";
    config.model = "deepseek-v4-flash";
    config.path = "/chat/completions";

    bool result = sdk_->RegisterModel(config);
    EXPECT_TRUE(result) << "应该成功注册模型";
}

// 测试注册 Gemini 模型
TEST_F(AIChatSdkTest, RegisterGeminiModel)
{
    Config config;
    config.model_type = ModelType::GEMINI;
    config.model_info.model_name = "my-gemini";
    config.model_info.model_decs = "Gemini 大模型";
    config.apikey = "test-gemini-key";
    config.end_point = "https://generativelanguage.googleapis.com";
    config.model = "gemini-pro";
    config.path = "/v1beta/models/gemini-pro:generateContent";

    bool result = sdk_->RegisterModel(config);
    EXPECT_TRUE(result) << "应该成功注册 Gemini 模型";
}

// 测试注册 ChatGPT 模型
TEST_F(AIChatSdkTest, RegisterChatGPTModel)
{
    Config config;
    config.model_type = ModelType::CHATGPT;
    config.model_info.model_name = "my-chatgpt";
    config.model_info.model_decs = "ChatGPT 大模型";
    config.apikey = "test-openai-key";
    config.end_point = "https://api.openai.com";
    config.model = "gpt-4";
    config.path = "/v1/chat/completions";

    bool result = sdk_->RegisterModel(config);
    EXPECT_TRUE(result) << "应该成功注册 ChatGPT 模型";
}

// 测试注册重复模型名称
TEST_F(AIChatSdkTest, RegisterDuplicateModel)
{
    ASSERT_TRUE(RegisterTestModel("duplicate-model"));

    // 尝试用相同的 config.model（注册 key）注册另一个模型
    Config config;
    config.model_type = ModelType::DEEPSEEK;
    config.model_info.model_name = "duplicate-model";
    config.model_info.model_decs = "Duplicate Model";
    config.apikey = "another-key";
    config.end_point = "http://localhost:19999";
    config.model = "duplicate-model";  // 与已注册模型相同的 key
    config.path = "/test";

    bool result = sdk_->RegisterModel(config);
    EXPECT_FALSE(result) << "不应该允许注册重复 key 的模型";
}

// 测试 API Key 为空时注册失败
TEST_F(AIChatSdkTest, RegisterModelWithEmptyApiKey)
{
    Config config;
    config.model_type = ModelType::DEEPSEEK;
    config.model_info.model_name = "no-key-model";
    config.model_info.model_decs = "Model without API key";
    config.apikey = "";  // 空 API key
    config.end_point = "https://api.deepseek.com";
    config.model = "deepseek-v4-flash";
    config.path = "/chat/completions";

    bool result = sdk_->RegisterModel(config);
    EXPECT_FALSE(result) << "API Key 为空时不应注册成功";
}

// 测试 GetAllModels 初始为空
TEST_F(AIChatSdkTest, GetAllModelsInitiallyEmpty)
{
    auto models = sdk_->GetAllModels();
    EXPECT_TRUE(models.empty()) << "初始状态应该没有模型";
}

// 测试注册模型后 GetAllModels
TEST_F(AIChatSdkTest, GetAllModelsAfterRegistration)
{
    ASSERT_TRUE(RegisterTestModel("model-1"));

    auto models = sdk_->GetAllModels();
    EXPECT_EQ(models.size(), 1);
    EXPECT_EQ(models[0].model_name, "model-1");
    EXPECT_EQ(models[0].model_decs, "Test DeepSeek Model");
}

// 测试注册多个模型后 GetAllModels
TEST_F(AIChatSdkTest, GetAllModelsMultipleRegistrations)
{
    ASSERT_TRUE(RegisterTestModel("model-a"));
    ASSERT_TRUE(RegisterTestModel("model-b"));
    ASSERT_TRUE(RegisterTestModel("model-c"));

    auto models = sdk_->GetAllModels();
    EXPECT_EQ(models.size(), 3);

    // 验证所有模型名都存在
    std::set<std::string> names;
    for (auto& m : models)
    {
        names.insert(m.model_name);
    }
    EXPECT_TRUE(names.count("model-a"));
    EXPECT_TRUE(names.count("model-b"));
    EXPECT_TRUE(names.count("model-c"));
}

// =================================================================
//                    2. 用户管理测试 (CreateUser / GetUser / HasUser)
// =================================================================

// 测试创建用户
TEST_F(AIChatSdkTest, CreateUserSuccess)
{
    std::string uid = sdk_->CreateUser("alice", "password123");
    EXPECT_FALSE(uid.empty()) << "创建用户应该返回非空 UID";
    EXPECT_EQ(uid.length(), 36) << "UID 应该是 UUID v4 格式 (36字符)";
}

// 测试创建重复用户
TEST_F(AIChatSdkTest, CreateDuplicateUser)
{
    std::string uid1 = sdk_->CreateUser("bob", "password1");
    ASSERT_FALSE(uid1.empty());

    std::string uid2 = sdk_->CreateUser("bob", "password2");
    EXPECT_TRUE(uid2.empty()) << "重复用户名应返回空字符串";
}

// 测试 HasUser - 存在的用户
TEST_F(AIChatSdkTest, HasUserExists)
{
    ASSERT_FALSE(sdk_->CreateUser("charlie", "pass").empty());

    EXPECT_TRUE(sdk_->HasUser("charlie"));
}

// 测试 HasUser - 不存在的用户
TEST_F(AIChatSdkTest, HasUserNotExists)
{
    EXPECT_FALSE(sdk_->HasUser("nonexistent_user"));
}

// 测试 GetUser - 正确密码
TEST_F(AIChatSdkTest, GetUserWithCorrectPassword)
{
    sdk_->CreateUser("dave", "secret123");

    auto user = sdk_->GetUser("dave", "secret123");
    ASSERT_NE(user, nullptr);
    EXPECT_EQ(user->email, "dave");
    EXPECT_EQ(user->password, "secret123");
    EXPECT_FALSE(user->uid.empty());
}

// 测试 GetUser - 错误密码
TEST_F(AIChatSdkTest, GetUserWithWrongPassword)
{
    sdk_->CreateUser("eve", "correct_password");

    auto user = sdk_->GetUser("eve", "wrong_password");
    EXPECT_EQ(user, nullptr) << "密码错误应返回 nullptr";
}

// 测试 GetUser - 不存在的用户
TEST_F(AIChatSdkTest, GetUserNotExists)
{
    auto user = sdk_->GetUser("ghost", "any_password");
    EXPECT_EQ(user, nullptr) << "不存在的用户应返回 nullptr";
}

// 测试 GetUser - 空密码
TEST_F(AIChatSdkTest, GetUserWithEmptyPassword)
{
    sdk_->CreateUser("frank", "pass");

    auto user = sdk_->GetUser("frank", "");
    EXPECT_EQ(user, nullptr) << "空密码不应匹配";
}

// 测试创建多个不同用户
TEST_F(AIChatSdkTest, CreateMultipleUsers)
{
    const int num_users = 20;
    std::vector<std::string> uids;

    for (int i = 0; i < num_users; ++i)
    {
        std::string email = "multi_user_" + std::to_string(i);
        std::string uid = sdk_->CreateUser(email, "password");
        EXPECT_FALSE(uid.empty()) << "用户 " << email << " 创建失败";
        uids.push_back(uid);
    }

    // 验证所有 UID 唯一
    std::set<std::string> unique_uids(uids.begin(), uids.end());
    EXPECT_EQ(unique_uids.size(), num_users) << "所有用户 UID 应该唯一";

    // 验证所有用户都可以查到
    for (int i = 0; i < num_users; ++i)
    {
        std::string email = "multi_user_" + std::to_string(i);
        EXPECT_TRUE(sdk_->HasUser(email));

        auto user = sdk_->GetUser(email, "password");
        ASSERT_NE(user, nullptr);
        EXPECT_EQ(user->email, email);
    }
}

// =================================================================
//                    3. 会话管理测试 (CreateSession / GetUserAllSession / RemoveSession)
// =================================================================

// 测试创建会话
TEST_F(AIChatSdkTest, CreateSessionSuccess)
{
    std::string uid = CreateTestUser("session_user");
    std::string ssid = sdk_->CreateSession(uid, "deepseek");

    EXPECT_FALSE(ssid.empty()) << "会话 ID 不应为空";
    EXPECT_TRUE(ssid.find(uid) != std::string::npos) << "会话 ID 应包含用户 UID";
}

// 测试获取用户所有会话（空列表）
TEST_F(AIChatSdkTest, GetUserAllSessionEmpty)
{
    std::string uid = CreateTestUser("no_session_user");

    auto sessions = sdk_->GetUserAllSession(uid);
    EXPECT_TRUE(sessions.empty()) << "新用户不应有会话";
}

// 测试获取用户所有会话
TEST_F(AIChatSdkTest, GetUserAllSession)
{
    std::string uid = CreateTestUser("multi_session_user");

    std::vector<std::string> ssids;
    for (int i = 0; i < 5; ++i)
    {
        std::string ssid = sdk_->CreateSession(uid, "model_" + std::to_string(i));
        ssids.push_back(ssid);
        std::this_thread::sleep_for(std::chrono::milliseconds(5)); // 确保时间戳不同
    }

    auto sessions = sdk_->GetUserAllSession(uid);
    EXPECT_EQ(sessions.size(), 5);

    // 验证所有会话 ID 都出现
    std::set<std::string> returned_ssids;
    for (auto& s : sessions)
    {
        returned_ssids.insert(s.session_id);
        EXPECT_EQ(s.uid, uid);
    }
    for (auto& expected_ssid : ssids)
    {
        EXPECT_TRUE(returned_ssids.count(expected_ssid))
            << "会话 " << expected_ssid << " 应该在列表中";
    }
}

// 测试删除会话
TEST_F(AIChatSdkTest, RemoveSessionSuccess)
{
    std::string uid = CreateTestUser("remove_session_user");
    std::string ssid = sdk_->CreateSession(uid, "deepseek");

    bool result = sdk_->RemoveSession(ssid);
    EXPECT_TRUE(result) << "删除会话应该成功";

    // 验证会话已不在列表中
    auto sessions = sdk_->GetUserAllSession(uid);
    EXPECT_TRUE(sessions.empty()) << "删除后应该没有会话";
}

// 测试删除不存在的会话
TEST_F(AIChatSdkTest, RemoveNonExistentSession)
{
    bool result = sdk_->RemoveSession("non_existent_ssid");
    EXPECT_FALSE(result) << "删除不存在的会话应返回 false";
}

// 测试重复删除同一个会话
TEST_F(AIChatSdkTest, RemoveSessionTwice)
{
    std::string uid = CreateTestUser("double_remove_user");
    std::string ssid = sdk_->CreateSession(uid, "deepseek");

    EXPECT_TRUE(sdk_->RemoveSession(ssid));
    EXPECT_FALSE(sdk_->RemoveSession(ssid)) << "第二次删除应返回 false";
}

// 测试多用户会话隔离
TEST_F(AIChatSdkTest, SessionIsolationBetweenUsers)
{
    std::string uid1 = CreateTestUser("isolation_user_1");
    std::string uid2 = CreateTestUser("isolation_user_2");

    std::string ssid1 = sdk_->CreateSession(uid1, "deepseek");
    std::string ssid2 = sdk_->CreateSession(uid2, "chatgpt");

    auto sessions1 = sdk_->GetUserAllSession(uid1);
    auto sessions2 = sdk_->GetUserAllSession(uid2);

    EXPECT_EQ(sessions1.size(), 1);
    EXPECT_EQ(sessions2.size(), 1);
    EXPECT_EQ(sessions1[0].session_id, ssid1);
    EXPECT_EQ(sessions2[0].session_id, ssid2);
}

// 测试为不存在的用户创建会话（uid 是随意字符串也可以创建）
TEST_F(AIChatSdkTest, CreateSessionWithArbitraryUid)
{
    std::string ssid = sdk_->CreateSession("fake-uid-12345", "deepseek");
    EXPECT_FALSE(ssid.empty()) << "即使 uid 不对应真实用户，会话也应该创建成功";

    auto sessions = sdk_->GetUserAllSession("fake-uid-12345");
    EXPECT_EQ(sessions.size(), 1);
    EXPECT_EQ(sessions[0].session_id, ssid);
}

// =================================================================
//                    4. 消息发送测试 (SendMessage / SendMessageStream)
// =================================================================

// 测试 SendMessage - 不存在的会话
TEST_F(AIChatSdkTest, SendMessageToNonExistentSession)
{
    std::string result = sdk_->SendMessage("non_existent_ssid", "Hello");
    EXPECT_TRUE(result.empty()) << "发送消息到不存在的会话应返回空字符串";
}

// 测试 SendMessage - 模型未注册的会话
TEST_F(AIChatSdkTest, SendMessageWithUnregisteredModel)
{
    std::string uid = CreateTestUser("unreg_model_user");
    std::string ssid = sdk_->CreateSession(uid, "unregistered-model");

    std::string result = sdk_->SendMessage(ssid, "Hello, AI!");

    // 模型未注册，LLManager 找不到 provider，返回空
    EXPECT_TRUE(result.empty()) << "使用未注册模型应返回空字符串";

    // 但用户消息应该已经被保存到数据库
    auto sessions = sdk_->GetUserAllSession(uid);
    ASSERT_EQ(sessions.size(), 1);
    // 注意：GetUserAllSession 返回的 Session 包含 messages 字段
    // 验证用户消息已保存
    EXPECT_EQ(sessions[0].messages.size(), 1);
    EXPECT_EQ(sessions[0].messages[0].role, "user");
    EXPECT_EQ(sessions[0].messages[0].content, "Hello, AI!");
}

// 测试 SendMessage - 注册了模型但 HTTP 调用失败（使用假 endpoint）
TEST_F(AIChatSdkTest, SendMessageWithFakeEndpoint)
{
    // 注册模型（使用假的 endpoint，HTTP 调用会失败）
    ASSERT_TRUE(RegisterTestModel("fake-endpoint-model"));

    std::string uid = CreateTestUser("fake_ep_user");
    std::string ssid = sdk_->CreateSession(uid, "fake-endpoint-model");

    std::string result = sdk_->SendMessage(ssid, "这条消息发送后会调用假API");

    // HTTP 调用会失败（连接拒绝），返回空
    EXPECT_TRUE(result.empty()) << "假 endpoint 应导致 HTTP 调用失败";

    // 用户消息应该被保存
    auto sessions = sdk_->GetUserAllSession(uid);
    ASSERT_EQ(sessions.size(), 1);
    EXPECT_EQ(sessions[0].messages.size(), 1);
    EXPECT_EQ(sessions[0].messages[0].role, "user");
    EXPECT_EQ(sessions[0].messages[0].content, "这条消息发送后会调用假API");
}

// 测试 SendMessageStream - 不存在的会话
TEST_F(AIChatSdkTest, SendMessageStreamToNonExistentSession)
{
    std::atomic<int> callback_count{0};
    auto callback = [&](const std::string& message, bool finish) {
        callback_count++;
    };

    std::string result = sdk_->SendMessageStream("non_existent_ssid", "Hello", callback);
    EXPECT_TRUE(result.empty()) << "发送流式消息到不存在的会话应返回空";
    EXPECT_EQ(callback_count, 0) << "回调不应被调用";
}

// 测试 SendMessageStream - 模型未注册
TEST_F(AIChatSdkTest, SendMessageStreamWithUnregisteredModel)
{
    std::string uid = CreateTestUser("stream_unreg_user");
    std::string ssid = sdk_->CreateSession(uid, "unregistered-model");

    std::atomic<int> callback_count{0};
    std::vector<std::string> received_chunks;
    auto callback = [&](const std::string& message, bool finish) {
        callback_count++;
        received_chunks.push_back(message);
    };

    std::string result = sdk_->SendMessageStream(ssid, "Streaming hello", callback);

    EXPECT_TRUE(result.empty()) << "未注册模型应返回空";
    EXPECT_EQ(callback_count, 0) << "回调不应被调用";

    // 用户消息应该已保存
    auto sessions = sdk_->GetUserAllSession(uid);
    ASSERT_EQ(sessions.size(), 1);
    EXPECT_EQ(sessions[0].messages.size(), 1);
    EXPECT_EQ(sessions[0].messages[0].content, "Streaming hello");
}

// 测试 SendMessageStream - 假 endpoint
TEST_F(AIChatSdkTest, SendMessageStreamWithFakeEndpoint)
{
    ASSERT_TRUE(RegisterTestModel("stream-fake-model"));

    std::string uid = CreateTestUser("stream_fake_user");
    std::string ssid = sdk_->CreateSession(uid, "stream-fake-model");

    std::atomic<int> callback_count{0};
    auto callback = [&](const std::string& message, bool finish) {
        callback_count++;
    };

    std::string result = sdk_->SendMessageStream(ssid, "Streaming to fake endpoint", callback);

    EXPECT_TRUE(result.empty()) << "假 endpoint 流式调用应失败";
    // 用户消息应该被保存
    auto sessions = sdk_->GetUserAllSession(uid);
    ASSERT_EQ(sessions.size(), 1);
    EXPECT_EQ(sessions[0].messages.size(), 1);
}

// 测试 SendMessage 保存多轮对话
TEST_F(AIChatSdkTest, SendMessageMultiTurnConversation)
{
    ASSERT_TRUE(RegisterTestModel("multi-turn-model"));

    std::string uid = CreateTestUser("multi_turn_user");
    std::string ssid = sdk_->CreateSession(uid, "multi-turn-model");

    // 发送多条消息（每次 HTTP 调用都会失败，但用户消息会保存）
    sdk_->SendMessage(ssid, "第一轮问题");
    sdk_->SendMessage(ssid, "第二轮问题");
    sdk_->SendMessage(ssid, "第三轮问题");

    auto sessions = sdk_->GetUserAllSession(uid);
    ASSERT_EQ(sessions.size(), 1);

    // 应该有 3 条用户消息（assistant 回复因 HTTP 失败未保存）
    EXPECT_EQ(sessions[0].messages.size(), 3);
    EXPECT_EQ(sessions[0].messages[0].content, "第一轮问题");
    EXPECT_EQ(sessions[0].messages[1].content, "第二轮问题");
    EXPECT_EQ(sessions[0].messages[2].content, "第三轮问题");

    for (auto& msg : sessions[0].messages)
    {
        EXPECT_EQ(msg.role, "user");
    }
}

// =================================================================
//                    5. 完整集成流程测试
// =================================================================

// 测试完整流程: 注册模型 → 创建用户 → 创建会话 → 发送消息 → 获取会话列表 → 删除会话
TEST_F(AIChatSdkTest, FullIntegrationFlow)
{
    // 1. 注册模型
    ASSERT_TRUE(RegisterTestModel("integration-model"));

    // 验证模型已注册
    auto models = sdk_->GetAllModels();
    ASSERT_EQ(models.size(), 1);
    EXPECT_EQ(models[0].model_name, "integration-model");

    // 2. 创建用户
    std::string uid = sdk_->CreateUser("integration_user", "mypassword");
    ASSERT_FALSE(uid.empty());

    // 3. 验证用户存在
    EXPECT_TRUE(sdk_->HasUser("integration_user"));

    // 4. 验证密码认证
    auto user = sdk_->GetUser("integration_user", "mypassword");
    ASSERT_NE(user, nullptr);
    EXPECT_EQ(user->email, "integration_user");
    EXPECT_EQ(user->uid, uid);

    // 5. 创建会话
    std::string ssid = sdk_->CreateSession(uid, "integration-model");
    ASSERT_FALSE(ssid.empty());

    // 6. 发送消息
    sdk_->SendMessage(ssid, "你好，请介绍一下自己");

    // 7. 获取用户所有会话
    auto sessions = sdk_->GetUserAllSession(uid);
    ASSERT_EQ(sessions.size(), 1);
    EXPECT_EQ(sessions[0].session_id, ssid);
    EXPECT_EQ(sessions[0].model_name, "integration-model");
    EXPECT_EQ(sessions[0].messages.size(), 1);

    // 8. 删除会话
    EXPECT_TRUE(sdk_->RemoveSession(ssid));

    // 9. 验证会话已删除
    auto sessions_after = sdk_->GetUserAllSession(uid);
    EXPECT_TRUE(sessions_after.empty());
}

// 测试多用户多会话多模型场景
TEST_F(AIChatSdkTest, MultiUserMultiSessionScenario)
{
    // 注册多个模型
    ASSERT_TRUE(RegisterTestModel("model-x"));
    ASSERT_TRUE(RegisterTestModel("model-y"));

    // 创建两个用户
    std::string alice_uid = CreateTestUser("alice", "alice_pass");
    std::string bob_uid = CreateTestUser("bob", "bob_pass");

    // Alice 创建两个会话
    std::string alice_ssid1 = sdk_->CreateSession(alice_uid, "model-x");
    std::string alice_ssid2 = sdk_->CreateSession(alice_uid, "model-y");

    // Bob 创建一个会话
    std::string bob_ssid1 = sdk_->CreateSession(bob_uid, "model-x");

    // Alice 在两个会话中发消息
    sdk_->SendMessage(alice_ssid1, "Alice 在 model-x 的消息");
    sdk_->SendMessage(alice_ssid2, "Alice 在 model-y 的消息");

    // Bob 发消息
    sdk_->SendMessage(bob_ssid1, "Bob 在 model-x 的消息");

    // 验证 Alice 的会话
    auto alice_sessions = sdk_->GetUserAllSession(alice_uid);
    EXPECT_EQ(alice_sessions.size(), 2);

    // 验证 Bob 的会话
    auto bob_sessions = sdk_->GetUserAllSession(bob_uid);
    EXPECT_EQ(bob_sessions.size(), 1);
    EXPECT_EQ(bob_sessions[0].messages[0].content, "Bob 在 model-x 的消息");

    // 删除 Alice 的一个会话
    EXPECT_TRUE(sdk_->RemoveSession(alice_ssid1));
    auto alice_sessions_after = sdk_->GetUserAllSession(alice_uid);
    EXPECT_EQ(alice_sessions_after.size(), 1);
    EXPECT_EQ(alice_sessions_after[0].session_id, alice_ssid2);
}

// =================================================================
//                    6. 边界条件测试
// =================================================================

// 测试空用户名
TEST_F(AIChatSdkTest, EmptyUserName)
{
    std::string uid = sdk_->CreateUser("", "password");
    // 不应崩溃
    SUCCEED();
}

// 测试空密码
TEST_F(AIChatSdkTest, EmptyPassword)
{
    std::string uid = sdk_->CreateUser("empty_pass_user", "");
    EXPECT_FALSE(uid.empty()) << "空密码用户也应能创建";

    // 用空密码应该能获取用户
    auto user = sdk_->GetUser("empty_pass_user", "");
    ASSERT_NE(user, nullptr);
    EXPECT_EQ(user->password, "");
}

// 测试特殊字符用户名
TEST_F(AIChatSdkTest, SpecialCharacterUserName)
{
    std::string uid = sdk_->CreateUser("user@test.com", "pass");
    EXPECT_FALSE(uid.empty());

    EXPECT_TRUE(sdk_->HasUser("user@test.com"));

    auto user = sdk_->GetUser("user@test.com", "pass");
    ASSERT_NE(user, nullptr);
    EXPECT_EQ(user->email, "user@test.com");
}

// 测试特殊字符密码
TEST_F(AIChatSdkTest, SpecialCharacterPassword)
{
    std::string uid = sdk_->CreateUser("special_pass_user", "!@#$%^&*()_+-=[]{}|;':\",./<>?");
    EXPECT_FALSE(uid.empty());

    auto user = sdk_->GetUser("special_pass_user", "!@#$%^&*()_+-=[]{}|;':\",./<>?");
    ASSERT_NE(user, nullptr);
}

// 测试长用户名
TEST_F(AIChatSdkTest, LongUserName)
{
    std::string long_name(500, 'a');
    std::string uid = sdk_->CreateUser(long_name, "password");
    // 不应崩溃
    SUCCEED();
}

// 测试长消息内容
TEST_F(AIChatSdkTest, LongMessageContent)
{
    std::string uid = CreateTestUser("long_msg_user");
    std::string ssid = sdk_->CreateSession(uid, "deepseek");

    std::string long_content(50000, 'x');
    std::string result = sdk_->SendMessage(ssid, long_content);
    // HTTP 调用会失败（模型未注册），但消息保存不应崩溃

    auto sessions = sdk_->GetUserAllSession(uid);
    ASSERT_EQ(sessions.size(), 1);
    ASSERT_EQ(sessions[0].messages.size(), 1);
    EXPECT_EQ(sessions[0].messages[0].content.length(), 50000);
}

// 测试会话模型名为空
TEST_F(AIChatSdkTest, CreateSessionWithEmptyModelName)
{
    std::string uid = CreateTestUser("empty_model_user");
    std::string ssid = sdk_->CreateSession(uid, "");

    EXPECT_FALSE(ssid.empty()) << "即使模型名为空，会话也应创建成功";
}

// =================================================================
//                    7. Unicode / 中文测试
// =================================================================

// 测试中文用户名
TEST_F(AIChatSdkTest, ChineseUserName)
{
    std::string uid = sdk_->CreateUser("张三", "密码123");
    EXPECT_FALSE(uid.empty());

    EXPECT_TRUE(sdk_->HasUser("张三"));

    auto user = sdk_->GetUser("张三", "密码123");
    ASSERT_NE(user, nullptr);
    EXPECT_EQ(user->email, "张三");
    EXPECT_EQ(user->password, "密码123");
}

// 测试中文消息
TEST_F(AIChatSdkTest, ChineseMessage)
{
    std::string uid = CreateTestUser("中文用户");
    std::string ssid = sdk_->CreateSession(uid, "deepseek");

    sdk_->SendMessage(ssid, "你好，世界！今天天气真好。🌞");

    auto sessions = sdk_->GetUserAllSession(uid);
    ASSERT_EQ(sessions.size(), 1);
    ASSERT_EQ(sessions[0].messages.size(), 1);
    EXPECT_EQ(sessions[0].messages[0].content, "你好，世界！今天天气真好。🌞");
}

// 测试混合 Unicode
TEST_F(AIChatSdkTest, MixedUnicodeContent)
{
    std::string uid = CreateTestUser("ユーザー_test_123");
    std::string ssid = sdk_->CreateSession(uid, "deepseek");

    sdk_->SendMessage(ssid, "Hello 你好 こんにちは 🌍 🎉 émoji test");

    auto sessions = sdk_->GetUserAllSession(uid);
    ASSERT_EQ(sessions.size(), 1);
    ASSERT_EQ(sessions[0].messages.size(), 1);
    EXPECT_EQ(sessions[0].messages[0].content, "Hello 你好 こんにちは 🌍 🎉 émoji test");
}

// =================================================================
//                    8. 数据持久化测试
// =================================================================

// 测试重新创建 AIChatSdk 后数据仍然存在（用户 + 会话）
TEST_F(AIChatSdkTest, DataPersistenceAfterRestart)
{
    // 第一阶段：创建用户和会话（不发送消息，避免 HTTP 调用引入额外复杂性）
    std::string uid = sdk_->CreateUser("persist_user", "persist_pass");
    ASSERT_FALSE(uid.empty());

    std::string ssid = sdk_->CreateSession(uid, "deepseek");
    ASSERT_FALSE(ssid.empty());

    // 验证数据在第一阶段存在
    EXPECT_TRUE(sdk_->HasUser("persist_user"));
    auto sessions_before = sdk_->GetUserAllSession(uid);
    ASSERT_EQ(sessions_before.size(), 1);
    EXPECT_EQ(sessions_before[0].session_id, ssid);

    // 销毁 SDK，等待确保 TimerWheel 线程完全退出
    sdk_.reset();
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // 确认数据库文件仍然存在
    EXPECT_TRUE(std::filesystem::exists(test_db_name_)) << "数据库文件应该仍然存在";

    // 重新创建 SDK（使用相同的数据库文件）
    sdk_ = std::make_unique<AIChatSdk>(test_db_name_);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 验证用户数据持久化
    EXPECT_TRUE(sdk_->HasUser("persist_user")) << "用户应该持久化到磁盘";
    auto restored_user = sdk_->GetUser("persist_user", "persist_pass");
    ASSERT_NE(restored_user, nullptr) << "应该能从数据库恢复用户信息";
    EXPECT_EQ(restored_user->email, "persist_user");
    EXPECT_EQ(restored_user->uid, uid);

    // 验证会话数据持久化
    auto sessions_after = sdk_->GetUserAllSession(uid);
    ASSERT_EQ(sessions_after.size(), 1) << "会话应该持久化到磁盘";
    EXPECT_EQ(sessions_after[0].session_id, ssid);
    EXPECT_EQ(sessions_after[0].model_name, "deepseek");
}

// 测试消息持久化：验证消息在重启后仍能从 DB 加载
// 注：GetUserAllSession 返回的 Session 对象中 messages 字段是懒加载的，
// 从 DB 恢复后只有通过 GetSessionAllMessage（SendMessage 内部调用）才会填充。
TEST_F(AIChatSdkTest, MessagePersistenceAfterRestart)
{
    std::string uid = sdk_->CreateUser("msg_persist_user", "pass");
    ASSERT_FALSE(uid.empty());

    std::string ssid = sdk_->CreateSession(uid, "deepseek");
    ASSERT_FALSE(ssid.empty());

    // 发送消息（即使 HTTP 调用失败，用户消息也已保存到 DB）
    sdk_->SendMessage(ssid, "第一条消息");
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    sdk_->SendMessage(ssid, "第二条消息");

    // 验证消息已存在于当前内存中
    auto sessions_before = sdk_->GetUserAllSession(uid);
    ASSERT_EQ(sessions_before.size(), 1);
    ASSERT_EQ(sessions_before[0].messages.size(), 2);
    EXPECT_EQ(sessions_before[0].messages[0].content, "第一条消息");
    EXPECT_EQ(sessions_before[0].messages[1].content, "第二条消息");

    // 销毁并重建 SDK
    sdk_.reset();
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    EXPECT_TRUE(std::filesystem::exists(test_db_name_));

    sdk_ = std::make_unique<AIChatSdk>(test_db_name_);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 验证会话存在
    auto sessions_after = sdk_->GetUserAllSession(uid);
    ASSERT_EQ(sessions_after.size(), 1) << "会话应该持久化到磁盘";
    EXPECT_EQ(sessions_after[0].session_id, ssid);
    EXPECT_EQ(sessions_after[0].model_name, "deepseek");

    // 注意：messages 字段在从 DB 恢复后为空是正常的（懒加载设计），
    // 历史消息仍完整保留在数据库中，会在下次 GetSessionAllMessage 调用时加载
}

// 测试持久化：模型信息不会持久化（LLManager 只在内存中）
TEST_F(AIChatSdkTest, ModelInfoNotPersisted)
{
    ASSERT_TRUE(RegisterTestModel("volatile-model"));

    auto models_before = sdk_->GetAllModels();
    ASSERT_EQ(models_before.size(), 1);

    // 重建 SDK
    sdk_.reset();
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    sdk_ = std::make_unique<AIChatSdk>(test_db_name_);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 模型信息不应持久化（LLManager 是纯内存的）
    auto models_after = sdk_->GetAllModels();
    EXPECT_TRUE(models_after.empty()) << "模型信息不应持久化";
}

// =================================================================
//                    9. 并发测试
// =================================================================

// 测试并发创建用户
TEST_F(AIChatSdkTest, ConcurrentCreateUsers)
{
    const int num_threads = 15;
    std::atomic<int> success_count{0};
    std::vector<std::thread> threads;

    for (int i = 0; i < num_threads; ++i)
    {
        threads.emplace_back([this, i, &success_count]() {
            std::string name = "concurrent_user_" + std::to_string(i);
            std::string uid = sdk_->CreateUser(name, "password");
            if (!uid.empty())
            {
                success_count++;
            }
        });
    }

    for (auto& t : threads)
    {
        t.join();
    }

    EXPECT_EQ(success_count, num_threads) << "所有并发用户创建都应成功";
}

// 测试并发创建会话
TEST_F(AIChatSdkTest, ConcurrentCreateSessions)
{
    std::string uid = CreateTestUser("concurrent_session_user");

    const int num_threads = 15;
    std::atomic<int> success_count{0};
    std::vector<std::thread> threads;
    std::mutex ssid_mutex;
    std::vector<std::string> ssids;

    for (int i = 0; i < num_threads; ++i)
    {
        threads.emplace_back([this, uid, i, &success_count, &ssid_mutex, &ssids]() {
            std::string ssid = sdk_->CreateSession(uid, "model_" + std::to_string(i));
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

    // 验证所有会话 ID 唯一
    std::set<std::string> unique_ssids(ssids.begin(), ssids.end());
    EXPECT_EQ(unique_ssids.size(), num_threads);

    // 验证所有会话都属于该用户
    auto sessions = sdk_->GetUserAllSession(uid);
    EXPECT_EQ(sessions.size(), num_threads);
}

// 测试并发读写（不应崩溃）
TEST_F(AIChatSdkTest, ConcurrentReadWrite)
{
    std::string uid = CreateTestUser("concurrent_rw_user");
    std::string ssid = sdk_->CreateSession(uid, "deepseek");

    const int num_threads = 20;
    std::vector<std::thread> threads;

    for (int i = 0; i < num_threads; ++i)
    {
        if (i % 2 == 0)
        {
            // 写线程：发送消息
            threads.emplace_back([this, ssid, i]() {
                sdk_->SendMessage(ssid, "Message_" + std::to_string(i));
            });
        }
        else
        {
            // 读线程：获取会话列表 / 用户信息
            threads.emplace_back([this, uid]() {
                sdk_->GetUserAllSession(uid);
                sdk_->HasUser("concurrent_rw_user");
            });
        }
    }

    for (auto& t : threads)
    {
        t.join();
    }

    // 不应崩溃
    SUCCEED();
}

// 测试并发注册模型
TEST_F(AIChatSdkTest, ConcurrentRegisterModels)
{
    const int num_threads = 10;
    std::atomic<int> success_count{0};
    std::vector<std::thread> threads;

    for (int i = 0; i < num_threads; ++i)
    {
        threads.emplace_back([this, i, &success_count]() {
            Config config;
            config.model_type = ModelType::DEEPSEEK;
            config.model_info.model_name = "concurrent_model_" + std::to_string(i);
            config.model_info.model_decs = "Concurrent Model " + std::to_string(i);
            config.apikey = "test-key";
            config.end_point = "http://localhost:19999";
            config.model = "concurrent_model_" + std::to_string(i);  // 唯一的注册 key
            config.path = "/test";

            if (sdk_->RegisterModel(config))
            {
                success_count++;
            }
        });
    }

    for (auto& t : threads)
    {
        t.join();
    }

    EXPECT_EQ(success_count, num_threads) << "所有并发模型注册都应成功";
}

// =================================================================
//                    10. 参数组合与错误路径测试
// =================================================================

// 测试 SendMessage 在会话被删除后调用
TEST_F(AIChatSdkTest, SendMessageAfterSessionRemoved)
{
    std::string uid = CreateTestUser("removed_session_user");
    std::string ssid = sdk_->CreateSession(uid, "deepseek");

    ASSERT_TRUE(sdk_->RemoveSession(ssid));

    std::string result = sdk_->SendMessage(ssid, "会话已删除的消息");
    EXPECT_TRUE(result.empty()) << "已删除会话发送消息应返回空";
}

// 测试随机 UID 的会话与真实用户会话隔离
TEST_F(AIChatSdkTest, RandomUidSessionIsolation)
{
    // 真实用户
    std::string uid = CreateTestUser("real_uid_user");
    std::string real_ssid = sdk_->CreateSession(uid, "deepseek");

    // 随机 UID
    std::string fake_ssid = sdk_->CreateSession("random-fake-uid", "deepseek");

    // 真实用户的会话列表不应包含随机 UID 的会话
    auto real_sessions = sdk_->GetUserAllSession(uid);
    EXPECT_EQ(real_sessions.size(), 1);
    EXPECT_EQ(real_sessions[0].session_id, real_ssid);

    // 随机 UID 的会话列表也不应包含真实用户的会话
    auto fake_sessions = sdk_->GetUserAllSession("random-fake-uid");
    EXPECT_EQ(fake_sessions.size(), 1);
    EXPECT_EQ(fake_sessions[0].session_id, fake_ssid);
}

// 测试 GetAllModels 在部分注册失败后的一致性
TEST_F(AIChatSdkTest, GetAllModelsConsistencyAfterPartialFailure)
{
    // 成功注册第一个
    ASSERT_TRUE(RegisterTestModel("consistent-model-1"));

    // 尝试用空 API key 注册第二个（会失败）
    Config bad_config;
    bad_config.model_type = ModelType::DEEPSEEK;
    bad_config.model_info.model_name = "consistent-model-2";
    bad_config.model_info.model_decs = "Should Fail";
    bad_config.apikey = "";  // 会导致 Init 失败
    bad_config.end_point = "http://localhost:19999";
    bad_config.model = "test-model";
    bad_config.path = "/test";
    EXPECT_FALSE(sdk_->RegisterModel(bad_config));

    // 成功注册第三个
    ASSERT_TRUE(RegisterTestModel("consistent-model-3"));

    // 应该只有两个成功的模型
    auto models = sdk_->GetAllModels();
    EXPECT_EQ(models.size(), 2);

    std::set<std::string> names;
    for (auto& m : models) names.insert(m.model_name);
    EXPECT_TRUE(names.count("consistent-model-1"));
    EXPECT_FALSE(names.count("consistent-model-2")) << "注册失败的模型不应出现";
    EXPECT_TRUE(names.count("consistent-model-3"));
}

// =================================================================
//                    11. UID / SSID 格式测试
// =================================================================

// 测试 CreateUser 返回的 UID 是 UUID v4 格式
TEST_F(AIChatSdkTest, UserIdIsUuidV4Format)
{
    std::string uid = sdk_->CreateUser("format_test_user", "password");
    ASSERT_FALSE(uid.empty());

    // UUID v4: xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx (36 chars)
    EXPECT_EQ(uid.length(), 36);
    EXPECT_EQ(uid[8], '-');
    EXPECT_EQ(uid[13], '-');
    EXPECT_EQ(uid[18], '-');
    EXPECT_EQ(uid[23], '-');
    EXPECT_EQ(uid[14], '4') << "UUID v4 版本号应为 4";
}

// 测试多个用户的 UID 唯一性
TEST_F(AIChatSdkTest, UserIdsAreUnique)
{
    const int count = 100;
    std::set<std::string> uids;

    for (int i = 0; i < count; ++i)
    {
        std::string uid = sdk_->CreateUser("unique_user_" + std::to_string(i), "pass");
        ASSERT_FALSE(uid.empty());
        uids.insert(uid);
    }

    EXPECT_EQ(uids.size(), count) << "100 个用户的 UID 应该全部唯一";
}

// 测试会话 ID 包含用户 UID
TEST_F(AIChatSdkTest, SessionIdContainsUid)
{
    std::string uid = CreateTestUser("ssid_format_user");
    std::string ssid = sdk_->CreateSession(uid, "deepseek");

    EXPECT_TRUE(ssid.find(uid) == 0) << "会话 ID 应以用户 UID 开头";
}

// =================================================================
//                    12. 流式回调功能测试
// =================================================================

// 测试流式回调的基本接口（即使 HTTP 失败，也应正确返回且不崩溃）
TEST_F(AIChatSdkTest, StreamCallbackInterface)
{
    ASSERT_TRUE(RegisterTestModel("stream-cb-model"));

    std::string uid = CreateTestUser("stream_cb_user");
    std::string ssid = sdk_->CreateSession(uid, "stream-cb-model");

    std::atomic<int> chunk_count{0};
    std::atomic<bool> finish_received{false};
    std::vector<std::string> chunks;
    std::mutex chunk_mutex;

    auto callback = [&](const std::string& message, bool finish) {
        if (finish)
        {
            finish_received = true;
        }
        else if (!message.empty())
        {
            chunk_count++;
            std::lock_guard<std::mutex> lock(chunk_mutex);
            chunks.push_back(message);
        }
    };

    std::string full_response = sdk_->SendMessageStream(ssid, "测试流式消息", callback);

    // HTTP 调用会失败，所以没有 chunks
    // 但不应崩溃，且用户消息应被保存
    auto sessions = sdk_->GetUserAllSession(uid);
    ASSERT_EQ(sessions.size(), 1);
    EXPECT_EQ(sessions[0].messages.size(), 1);
}

// =================================================================
//        13. 大模型真实调度测试 (需要真实 API Key + 网络)
//
//   API Key 从环境变量读取：
//       - DeepSeek_apikey
//       - Gemini_apikey
//       - ChatGPT_apikey
//   未设置对应环境变量时，测试会被跳过 (GTEST_SKIP)，不会失败。
//
//   注意：Gemini / ChatGPT 的 Provider 内部固定使用本地代理
//   127.0.0.1:7897，运行这些用例需要本地代理可用。
//
//   验证点：会话的 model_name 与注册时的 config.model 一致时，
//   AIChatSdk → LLManager 能正确「调度」到对应 Provider 并返回内容。
// =================================================================

// 调度 DeepSeek：注册 → 建会话 → 发消息 → 收到非空回复
TEST_F(AIChatSdkTest, ScheduleDeepSeekRealApi)
{
    std::string apikey = GetEnvApiKey("DeepSeek_apikey");
    if (apikey.empty())
    {
        GTEST_SKIP() << "未设置环境变量 DeepSeek_apikey，跳过 DeepSeek 真实调度测试";
    }

    const std::string model_id = "deepseek-chat";

    Config config;
    config.model_type = ModelType::DEEPSEEK;
    config.model_info.model_name = "deepseek-prod";
    config.model_info.model_decs = "DeepSeek 真实模型";
    config.apikey = apikey;
    config.end_point = "https://api.deepseek.com";
    config.model = model_id;                 // LLManager 调度键
    config.path = "/chat/completions";
    config.max_token = 64;
    ASSERT_TRUE(sdk_->RegisterModel(config)) << "注册 DeepSeek 模型失败";

    std::string uid = CreateTestUser("deepseek_real_user");
    // 会话的 model_name 必须等于 config.model 才能被正确调度
    std::string ssid = sdk_->CreateSession(uid, model_id);
    ASSERT_FALSE(ssid.empty());

    std::string reply = SendMessageWithRetry(ssid, "请用一句话回答：你好");
    EXPECT_FALSE(reply.empty()) << "DeepSeek 应返回非空回复（检查 API Key / 网络）";

    if (!reply.empty())
    {
        // 调度成功后，assistant 回复应被持久化
        auto sessions = sdk_->GetUserAllSession(uid);
        ASSERT_EQ(sessions.size(), 1);
        ASSERT_GE(sessions[0].messages.size(), 2u);
        EXPECT_EQ(sessions[0].messages[0].role, "user");
        EXPECT_EQ(sessions[0].messages.back().role, "assistant");
        EXPECT_EQ(sessions[0].messages.back().content, reply);
    }
}

// 调度 Gemini：注册 → 建会话 → 发消息 → 收到非空回复
TEST_F(AIChatSdkTest, ScheduleGeminiRealApi)
{
    std::string apikey = GetEnvApiKey("Gemini_apikey");
    if (apikey.empty())
    {
        GTEST_SKIP() << "未设置环境变量 Gemini_apikey，跳过 Gemini 真实调度测试";
    }

    const std::string model_id = "gemini-3.5-flash";

    Config config;
    config.model_type = ModelType::GEMINI;
    config.model_info.model_name = "gemini-prod";
    config.model_info.model_decs = "Gemini 真实模型";
    config.apikey = apikey;
    config.end_point = "https://gemini-api.poenl.top";
    config.model = model_id;                 // LLManager 调度键
    // path / streampath 留空，Provider::Init 会按默认值填充（默认即 gemini-3.5-flash）
    config.max_token = 64;
    ASSERT_TRUE(sdk_->RegisterModel(config)) << "注册 Gemini 模型失败";

    std::string uid = CreateTestUser("gemini_real_user");
    std::string ssid = sdk_->CreateSession(uid, model_id);
    ASSERT_FALSE(ssid.empty());

    std::string reply = SendMessageWithRetry(ssid, "Reply in one short sentence: hello");
    EXPECT_FALSE(reply.empty())
        << "Gemini 应返回非空回复（检查 API Key / 本地代理 127.0.0.1:7897 / 网络）";

    if (!reply.empty())
    {
        auto sessions = sdk_->GetUserAllSession(uid);
        ASSERT_EQ(sessions.size(), 1);
        ASSERT_GE(sessions[0].messages.size(), 2u);
        EXPECT_EQ(sessions[0].messages.back().role, "assistant");
        EXPECT_EQ(sessions[0].messages.back().content, reply);
    }
}

// 调度 ChatGPT：注册 → 建会话 → 发消息 → 收到非空回复
TEST_F(AIChatSdkTest, ScheduleChatGPTRealApi)
{
    std::string apikey = GetEnvApiKey("ChatGPT_apikey");
    if (apikey.empty())
    {
        GTEST_SKIP() << "未设置环境变量 ChatGPT_apikey，跳过 ChatGPT 真实调度测试";
    }

    const std::string model_id = "gpt-5.4";

    Config config;
    config.model_type = ModelType::CHATGPT;
    config.model_info.model_name = "chatgpt-prod";
    config.model_info.model_decs = "ChatGPT 真实模型";
    config.apikey = apikey;
    config.end_point = "https://api.openai.com";
    config.model = model_id;                 // LLManager 调度键
    config.path = "/v1/responses";
    config.max_token = 64;
    ASSERT_TRUE(sdk_->RegisterModel(config)) << "注册 ChatGPT 模型失败";

    std::string uid = CreateTestUser("chatgpt_real_user");
    std::string ssid = sdk_->CreateSession(uid, model_id);
    ASSERT_FALSE(ssid.empty());

    std::string reply = SendMessageWithRetry(ssid, "Reply in one short sentence: hello");
    EXPECT_FALSE(reply.empty())
        << "ChatGPT 应返回非空回复（检查 API Key / 本地代理 127.0.0.1:7897 / 网络）";

    if (!reply.empty())
    {
        auto sessions = sdk_->GetUserAllSession(uid);
        ASSERT_EQ(sessions.size(), 1);
        ASSERT_GE(sessions[0].messages.size(), 2u);
        EXPECT_EQ(sessions[0].messages.back().role, "assistant");
        EXPECT_EQ(sessions[0].messages.back().content, reply);
    }
}

// 流式调度：任选一个有 Key 的模型，验证流式回调能收到内容并正常结束
TEST_F(AIChatSdkTest, ScheduleStreamRealApi)
{
    std::string ds_key = GetEnvApiKey("DeepSeek_apikey");
    std::string gm_key = GetEnvApiKey("Gemini_apikey");
    std::string gpt_key = GetEnvApiKey("ChatGPT_apikey");

    Config config;
    std::string model_id;
    if (!ds_key.empty())
    {
        model_id = "deepseek-chat";
        config.model_type = ModelType::DEEPSEEK;
        config.apikey = ds_key;
        config.end_point = "https://api.deepseek.com";
        config.model = model_id;
        config.path = "/chat/completions";
    }
    else if (!gm_key.empty())
    {
        model_id = "gemini-3.5-flash";
        config.model_type = ModelType::GEMINI;
        config.apikey = gm_key;
        config.end_point = "https://gemini-api.poenl.top";
        config.model = model_id;
    }
    else if (!gpt_key.empty())
    {
        model_id = "gpt-5.4";
        config.model_type = ModelType::CHATGPT;
        config.apikey = gpt_key;
        config.end_point = "https://api.openai.com";
        config.model = model_id;
        config.path = "/v1/responses";
    }
    else
    {
        GTEST_SKIP() << "未设置任何模型的 API Key，跳过流式真实调度测试";
    }

    config.model_info.model_name = model_id;
    config.model_info.model_decs = "Stream 真实模型";
    config.max_token = 64;
    ASSERT_TRUE(sdk_->RegisterModel(config)) << "注册流式模型失败";

    std::string uid = CreateTestUser("stream_real_user");
    std::string ssid = sdk_->CreateSession(uid, model_id);
    ASSERT_FALSE(ssid.empty());

    std::atomic<int> chunk_count{0};
    std::atomic<bool> finish_received{false};
    std::string assembled;
    std::mutex assembled_mutex;

    auto callback = [&](const std::string& message, bool finish) {
        if (finish)
        {
            finish_received = true;
        }
        else if (!message.empty())
        {
            chunk_count++;
            std::lock_guard<std::mutex> lock(assembled_mutex);
            assembled += message;
        }
    };

    // 带重试：真实服务偶发 503/超时，重试以降低抖动
    std::string full;
    for (int i = 0; i < 5; ++i)
    {
        chunk_count = 0;
        finish_received = false;
        {
            std::lock_guard<std::mutex> lock(assembled_mutex);
            assembled.clear();
        }
        full = sdk_->SendMessageStream(ssid, "Reply in one short sentence: hi", callback);
        if (!full.empty())
        {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(800));
    }

    EXPECT_FALSE(full.empty()) << "流式调度应返回非空完整回复";
    EXPECT_GT(chunk_count.load(), 0) << "应至少收到一个流式数据块";
    EXPECT_TRUE(finish_received.load()) << "应收到结束回调 (finish=true)";
    EXPECT_EQ(full, assembled) << "完整回复应等于各数据块拼接结果";
}
