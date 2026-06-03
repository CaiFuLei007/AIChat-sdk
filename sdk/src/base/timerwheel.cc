
#include "base/timerwheel.h"
#include "base/util/mylog.h"

#include <signal.h>
#include <time.h>
#include <sys/timerfd.h>
#include <sys/epoll.h>


int TimerWheel::CreateTimerFd()
{
    int fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if(fd < 0)
    {
        ERR("TIMERFD CREATE FAIL");
        return -1;
    }
    return fd;
}   

void TimerWheel::HandleAllTask(size_t index)
{
    std::vector<std::shared_ptr<TimeTask>> expired;
    
    {
        std::unique_lock<std::mutex> lock(mutex_);
        expired.swap(wheel_[index]);
    }
}

void TimerWheel::TimerFdReadCallback()
{
    uint64_t expirations;
    ssize_t ret = read(timerfd_, &expirations, sizeof(expirations));
    if(ret != sizeof(expirations))
    {
        if(ret < 0)
        {
            WARN("READ FAIL: {}", strerror(errno));
        }   
        else
        {
            WARN("READ INCOMPLETE: {} bytes", ret);
        }   
        return;
    } 

    for(int i = 0 ; i < expirations ; i++)
    {
        tick_ = tick_ % wheel_.size();
        HandleAllTask(tick_++);
    }
}

TimerWheel::TimerWheel()
:tick_(0) ,
wheel_(60) ,
timerfd_(CreateTimerFd()) ,
poller_(std::make_unique<Poller>()) ,
running_(false)
{
}

void TimerWheel::SetTask(const std::string& id ,size_t timeout , Task task)
{
    std::unique_lock<std::mutex> lock(mutex_);

    // 先删除旧任务
    auto old_it = tasks_.find(id);
    if(old_it != tasks_.end())
    {
        auto old_task = old_it->second.lock();
        if(old_task)
        {
            old_task->Cancel();
        }   
        tasks_.erase(old_it);
    }   

    std::shared_ptr<TimeTask> task_ptr = std::make_shared<TimeTask>(id , timeout , [this, id, task]()
    {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            tasks_.erase(id);
        }
        task();
    });
    int index = (tick_ + timeout) % wheel_.size();
    wheel_[index].push_back(task_ptr);
    tasks_.emplace(id , task_ptr);
}

void TimerWheel::UpdateTask(const std::string& id)
{
    std::unique_lock<std::mutex> lock(mutex_);

    auto it = tasks_.find(id);
    if(it == tasks_.end())
    {
        return ;
    }
    auto task = it->second.lock();
    if(!task) 
    {
        tasks_.erase(it);  // 清理失效的 weak_ptr
        return;
    }  
    int timeout = task->Timeout();
    int index = (tick_ + timeout) % wheel_.size();
    wheel_[index].push_back(task);
}

void TimerWheel::CancelTask(const std::string& id)
{
    std::unique_lock<std::mutex> lock(mutex_);

    auto it = tasks_.find(id);
    if(it == tasks_.end())
    {
        return ;
    }
    auto task = it->second.lock();
    if(!task) 
    {
        tasks_.erase(it);  // 清理失效的 weak_ptr
        return;
    }  
    task->Cancel();
    tasks_.erase(it);
}
bool TimerWheel::HasTask(const std::string &id)
{
    return tasks_.count(id);
}
void TimerWheel::Ready()
{
    struct itimerspec ts;
    ts.it_interval.tv_sec = 60;  // 超时时间
    ts.it_interval.tv_nsec = 0;
    ts.it_value.tv_sec = 60;
    ts.it_value.tv_nsec = 0;
    int ret = timerfd_settime(timerfd_, 0, &ts, NULL);
    if (ret) {
        ERR("TIMER SETTIME FAIL");
    }

    running_ = true;
    poller_->UpdateEvent(timerfd_ , EPOLLIN);
    worker_thread_ = std::thread(&TimerWheel::ThreadCallback , this);
}

void TimerWheel::ThreadCallback()
{
    std::vector<std::pair<int , int >> array;
    while(running_)
    {
        array = poller_->EpollWait(1000);
        for(auto [fd , _] : array)
        {
            if(fd == timerfd_)
            {
                TimerFdReadCallback();
            }
        }
    }
}