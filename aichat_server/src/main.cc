#include "aichat_server.h"
#include "base/util/mylog.h"
#include <cstdlib>
#include <filesystem>

static const char *GetEnv(const char *name)
{
    const char *val = std::getenv(name);
    return val ? val : "";
}

int main()
{
    // 初始化日志
    ai_sdk::Logger::initLogger("aichat_server", "stdout", spdlog::level::debug);

    // ============ SMTP 邮箱配置 ============
    util::Config_info smtp_config(
        "",                          // username (未使用)
        GetEnv("SMTP_PASSWORD"),     // SMTP 授权码
        GetEnv("SMTP_URL"),          // SMTP 服务器地址
        GetEnv("SMTP_FROM")          // 发件人邮箱
    );

    // ============ 服务器配置 ============
    std::string db_name = GetEnv("DB_NAME");
    if (db_name.empty()) db_name = "aichat.db";

    std::string ip = GetEnv("SERVER_IP");
    if (ip.empty()) ip = "0.0.0.0";

    uint16_t port = 8080;

    // ============ 创建服务器 ============
    AIChatServer server(db_name, smtp_config);

    // ============ 注册模型 ============
    std::vector<ai_sdk::Config> configs;

    // DeepSeek
    const char *deepseek_key = std::getenv("DEEPSEEK_APIKEY");
    if (deepseek_key && deepseek_key[0] != '\0')
    {
        ai_sdk::Config cfg;
        cfg.model_type = ai_sdk::ModelType::DEEPSEEK;
        cfg.model = "deepseek-v4-flash";
        cfg.model_info.model_name = "deepseek-v4-flash";
        cfg.model_info.model_decs = "DeepSeek AI 模型";
        cfg.apikey = deepseek_key;
        configs.push_back(cfg);
        INFO("已注册模型: deepseek");
    }

    // ChatGPT
    const char *chatgpt_key = std::getenv("CHATGPT_APIKEY");
    if (chatgpt_key && chatgpt_key[0] != '\0')
    {
        ai_sdk::Config cfg;
        cfg.model_type = ai_sdk::ModelType::CHATGPT;
        cfg.model = "gpt-4o";
        cfg.model_info.model_name = "gpt-4o";
        cfg.model_info.model_decs = "ChatGPT AI 模型";
        cfg.apikey = chatgpt_key;
        configs.push_back(cfg);
        INFO("已注册模型: chatgpt");
    }

    // Gemini
    const char *gemini_key = std::getenv("GEMINI_APIKEY");
    if (gemini_key && gemini_key[0] != '\0')
    {
        ai_sdk::Config cfg;
        cfg.model_type = ai_sdk::ModelType::GEMINI;
        cfg.model = "gemini-3.5-flash";
        cfg.model_info.model_name = "gemini-3.5-flash";
        cfg.model_info.model_decs = "Gemini AI 模型";
        cfg.apikey = gemini_key;
        configs.push_back(cfg);
        INFO("已注册模型: gemini");
    }

    if (configs.empty())
    {
        WARN("未检测到任何模型 APIKEY 环境变量，可用变量: DEEPSEEK_APIKEY / CHATGPT_APIKEY / GEMINI_APIKEY");
    }

    server.RegisterModels(configs);

    // ============ 设置静态文件目录 ============
    // 可执行文件在 build/bin/，web/ 在 aichat_server/web/，即 ../../web
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
    fs::path web_dir = exe_dir / "../../web";
    server.SetWebRoot("/", web_dir.string());

    // ============ 启动服务器 ============
    INFO("服务器启动 {}:{}", ip, port);
    server.Run(ip, port);

    // 防止主线程退出
    while (true)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}
