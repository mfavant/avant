#include "event/event_poller.h"

using namespace tubekit::event;

event_poller::event_poller() : m_epfd(0),
                               m_max_connections(0),
                               m_events(nullptr)
{
}

event_poller::~event_poller()
{
    if (m_epfd > 0)
    {
        if (m_epfd != 0)
            ::close(m_epfd);
        m_epfd = 0;
    }
    if (m_events != nullptr)
    {
        delete[] m_events;
        m_events = nullptr;
    }
}

void event_poller::create(int max_connections)
{
    m_max_connections = max_connections;
    m_epfd = ::epoll_create(max_connections + 1); // plane
    if (m_epfd < 0)
    {
        return;
    }
    if (m_events != nullptr)
    {
        delete[] m_events;
        m_events = nullptr;
    }
    m_events = new ::epoll_event[max_connections + 1];
}

int event_poller::ctrl(int fd, void *ptr, uint32_t events, int op, bool et /*=true*/)
{
    struct ::epoll_event ev;
    ev.data.ptr = ptr;
    if (et && !(EPOLL_CTL_DEL & events))
    {
        ev.events = events | EPOLLET; // decause epoll default LT
        /*
        epoll是linux系统最新的处理多连接的高效率模型， 工作在两种方式下， EPOLLLT方式和EPOLLET方式。
        EPOLLLT是系统默认， 工作在这种方式下， 程序员不易出问题， 在接收数据时，只要socket输入缓存有数据，
        都能够获得EPOLLIN的持续通知， 同样在发送数据时， 只要发送缓存够用， 都会有持续不间断的EPOLLOUT
        通知。而对于EPOLLET是另外一种触发方式， 比EPOLLLT要高效很多， 对程序员的要求也多些， 程序员必须
        小心使用，因为工作在此种方式下时， 在接收数据时， 如果有数据只会通知一次， 假如read时未读完数据，
        那么不会再有EPOLLIN的通知了， 直到下次有新的数据到达时为止； 当发送数据时， 如果发送缓存未满也只有
        一次EPOLLOUT的通知， 除非你把发送缓存塞满了， 才会有第二次EPOLLOUT通知的机会， 所以在此方式下
        read和write时都要处理好。 暂时写到这里， 留作备忘。
        如果将一个socket描述符添加到两个epoll中， 那么即使在EPOLLET模式下， 只要前一个epoll_wait时，
        未读完， 那么后一个epoll_wait事件时， 也会得到读的通知， 但前一个读完的情况下， 后一个epoll就不会得到读事件的通知了
        */
    }
    else
    {
        ev.events = events;
    }
    return epoll_ctl(m_epfd, op, fd, &ev);
}

int event_poller::add(int fd, void *ptr, uint32_t events, bool et /*=true*/)
{
    return ctrl(fd, ptr, events, EPOLL_CTL_ADD, et);
}

int event_poller::mod(int fd, void *ptr, uint32_t events, bool et /*=true*/)
{
    return ctrl(fd, ptr, events, EPOLL_CTL_MOD, et);
}

int event_poller::del(int fd, void *ptr, uint32_t events, bool et /*=true*/)
{
    return ctrl(fd, ptr, events, EPOLL_CTL_DEL, et);
}

int event_poller::wait(int millsecond)
{
    return epoll_wait(m_epfd, m_events, m_max_connections + 1, millsecond);
}
