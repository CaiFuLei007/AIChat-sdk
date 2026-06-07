#include "aichat_server.h"
#include "base/util/mylog.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <gflags/gflags.h>
#include <json/json.h>

// ==================== gflags 定义 ====================

// 通用
DEFINE_string(config, "", "配置文件路径 (JSON 格式)");

// 服务器
DEFINE_string(ip, "", "监听 IP 地址");
DEFINE_int32(port, 0, "监听端口号");
DEFINE_string(db_name, "", "SQLite 数据库文件名");
DEFINE_string(web_dir, "", "前端静态文件目录");

// SMTP 邮箱
DEFINE_string(smtp_url, "", "SMTP 服务器地址 (如 smtp.qq.com:465)");
DEFINE_string(smtp_from, "", "发件人邮箱");
DEFINE_string(smtp_password, "", "SMTP 授权码");

// 模型 API Keys
DEFINE_string(deepseek_apikey, "", "DeepSeek API Key");
DEFINE_string(chatgpt_apikey, "", "ChatGPT API Key");
DEFINE_string(gemini_apikey, "", "Gemini API Key");

// 代理
DEFINE_bool(set_proxy, false, "是否启用代理");
DEFINE_string(proxy_ip, "", "代理 IP 地址");
DEFINE_int32(proxy_port, 0, "代理端口号");

// ==================== 配置文件加载 ====================

static bool LoadConfigFile(const std::string &path, Json::Value &root)
{
    std::ifstream ifs(path);
    if (!ifs.is_open())
    {
        std::cerr << "无法打开配置文件: " << path << std::endl;
        return false;
    }

    Json::CharReaderBuilder builder;
    std::string errors;
    if (!Json::parseFromStream(builder, ifs, &root, &errors))
    {
        std::cerr << "配置文件解析失败: " << errors << std::endl;
        return false;
    }

    return true;
}

// 如果 gflags 的值为空（用户未通过命令行指定），则从配置文件中读取
static void OverrideFromConfig(const Json::Value &root, const std::string &section,
                               const std::string &key, std::string &target)
{
    if (target.empty() && root.isMember(section) && root[section].isMember(key))
    {
        target = root[section][key].asString();
    }
}

static void OverrideFromConfigInt(const Json::Value &root, const std::string &section,
                                  const std::string &key, int32_t &target)
{
    if (target == 0 && root.isMember(section) && root[section].isMember(key))
    {
        target = root[section][key].asInt();
    }
}

