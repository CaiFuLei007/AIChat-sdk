
#pragma once

#include <gtest/gtest.h>
#include "llmmanager.h"
#include "base/util/mylog.h"

class MockProvider : public aichat_sdk::Provider
{
public:
    bool Init(aichat_sdk::Config config) override
    {
        if (config.apikey.empty()) return false;
        config_ = std::move(config);
        is_avaiable_ = true;
        return true;
    }

    std::string SendMessage(const std::vector<aichat_sdk::Message>& messages) override
    {
        if (messages.empty()) return "";
        return "mock_reply:" + messages.back().content;
    }

    std::string SendMessageStream(const std::vector<aichat_sdk::Message>& messages, MessageCallback cb) override
    {
        if (messages.empty()) return "";
        std::string full = "stream:" + messages.back().content;
        cb(full, false);
        cb("", true);
        return full;
    }
};

class LLManagerTest : public ::testing::Test
{
protected:
    aichat_sdk::LLManager manager_;

    static void SetUpTestSuite()
    {
        aichat_sdk::Logger::initLogger("test", "stdout", spdlog::level::debug);
    }

    aichat_sdk::Config MakeConfig(const std::string& name)
    {
        aichat_sdk::Config cfg;
        cfg.apikey = "test-key";
        cfg.model_info.model_name = name;
        cfg.model_info.model_decs = name + " desc";
        return cfg;
    }
};

TEST_F(LLManagerTest, RegisterAndInitProvider)
{
    auto provider = std::make_unique<MockProvider>();
    EXPECT_TRUE(manager_.RegisterProvider("mock", std::move(provider)));
    EXPECT_TRUE(manager_.InitProvider("mock", MakeConfig("mock")));
}

TEST_F(LLManagerTest, RegisterDuplicateFails)
{
    manager_.RegisterProvider("mock", std::make_unique<MockProvider>());
    EXPECT_FALSE(manager_.RegisterProvider("mock", std::make_unique<MockProvider>()));
}

TEST_F(LLManagerTest, InitUnregisteredFails)
{
    EXPECT_FALSE(manager_.InitProvider("unknown", MakeConfig("unknown")));
}

TEST_F(LLManagerTest, InitWithEmptyApikeyFails)
{
    manager_.RegisterProvider("mock", std::make_unique<MockProvider>());
    aichat_sdk::Config cfg;
    EXPECT_FALSE(manager_.InitProvider("mock", cfg));
}

TEST_F(LLManagerTest, SendMessageToRegisteredProvider)
{
    manager_.RegisterProvider("mock", std::make_unique<MockProvider>());
    manager_.InitProvider("mock", MakeConfig("mock"));

    std::vector<aichat_sdk::Message> msgs = {{"", "", "user", "hello", 0}};
    std::string reply = manager_.SendMessage("mock", msgs);
    EXPECT_EQ(reply, "mock_reply:hello");
}

TEST_F(LLManagerTest, SendMessageToUnregisteredReturnsEmpty)
{
    std::vector<aichat_sdk::Message> msgs = {{"", "", "user", "hello", 0}};
    EXPECT_TRUE(manager_.SendMessage("unknown", msgs).empty());
}

TEST_F(LLManagerTest, SendMessageStreamToRegisteredProvider)
{
    manager_.RegisterProvider("mock", std::make_unique<MockProvider>());
    manager_.InitProvider("mock", MakeConfig("mock"));

    std::vector<aichat_sdk::Message> msgs = {{"", "", "user", "hello", 0}};
    std::string streamed;
    bool got_finish = false;

    auto cb = [&](const std::string& data, bool finish) {
        if (finish) got_finish = true;
        else streamed += data;
    };

    std::string full = manager_.SendMessageStream("mock", msgs, cb);
    EXPECT_EQ(full, "stream:hello");
    EXPECT_EQ(full, streamed);
    EXPECT_TRUE(got_finish);
}

TEST_F(LLManagerTest, SendMessageStreamToUnregisteredReturnsEmpty)
{
    std::vector<aichat_sdk::Message> msgs = {{"", "", "user", "hello", 0}};
    std::string result = manager_.SendMessageStream("unknown", msgs, [](const std::string&, bool){});
    EXPECT_TRUE(result.empty());
}

TEST_F(LLManagerTest, GetAllModelReturnsRegisteredModels)
{
    manager_.RegisterProvider("a", std::make_unique<MockProvider>());
    manager_.RegisterProvider("b", std::make_unique<MockProvider>());
    manager_.InitProvider("a", MakeConfig("model_a"));
    manager_.InitProvider("b", MakeConfig("model_b"));

    auto models = manager_.GetAllModel();
    EXPECT_EQ(models.size(), 2);

    std::vector<std::string> names;
    for (auto& m : models) names.push_back(m.model_name);
    std::sort(names.begin(), names.end());
    EXPECT_EQ(names[0], "model_a");
    EXPECT_EQ(names[1], "model_b");
}

TEST_F(LLManagerTest, GetAllModelEmptyBeforeInit)
{
    manager_.RegisterProvider("mock", std::make_unique<MockProvider>());
    EXPECT_TRUE(manager_.GetAllModel().empty());
}
