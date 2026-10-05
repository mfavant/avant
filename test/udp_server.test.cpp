// g++ -std=c++20 udp_server.test.cpp ../external/avant-ipc/udp_component.cpp -o udp_server.exe

#include <iostream>
#include <string>
#include "../external/avant-ipc/udp_component.h"

using namespace std;
using namespace avant::ipc;

int main(int argc, char **argv)
{
    udp_component udp;
    uint64_t recv_count = 0;

    udp.tick_callback = [](bool &to_stop)
    {
        // tick
    };

    udp.message_callback =
        [&udp, &recv_count](const char *buffer,
                            ssize_t len,
                            const sockaddr_storage &addr, // 传入客户端地址信息
                            socklen_t addr_len)
    {
        ++recv_count;

        string client_ip = udp.udp_component_get_ip(addr);
        int client_port = udp.udp_component_get_port(addr);

        std::cout << "** Received Message **" << std::endl;
        std::cout << "Data: " << std::string(buffer, len) << std::endl;
        std::cout << "Count: " << recv_count << std::endl;
        std::cout << "Client: " << client_ip << ":" << client_port << std::endl;
        std::cout << "Family: " << (addr.ss_family == AF_INET ? "IPv4" : (addr.ss_family == AF_INET6 ? "IPv6" : "Other")) << std::endl;
        std::cout << "---" << std::endl;

        // echo 回去
        // 传入 addr 参数，避免进入 event_loop()
        udp.udp_component_client(
            "", 0,
            buffer,
            len,
            (sockaddr *)&addr,
            addr_len);
    };

    udp.close_callback = []()
    {
        std::cout << "udp closed" << std::endl;
    };

    std::cout << "udp listening on 127.0.0.1 20027" << std::endl;
    // ipv4 127.0.0.1 20027
    // ipv6 ::1 20027
    udp.udp_component_server("127.0.0.1", 20027);

    return 0;
}
