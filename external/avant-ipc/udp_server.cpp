// g++ udp_server.cpp udp_component.cpp -o udp_server.exe --std=c++11
#include <iostream>
#include "udp_component.h"
using namespace std;
using namespace avant::ipc;

int main(int argc, char **argv)
{
    udp_component udp;
    uint64_t recv_count = 0;

    udp.tick_callback = [&udp](bool &to_stop)
    {
        // std::cout<<"on_tick"<<std::endl;
    };

    udp.message_callback = [&udp, &recv_count](const char *buffer, ssize_t len, sockaddr &addr, size_t addr_len)
    {
        ++recv_count;
        std::cout << std::string(buffer, len) << " recv_count " << recv_count << " family " << addr.sa_family << std::endl;
        // pingpong
        udp.udp_component_client("", 0,
                                 buffer,
                                 len,
                                 &addr,
                                 addr_len);
    };

    udp.close_callback = [&udp]()
    {
        // close callback
    };

    udp.udp_component_server("::", 20025);

    return 0;
}
