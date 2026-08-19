
#pragma once

/*
    - 对 spdlog 进行封装

*/

#include <mutex>
#include <spdlog/logger.h>
#include <spdlog/spdlog.h>

namespace aichat_sdk{


class Logger{
    public:
        static void initLogger(const std::string& loggerName, const std::string& loggerFile, spdlog::level::level_enum logLevel = spdlog::level::info);
        static std::shared_ptr<spdlog::logger> getLogger();
    private:
        Logger();
        Logger(const Logger&) = delete;
        Logger& operator=(const Logger&) = delete;
    private:
        static std::shared_ptr<spdlog::logger> _logger;
        static std::mutex _mutex;
};


#define TRACE(format, ...) aichat_sdk::Logger::getLogger()->trace("[{:>10s}:{:<4d}]" format, __FILE__, __LINE__, ##__VA_ARGS__)
#define DBG(format, ...) aichat_sdk::Logger::getLogger()->debug("[{:>10s}:{:<4d}]" format, __FILE__, __LINE__, ##__VA_ARGS__)
#define INFO(format, ...) aichat_sdk::Logger::getLogger()->info("[{:>10s}:{:<4d}]" format, __FILE__, __LINE__, ##__VA_ARGS__)
#define WARN(format, ...) aichat_sdk::Logger::getLogger()->warn("[{:>10s}:{:<4d}]" format, __FILE__, __LINE__, ##__VA_ARGS__)
#define ERR(format, ...) aichat_sdk::Logger::getLogger()->error("[{:>10s}:{:<4d}]" format, __FILE__, __LINE__, ##__VA_ARGS__)
#define CRIT(format, ...) aichat_sdk::Logger::getLogger()->critical("[{:>10s}:{:<4d}]" format, __FILE__, __LINE__, ##__VA_ARGS__)


} // end aichat_sdk