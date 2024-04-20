#pragma once

namespace avant::socket
{
    class socket_pair
    {
    public:
        socket_pair();
        ~socket_pair();
        int init();

    public:
        int get_me();
        int get_other();

    private:
        static constexpr int m_fd_size{2};
        int m_fd[2]{0};
    };
};