int main(int argc, char *argv[])
{
    // ==================== 设置帮助信息 ====================
    gflags::SetUsageMessage(
        "AIChat Server - AI 对话服务\n"
        "用法:\n"
        "  ./aichat_server --config config.json\n"
        "  ./aichat_server --ip 0.0.0.0 --port 8080 --deepseek_apikey=xxx\n"
        "查看帮助: ./aichat_server --help"
    );

    // ==================== 解析命令行参数 ====================
    gflags::ParseCommandLineFlags(&argc, &argv, true);

    // 没有任何参数时显示帮助并退出
    if (argc == 1 && FLAGS_config.empty())
    {
        std::cout << gflags::ProgramUsage() << std::endl;
        std::cout << "可用参数:" << std::endl;
        std::cout << "  --config          配置文件路径 (JSON 格式)" << std::endl;
        std::cout << "  --ip              监听 IP 地址 (默认: 0.0.0.0)" << std::endl;
        std::cout << "  --port            监听端口号 (默认: 8080)" << std::endl;
        std::cout << "  --db_name         数据库文件名 (默认: aichat.db)" << std::endl;
        std::cout << "  --web_dir         前端静态文件目录" << std::endl;
        std::cout << "  --smtp_url        SMTP 服务器地址" << std::endl;
        std::cout << "  --smtp_from       发件人邮箱" << std::endl;
        std::cout << "  --smtp_password   SMTP 授权码" << std::endl;
        std::cout << "  --deepseek_apikey DeepSeek API Key" << std::endl;
        std::cout << "  --chatgpt_apikey  ChatGPT API Key" << std::endl;
        std::cout << "  --gemini_apikey   Gemini API Key" << std::endl;
        std::cout << "  --set_proxy       是否启用代理 (默认: false)" << std::endl;
        std::cout << "  --proxy_ip        代理 IP 地址" << std::endl;
        std::cout << "  --proxy_port      代理端口号" << std::endl;
        std::cout << std::endl;
        std::cout << "示例:" << std::endl;
        std::cout << "  ./aichat_server --config config.json" << std::endl;
        std::cout << "  ./aichat_server --port 9090 --deepseek_apikey=sk-xxx" << std::endl;
        std::cout << "  ./aichat_server --set_proxy --proxy_ip=127.0.0.1 --proxy_port=7890" << std::endl;
        return 0;
    }

    // ==================== 初始化日志 ====================
    ai_sdk::Logger::initLogger("aichat_server", "stdout", spdlog::level::debug);

    // ==================== 加载配置文件 ====================
    Json::Value config_root;
    if (!FLAGS_config.empty())
    {
        if (!LoadConfigFile(FLAGS_config, config_root))
        {
            std::cerr << "配置加载失败，退出" << std::endl;
            return 1;
        }
        INFO("已加载配置文件: {}", FLAGS_config);
    }

    // ==================== 合并配置：命令行优先，配置文件兜底 ====================

    // 服务器
    OverrideFromConfig(config_root, "server", "ip", FLAGS_ip);
    OverrideFromConfig(config_root, "server", "db_name", FLAGS_db_name);
    OverrideFromConfig(config_root, "server", "web_dir", FLAGS_web_dir);
    OverrideFromConfigInt(config_root, "server", "port", FLAGS_port);

    std::string ip = FLAGS_ip.empty() ? "0.0.0.0" : FLAGS_ip;
    int32_t port = FLAGS_port == 0 ? 8080 : FLAGS_port;
    std::string db_name = FLAGS_db_name.empty() ? "aichat.db" : FLAGS_db_name;

    // SMTP
    OverrideFromConfig(config_root, "smtp", "url", FLAGS_smtp_url);
    OverrideFromConfig(config_root, "smtp", "from", FLAGS_smtp_from);
    OverrideFromConfig(config_root, "smtp", "password", FLAGS_smtp_password);

    util::Config_info smtp_config(
        "",
        FLAGS_smtp_password,
        FLAGS_smtp_url,
        FLAGS_smtp_from
    );

    // 模型 API Keys
    OverrideFromConfig(config_root, "models", "deepseek_apikey", FLAGS_deepseek_apikey);
    OverrideFromConfig(config_root, "models", "chatgpt_apikey", FLAGS_chatgpt_apikey);
    OverrideFromConfig(config_root, "models", "gemini_apikey", FLAGS_gemini_apikey);

    // 代理配置
    if (config_root.isMember("proxy") && config_root["proxy"].isMember("set_proxy")) {
        FLAGS_set_proxy = config_root["proxy"]["set_proxy"].asBool();
    }
    OverrideFromConfig(config_root, "proxy", "proxy_ip", FLAGS_proxy_ip);
    OverrideFromConfigInt(config_root, "proxy", "proxy_port", FLAGS_proxy_port);

    // ==================== 创建服务器 ====================
    AIChatServer server(db_name, smtp_config);

    // ==================== 注册模型 ====================
    std::vector<ai_sdk::Config> configs;

    // DeepSeek
    if (!FLAGS_deepseek_apikey.empty())
    {
        ai_sdk::Config cfg;
        cfg.model_type = ai_sdk::ModelType::DEEPSEEK;
        cfg.model = "deepseek-v4-flash";
        cfg.model_info.model_name = "deepseek-v4-flash";
        cfg.model_info.model_decs = "DeepSeek AI 模型";
        cfg.apikey = FLAGS_deepseek_apikey;
        cfg.proxy.set_proxy = FLAGS_set_proxy;
        cfg.proxy.proxy_ip = FLAGS_proxy_ip;
        cfg.proxy.proxy_port = FLAGS_proxy_port;
        configs.push_back(cfg);
        INFO("已注册模型: deepseek");
    }

    // ChatGPT
    if (!FLAGS_chatgpt_apikey.empty())
    {
        ai_sdk::Config cfg;
        cfg.model_type = ai_sdk::ModelType::CHATGPT;
        cfg.model = "gpt-4o";
        cfg.model_info.model_name = "gpt-4o";
        cfg.model_info.model_decs = "ChatGPT AI 模型";
        cfg.apikey = FLAGS_chatgpt_apikey;
        cfg.proxy.set_proxy = FLAGS_set_proxy;
        cfg.proxy.proxy_ip = FLAGS_proxy_ip;
        cfg.proxy.proxy_port = FLAGS_proxy_port;
        configs.push_back(cfg);
        INFO("已注册模型: chatgpt");
    }

    // Gemini
    if (!FLAGS_gemini_apikey.empty())
    {
        ai_sdk::Config cfg;
        cfg.model_type = ai_sdk::ModelType::GEMINI;
        cfg.model = "gemini-3.5-flash";
        cfg.model_info.model_name = "gemini-3.5-flash";
        cfg.model_info.model_decs = "Gemini AI 模型";
        cfg.apikey = FLAGS_gemini_apikey;
        cfg.proxy.set_proxy = FLAGS_set_proxy;
        cfg.proxy.proxy_ip = FLAGS_proxy_ip;
        cfg.proxy.proxy_port = FLAGS_proxy_port;
        configs.push_back(cfg);
        INFO("已注册模型: gemini");
    }

    if (configs.empty())
    {
        WARN("未检测到任何模型 API Key，请通过 --deepseek_apikey / --chatgpt_apikey / --gemini_apikey 指定，或在配置文件中填写");
    }

    server.RegisterModels(configs);

    // ==================== 设置静态文件目录 ====================
    std::string web_dir = FLAGS_web_dir;
    if (web_dir.empty())
    {
        // 默认: 可执行文件的 ../web
        namespace fs = std::filesystem;
        fs::path exe_dir;
#ifdef __linux__
        char buf[4096];
        ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
        if (len > 0) {
            buf[len] = '\0';
            exe_dir = fs::path(buf).parent_path();
        } else {
            exe_dir = fs::current_path();
        }
#else
        exe_dir = fs::current_path();
#endif
        web_dir = (exe_dir / "../../web").string();
    }
    server.SetWebRoot("/", web_dir);

    // ==================== 启动服务器 ====================
    INFO("服务器启动 {}:{}", ip, port);
    server.Run(ip, port);

    // 防止主线程退出
    while (true)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}
