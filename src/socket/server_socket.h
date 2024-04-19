#pragma once
#include "socket/socket.h"

namespace avant::socket
{
    class server_socket : public socket
    {
    public:
        server_socket();
        server_socket(const std::string &ip, int port, int max_connections);
        virtual ~server_socket();
    };
}
