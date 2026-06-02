

#pragma once

#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <curl/curl.h>

namespace util
{

    struct Config_info
    {
        Config_info() = default;
        Config_info(const std::string &s_username, const std::string &s_password, const std::string &s_url, const std::string &s_from)
            : username(s_username),
              password(s_password),
              url(s_url),
              from(s_from)
        {
        }

        std::string username;
        std::string password;
        std::string url;
        std::string from;
    };

    class Curl_Base
    {
    public:
        Curl_Base(const Config_info &config)
            : config_(config)
        {
        }

        virtual int Send(const std::vector<std::string> &clients, const std::string &message) = 0;

    protected:
        Config_info config_;
    };

    class Curl : public Curl_Base
    {
    private:
        static size_t read_callback(char *ptr, size_t size, size_t nmemb, void *userdata)
        {
            std::stringstream *ss = static_cast<std::stringstream *>(userdata);
            ss->read(ptr, size * nmemb);
            return ss->gcount();
        }
    public:
        Curl(const Config_info &config)
            : Curl_Base(config)
        {
            if (curl_global_init(CURL_GLOBAL_ALL) != 0)
                std::cout << "curl_global_init error" << std::endl;
        }

        virtual int Send(const std::vector<std::string> &clients, const std::string &message) override
        {
            // 1. 创建句柄
            // 2. 设置配置信息
            // 3. 发送
            // 4. 销毁,清除
            CURL *curl = curl_easy_init();
            // curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

            if (curl == nullptr)
            {
                std::cout << "curl_easy_init() err\n";
                return false;
            }
            if (curl_easy_setopt(curl, CURLOPT_URL, config_.url.c_str()) != CURLE_OK)
            {
                std::cout << "curl_easy_setopt : CURLOPT_URL error" << std::endl;
                return -1;
            }
            if (curl_easy_setopt(curl, CURLOPT_USERNAME, config_.from.c_str()) != CURLE_OK)
            {
                std::cout << "curl_easy_setopt : CURLOPT_USERNAME error" << std::endl;
                return -1;
            }
            if (curl_easy_setopt(curl, CURLOPT_PASSWORD, config_.password.c_str()) != CURLE_OK)
            {
                std::cout << "curl_easy_setopt : CURLOPT_PASSWORD error" << std::endl;
                return -1;
            }
            if (curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L) != CURLE_OK)
            {
                std::cout << "curl_easy_setopt : CURLOPT_UPLOAD error" << std::endl;
                return -1;
            }
            if (curl_easy_setopt(curl, CURLOPT_MAIL_FROM, config_.from.c_str()) != CURLE_OK)
            {
                std::cout << "curl_easy_setopt : CURLOPT_MAIL_FROM error" << std::endl;
                return -1;
            }
            curl_slist *curl_clients = nullptr;
            for (auto client : clients)
            {
                if ((curl_clients = curl_slist_append(curl_clients, client.c_str())) == nullptr)
                {
                    std::cout << "curl_slist_append error" << std::endl;
                    return -1;
                }
            }
            if (curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, curl_clients) != CURLE_OK)
            {
                std::cout << "curl_easy_setopt : CURLOPT_MAIL_RCPT error" << std::endl;
                return -1;
            }

            std::stringstream ss(message);
            if (curl_easy_setopt(curl, CURLOPT_READDATA, (void *)&ss) != CURLE_OK)
            {
                std::cout << "curl_easy_setopt : CURLOPT_READDATA error" << std::endl;
                return -1;
            }
            auto ret = curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_callback);

            if (ret != CURLE_OK)
            {
                std::cout << "curl_easy_setopt : CURLOPT_READFUNCTION error" << std::endl;
                return -1;
            }

            auto res = curl_easy_perform(curl);
            if (res != CURLE_OK)
            {
                std::cerr << "curl_easy_perform() failed: " << res
                          << " (" << curl_easy_strerror(res) << ")" << std::endl;
                return -1;
            }
            curl_slist_free_all(curl_clients);
            curl_easy_cleanup(curl);
            return true;
        }

        ~Curl()
        {
            curl_global_cleanup();
        }

    private:
    };

};