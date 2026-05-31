
#include "base/poller.h"
#include "base/util/mylog.h"

#include <sys/epoll.h>
#include <errno.h>

int Poller::CreatePoll()
{
    int fd = epoll_create(1);
    if(fd < 0)
    {
        ERR("EPOLL CREATE FAIL");
    }
    return fd;
}

int Poller::EpollControl(int op , int fd , int events)
{
    struct epoll_event epevent;
    epevent.data.fd = fd;
    epevent.events = events;
    return epoll_ctl(epollfd_ , op , fd , &epevent);
}

int Poller::UpdateEvent(int fd , int events)
{
    auto it = fds_.find(fd);
    if(it == fds_.end())
    {
        int ret = EpollControl(EPOLL_CTL_ADD , fd , events);
        if(ret == 0)
        {
            fds_.emplace(fd);
        }
        return ret;
    }
    return EpollControl(EPOLL_CTL_MOD , fd , events);
}

int Poller::RemoveEvent(int fd )
{
    auto it = fds_.find(fd);
    if(it == fds_.end())
    {
        return -1;
    }
    int ret = epoll_ctl(epollfd_ , EPOLL_CTL_DEL , fd , nullptr);
    if(ret == 0)
    {
        fds_.erase(fd);
    }
    return ret;
}


std::vector<std::pair<int , int> > Poller::EpollWait(int wait_time)
{
    struct epoll_event events[1024];
    int nfds = 0 ;
    while((nfds = epoll_wait(epollfd_, events, 1024, wait_time)) < 0)
    {
        if(errno == EINTR)
        {
            continue;
        }
        return {};
    }
    std::vector<std::pair<int , int> > ret(nfds);
    for (int i = 0; i < nfds; ++i) {
        ret[i].first = events[i].data.fd;
        ret[i].second = events[i].events;
    }
    return ret;
}