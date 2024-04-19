#pragma once
#include <string>

#include "socket/socket.h"

namespace avant::socket
{
    /**
     * @brief socket for client
     *
     */
    class client_socket : public socket
    {
    public:
        client_socket();
        client_socket(const std::string &ip, int port);
        virtual ~client_socket();
    };
}