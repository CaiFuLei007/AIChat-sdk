
#pragma once

#include <gtest/gtest.h>
#include "provider/gemini_provider.h"
#include "base/util/mylog.h"
#include <cstdlib>

class GeminiProviderTest : public ::testing::Test
{
protected:
    aichat_sdk::GeminiProvider provider_;
    aichat_sdk::Config config_;

    static void SetUpTestSuite()
    {
        aichat_sdk::Logger::initLogger("test", "stdout", spdlog::level::debug);
    }

    void SetUp() override
    {
        const char* apikey = std::getenv("Gemini_apikey");
        if (apikey)
        {
            config_.apikey = apikey;
        }
        config_.model_info.model_name = "gemini-3.5-flash";
        config_.model_info.model_decs = "Gemini Flash Model";
        config_.end_point = "https://gemini-api.poenl.top";
    }
};

TEST_F(GeminiProviderTest, InitFailsWithEmptyApikey)
{
    aichat_sdk::Config empty_config;
    aichat_sdk::GeminiProvider p;
    EXPECT_FALSE(p.Init(empty_config));
}

TEST_F(GeminiProviderTest, InitSuccess)
{
    ASSERT_FALSE(config_.apikey.empty()) << "环境变量 Gemini_apikey 未设置";
    EXPECT_TRUE(provider_.Init(config_));
    EXPECT_TRUE(provider_.IsAvaiable());
}

TEST_F(GeminiProviderTest, InitFillsDefaults)
{
    ASSERT_FALSE(config_.apikey.empty()) << "环境变量 Gemini_apikey 未设置";
    EXPECT_TRUE(provider_.Init(config_));
    EXPECT_EQ(provider_.ModelName(), "gemini-3.5-flash");
    EXPECT_EQ(provider_.ModelDesc(), "Gemini Flash Model");
}

TEST_F(GeminiProviderTest, NotAvailableBeforeInit)
{
    aichat_sdk::GeminiProvider p;
    EXPECT_FALSE(p.IsAvaiable());
}

TEST_F(GeminiProviderTest, SendMessageReturnsResponse)
{
    ASSERT_FALSE(config_.apikey.empty()) << "环境变量 Gemini_apikey 未设置";
    ASSERT_TRUE(provider_.Init(config_));

    std::vector<aichat_sdk::Message> messages = {
        {"", "", "user", "Reply with one word: hello", 0}
    };

    std::string response = provider_.SendMessage(messages);
    EXPECT_FALSE(response.empty());
}

TEST_F(GeminiProviderTest, SendMessageStreamReturnsResponse)
{
    ASSERT_FALSE(config_.apikey.empty()) << "环境变量 Gemini_apikey 未设置";
    ASSERT_TRUE(provider_.Init(config_));

    std::vector<aichat_sdk::Message> messages = {
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
    std::cout << streamed_content << '\n';
    EXPECT_FALSE(full.empty());
    EXPECT_EQ(full, streamed_content);
    EXPECT_TRUE(got_finish);
}
