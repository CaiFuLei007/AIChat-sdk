
#pragma once

/*
    - 对 Poll 进行封装
    - 成员 : 
            1) pollfd 
            2) 哈希表 : 关系所有的 Channel
    - 接口 : 
            1) 创建 pollfd
            2) 更新监听事件
            3) 移除监听

*/

#include <unistd.h>
#include <cstring>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>


class Poller
{
private:
    int epollfd_;
    std::unordered_set<int> fds_;

private:
    int CreatePoll();

    int EpollControl(int op , int fd, int event);

public:
    Poller()
    {
        epollfd_ = CreatePoll();
    }

    ~Poller()
    {
        close(epollfd_);
    }

    int Fd()
    {
        return epollfd_;
    }

    int UpdateEvent(int fd , int events);
    int RemoveEvent(int fd);

    std::vector<std::pair<int , int> > EpollWait();  // 返回所有就绪事件
};
