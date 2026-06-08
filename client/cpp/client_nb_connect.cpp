// g++ client_nb_connect.cpp -o main.exe --std=c++11
#include <iostream>

#if defined(__linux__)
#include <sys/epoll.h>
#elif defined(__APPLE__)
#include <sys/event.h>
#endif

#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iostream>
#include <cstring>
#include <fcntl.h>

int set_nonblocking(int sockfd)
{
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags == -1)
    {
        std::cerr << "failed to get socket flags" << std::endl;
        return -1;
    }
    return fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
}

int main(int argc, const char **argv)
{
    if (argc != 3)
    {
        std::cerr << "./main.exe ipv4_ip port" << std::endl;
        return -1;
    }
    const char *server_ip = argv[1];
    int server_port = ::atoi(argv[2]);

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
    {
        std::cerr << "failed to create socket" << std::endl;
        return -1;
    }

    if (set_nonblocking(sockfd) == -1)
    {
        ::close(sockfd);
        return -1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    inet_pton(AF_INET, server_ip, &server_addr.sin_addr);

    int result = connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (result == -1 && errno != EINPROGRESS)
    {
        std::cerr << "failed to initiate connect" << std::endl;
        close(sockfd);
        return -1;
    }

#ifdef __linux__
    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1)
    {
        std::cerr << "failed to create epoll" << std::endl;
        ::close(sockfd);
        return -1;
    }

    epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLOUT;
    ev.data.fd = sockfd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sockfd, &ev) == -1)
    {
        std::cerr << "failed to add socket to epoll" << std::endl;
        ::close(sockfd);
        ::close(epoll_fd);
        return -1;
    }

    epoll_event events[1];
    int nfds = epoll_wait(epoll_fd, events, 1, 5000); // wait 5 s
    if (nfds == -1)
    {
        std::cerr << "epoll_wait failed" << std::endl;
        ::close(sockfd);
        ::close(epoll_fd);
        return -1;
    }
    else if (nfds == 0)
    {
        std::cerr << "connect timed out" << std::endl;
        ::close(sockfd);
        ::close(epoll_fd);
        return -1;
    }

    ::close(epoll_fd);

#endif

#ifdef __APPLE__
    // 创建一个 kqueue 实例
    int kq = kqueue();
    if (kq == -1)
    {
        std::cerr << "failed to create kqueue" << std::endl;
        ::close(sockfd);
        return -1;
    }

    // 定义一个kqueue时间结构
    struct kevent change;
    // 设置监听事件 监听写事件
    EV_SET(&change, sockfd, EVFILT_WRITE, EV_ADD, 0, 0, NULL);
    // 把change注册到kqueue kq 哪个kqueue &change 要提交的事件数组
    // 1 数组元素数量 NULL 不取返回事件 0 返回事件数量为0 NULL 不等待
    // 相当于 epoll_ctl
    if (kevent(kq, &change, 1, NULL, 0, NULL) == -1)
    {
        std::cerr << "failed to add socket to kqueue" << std::endl;
        ::close(sockfd);
        ::close(kq);
        return -1;
    }

    // 准备接收事件
    struct kevent events[1];
    struct timespec timeout; // 设置超时
    timeout.tv_sec = 5;      // 5 s
    timeout.tv_nsec = 0;
    // 等待事件 kq kqueue对象 NULL 没有新事件需要提交
    // 0 提交事件数量 events 接收触发事件
    // 1 最多返回1个事件
    // &timeout 超时设置
    int nfds = kevent(kq, NULL, 0, events, 1, &timeout);

    if (nfds == -1)
    {
        std::cerr << "kevent failed" << std::endl;
        ::close(sockfd);
        ::close(kq);
        return -1;
    }
    else if (nfds == 0)
    {
        std::cerr << "connect timed out" << std::endl;
        ::close(sockfd);
        ::close(kq);
        return -1;
    }

    ::close(kq);

#endif

    // checking result
    int error = 0;
    socklen_t len = sizeof(error);
    if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, &len) == -1)
    {
        std::cerr << "getsockopt failed" << std::endl;
    }
    else if (error != 0)
    {
        std::cerr << "connection failed " << strerror(error) << std::endl;
    }
    else
    {
        std::cout << "connected successfully" << std::endl;
    }

    ::close(sockfd);

    return 0;
}
