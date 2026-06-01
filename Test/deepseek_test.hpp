
#pragma once

#include <gtest/gtest.h>
#include "provider/deepseek_provider.h"
#include "base/util/mylog.h"
#include <cstdlib>

class DeepSeekProviderTest : public ::testing::Test
{
protected:
    ai_sdk::DeepSeekProvider provider_;
    ai_sdk::Config config_;

    static void SetUpTestSuite()
    {
        ai_sdk::Logger::initLogger("test", "stdout", spdlog::level::debug);
    }

    void SetUp() override
    {
        const char* apikey = std::getenv("DeepSeek_apikey");
        ASSERT_NE(apikey, nullptr) << "环境变量 DeepSeek_apikey 未设置";
        config_.apikey = apikey;
        config_.model_info.model_name = "deepseek-v4-flash";
        config_.model_info.model_decs = "DeepSeek V4 Flash Model";
    }
};

// Init 测试: apikey为空时返回false
TEST_F(DeepSeekProviderTest, InitFailsWithEmptyApikey)
{
    ai_sdk::Config empty_config;
    ai_sdk::DeepSeekProvider p;
    EXPECT_FALSE(p.Init(empty_config));
}

// Init 测试: 正常初始化
TEST_F(DeepSeekProviderTest, InitSuccess)
{
    EXPECT_TRUE(provider_.Init(config_));
    EXPECT_TRUE(provider_.IsAvaiable());
}

// Init 测试: 默认值填充
TEST_F(DeepSeekProviderTest, InitFillsDefaults)
{
    EXPECT_TRUE(provider_.Init(config_));
    EXPECT_EQ(provider_.ModelName(), "deepseek-v4-flash");
    EXPECT_EQ(provider_.ModelDesc(), "DeepSeek V4 Flash Model");
}

// SendMessage 测试: 全量返回
TEST_F(DeepSeekProviderTest, SendMessageReturnsResponse)
{
    ASSERT_TRUE(provider_.Init(config_));

    std::vector<ai_sdk::Message> messages = {
        {"", "", "user", "回复一个字:好", 0}
    };

    std::string response = provider_.SendMessage(messages);
    EXPECT_FALSE(response.empty());
}

// SendMessageStream 测试: 流式返回
TEST_F(DeepSeekProviderTest, SendMessageStreamReturnsResponse)
{
    ASSERT_TRUE(provider_.Init(config_));

    std::vector<ai_sdk::Message> messages = {
        {"", "", "user", "回复一个字:好", 0}
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

// 未初始化时IsAvaiable返回false
TEST_F(DeepSeekProviderTest, NotAvailableBeforeInit)
{
    ai_sdk::DeepSeekProvider p;
    EXPECT_FALSE(p.IsAvaiable());
}
