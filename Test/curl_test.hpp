#pragma once

#include <gtest/gtest.h>
#include "curl_util.hpp"
#include <cstdlib>

class CurlTest : public ::testing::Test
{
protected:
    std::string smtp_url_;
    std::string smtp_from_;
    std::string smtp_password_;
    std::string smtp_to_;

    void SetUp() override
    {
        const char *url = std::getenv("SMTP_URL");
        const char *from = std::getenv("SMTP_FROM");
        const char *password = std::getenv("SMTP_PASSWORD");
        const char *to = std::getenv("SMTP_TO");

        ASSERT_NE(url, nullptr) << "环境变量 SMTP_URL 未设置";
        ASSERT_NE(from, nullptr) << "环境变量 SMTP_FROM 未设置";
        ASSERT_NE(password, nullptr) << "环境变量 SMTP_PASSWORD 未设置";
        ASSERT_NE(to, nullptr) << "环境变量 SMTP_TO 未设置";

        smtp_url_ = url;
        smtp_from_ = from;
        smtp_password_ = password;
        smtp_to_ = to;
    }
};

TEST_F(CurlTest, SendEmailSuccess)
{
    util::Config_info config("", smtp_password_, smtp_url_, smtp_from_);
    util::Curl curl(config);

    bool ret = curl.Send({smtp_to_}, "这是一封单元测试邮件，验证码：12345");
    EXPECT_TRUE(ret);
}

TEST_F(CurlTest, SendEmailToMultipleRecipients)
{
    util::Config_info config("", smtp_password_, smtp_url_, smtp_from_);
    util::Curl curl(config);

    bool ret = curl.Send({smtp_to_}, "多收件人测试邮件");
    EXPECT_TRUE(ret);
}

TEST_F(CurlTest, SendEmailWithEmptyMessage)
{
    util::Config_info config("", smtp_password_, smtp_url_, smtp_from_);
    util::Curl curl(config);

    bool ret = curl.Send({smtp_to_}, "");
    EXPECT_TRUE(ret);
}

TEST_F(CurlTest, SendEmailInvalidUrl)
{
    util::Config_info config("", smtp_password_, "smtps://invalid.smtp.server:999", smtp_from_);
    util::Curl curl(config);

    int ret = curl.Send({smtp_to_}, "这封邮件应该发送失败");
    EXPECT_NE(ret, true);
}
