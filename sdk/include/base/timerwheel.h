

#pragma once

/*
    - 基于时间轮的定时器
    - 分为两个部分进行实现 : TimeTask , TimerWheel
    - TimeTask
        - 成员 : 
                1) 定时任务
                2) 触发时间
                3) 任务是否取消
                4) 任务 ID
        - 接口 : 
                1) 构造
                2) 析构
                3) 取消定时任务

    - TimerWheel 
        - 成员 :
                1) vector<vector<std::shared_ptr<Task> > > 二维数组作为时间轮存放所有任务
                2) click 指针
                3) hash : 存放所有任务
                4) timerfd
        - 接口 :
                1) 创建 timerfd 
                2) timerfd 读事件回调
                3) 添加定时任务 , 删除定时任务
                4) 更新定时任务
                5) 处理所有定时任务
*/

#include "base/poller.h"

#include <iostream>
#include <functional>
#include <vector>
#include <memory>
#include <unordered_map>
#include <thread>
#include <cstring>
#include <mutex>
#include <atomic>

class TimeTask
{
    using Task = std::function<void()>;
private:
    std::string id_;
    size_t timeout_;
    Task task_;
    bool is_cancel_;
    
public:
    TimeTask(const std::string& id  ,size_t timeout , Task task)
    :id_(id) , 
    timeout_(timeout) , 
    task_(task) , 
    is_cancel_(false)
    {}

    ~TimeTask()
    {
        if(!is_cancel_)
        {
            if(task_)
            {
                task_();
            }
        }
    }

    void Cancel()
    {
        is_cancel_ = true;
    }

    size_t Timeout()
    {
        return timeout_;
    }
};


class TimerWheel
{
    using Task = std::function<void()>;
private:
    std::unordered_map<std::string , std::weak_ptr<TimeTask> > tasks_;
    std::vector<std::vector<std::shared_ptr<TimeTask> > > wheel_;
    size_t tick_;
    int timerfd_;
    std::atomic<bool> running_;
    std::thread worker_thread_;
    std::mutex mutex_;

    std::unique_ptr<Poller> poller_;
private:
    int CreateTimerFd();

    void HandleAllTask(size_t index);
    void TimerFdReadCallback();

    void ThreadCallback();
public:
    TimerWheel();
    ~TimerWheel()
    {
        running_ = false;
        close(timerfd_);  // 关闭 timerfd 会让 epoll_wait 返回
        if(worker_thread_.joinable())
        {
            worker_thread_.join();
        }  
    }

    void SetTask(const std::string& id ,size_t timeout , Task task);
    void UpdateTask(const std::string& id);
    void CancelTask(const std::string& id);

    bool HasTask(const std::string &id);
    void Ready();
    
};



