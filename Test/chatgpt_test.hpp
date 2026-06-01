
#pragma once

#include <gtest/gtest.h>
#include "provider/chatgpt_provider.h"
#include "base/util/mylog.h"
#include <cstdlib>

class ChatGPTProviderTest : public ::testing::Test
{
protected:
    ai_sdk::ChatGPTProvider provider_;
    ai_sdk::Config config_;

    static void SetUpTestSuite()
    {
        ai_sdk::Logger::initLogger("test", "stdout", spdlog::level::debug);
    }

    void SetUp() override
    {
        const char* apikey = std::getenv("ChatGPT_apikey");
        ASSERT_NE(apikey, nullptr) << "环境变量 ChatGPT_apikey 未设置";
        config_.apikey = apikey;
        config_.model_info.model_name = "gpt-5.4";
        config_.model_info.model_decs = "ChatGPT Model";
    }
};

TEST_F(ChatGPTProviderTest, InitFailsWithEmptyApikey)
{
    ai_sdk::Config empty_config;
    ai_sdk::ChatGPTProvider p;
    EXPECT_FALSE(p.Init(empty_config));
}

TEST_F(ChatGPTProviderTest, InitSuccess)
{
    EXPECT_TRUE(provider_.Init(config_));
    EXPECT_TRUE(provider_.IsAvaiable());
}

TEST_F(ChatGPTProviderTest, InitFillsDefaults)
{
    EXPECT_TRUE(provider_.Init(config_));
    EXPECT_EQ(provider_.ModelName(), "gpt-5.4");
    EXPECT_EQ(provider_.ModelDesc(), "ChatGPT Model");
}

TEST_F(ChatGPTProviderTest, NotAvailableBeforeInit)
{
    ai_sdk::ChatGPTProvider p;
    EXPECT_FALSE(p.IsAvaiable());
}

TEST_F(ChatGPTProviderTest, SendMessageReturnsResponse)
{
    ASSERT_TRUE(provider_.Init(config_));

    std::vector<ai_sdk::Message> messages = {
        {"", "", "user", "Reply with one word: hello", 0}
    };

    std::string response = provider_.SendMessage(messages);
    EXPECT_FALSE(response.empty());
}

TEST_F(ChatGPTProviderTest, SendMessageStreamReturnsResponse)
{
    ASSERT_TRUE(provider_.Init(config_));

    std::vector<ai_sdk::Message> messages = {
        {"", "", "user", "Reply with one word: hello", 0}
    };

    std::string streamed_content;
    bool got_finish = false;

    auto callback = [&](const std::string& msg, bool finish) {
        if (finish) {
            got_finish = true;
        } else {
            streamed_content += msg;
        }
    };

    std::string full = provider_.SendMessageStream(messages, callback);
    EXPECT_FALSE(full.empty());
    EXPECT_EQ(full, streamed_content);
    EXPECT_TRUE(got_finish);
}
