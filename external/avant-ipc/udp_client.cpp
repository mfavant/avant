// g++ udp_client.cpp udp_component.cpp -o udp_client.exe --std=c++11

#include <iostream>
#include "udp_component.h"
using namespace std;
using namespace avant::ipc;

int main(int argc, char **argv)
{
    udp_component udp;
    uint64_t pingpong_count = 0;

    udp.tick_callback = [](bool &to_stop)
    {
        // tick
    };

    udp.message_callback =
        [&udp, &pingpong_count](const char *buffer,
                                ssize_t len,
                                const sockaddr_storage &addr, // 必须 const sockaddr_storage&
                                socklen_t addr_len)           // 必须 socklen_t
    {
        pingpong_count++;

        std::cout << std::string(buffer, len)
                  << " pingpong_count " << pingpong_count
                  << std::endl;

        // --- 核心修改部分：直接解析 sockaddr_storage ---
        char ipbuf[INET6_ADDRSTRLEN]; // 最大的地址字符串长度
        int client_port = -1;

        // 检查地址族并进行类型转换
        if (addr.ss_family == AF_INET)
        {
            const struct sockaddr_in *a = (const struct sockaddr_in *)&addr;
            // IPv4 地址转换：使用 inet_ntop
            if (inet_ntop(AF_INET, &(a->sin_addr), ipbuf, INET_ADDRSTRLEN) == nullptr)
            {
                std::strncpy(ipbuf, "Unknown IPv4", sizeof(ipbuf));
            }
            // 端口转换：从网络字节序转为主机字节序
            client_port = ntohs(a->sin_port);
        }
        else if (addr.ss_family == AF_INET6)
        {
            const struct sockaddr_in6 *a6 = (const struct sockaddr_in6 *)&addr;
            // IPv6 地址转换：使用 inet_ntop
            if (inet_ntop(AF_INET6, &(a6->sin6_addr), ipbuf, INET6_ADDRSTRLEN) == nullptr)
            {
                std::strncpy(ipbuf, "Unknown IPv6", sizeof(ipbuf));
            }
            // 端口转换：从网络字节序转为主机字节序
            client_port = ntohs(a6->sin6_port);
        }
        else
        {
            // 其他地址族
            std::strncpy(ipbuf, "Unsupported Family", sizeof(ipbuf));
        }

        std::string client_ip(ipbuf);
        // ----------------------------------------------------

        std::cout << "** Received Message **" << std::endl;
        std::cout << "Data: " << std::string(buffer, len) << std::endl;
        // 输出解析结果
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
        // close
    };

    // IPv6 地址示例
    udp.udp_component_client(
        "fe80::14c8:ffff:fe00:34", 20027,
        "pingpong", 8, nullptr, 0);
    udp.event_loop();

    return 0;
}
