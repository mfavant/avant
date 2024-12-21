// g++ client_nb_connect.cpp -o main.exe --std=c++11
#include <iostream>
#include <sys/epoll.h>
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

    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1)
    {
        std::cerr << "failed to create epoll" << std::endl;
        ::close(sockfd);
        return -1;
    }

    struct epoll_event event;
    event.events = EPOLLOUT;
    event.data.fd = sockfd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sockfd, &event) == -1)
    {
        std::cerr << "failed to add socket to epoll" << std::endl;
        ::close(sockfd);
        ::close(epoll_fd);
        return -1;
    }

    struct epoll_event events[1];
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
    ::close(epoll_fd);

    return 0;
}