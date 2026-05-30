

#include "base/util/mylog.h"
#include <memory>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/async.h>

namespace ai_sdk{

std::shared_ptr<spdlog::logger> Logger::_logger = nullptr;
std::mutex Logger::_mutex;

Logger::Logger()
{}

void Logger::initLogger(const std::string& loggerName, const std::string& loggerFile, spdlog::level::level_enum logLevel){
    if(nullptr == _logger){
        std::lock_guard<std::mutex> lock(_mutex);
        if(nullptr == _logger){
            spdlog::flush_on(logLevel);
            // 启用异步日志，即将日志信息存放队列中，有后台线程负责写入
            // 参数1：队列大小，参数2：后台线程数量
            spdlog::init_thread_pool(32768, 1);
            if("stdout" == loggerFile){
                // 创建一个带颜色的输出到控制台的日志器
                _logger = spdlog::stdout_color_mt(loggerName);
            }else{
                // 创建一个文件输出的日志器，日志会被写入到指定的文件中
                _logger = spdlog::basic_logger_mt<spdlog::async_factory>(loggerName, loggerFile);
            }
        }

        _logger->set_pattern("[%H:%M:%S][%n][%-7l]%v");
        _logger->set_level(logLevel);
    }
}

std::shared_ptr<spdlog::logger> Logger::getLogger(){
    return _logger;
}


} // end ai_sdk

