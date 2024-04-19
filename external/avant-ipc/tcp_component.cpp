#include "tcp_component.h"
using avant::ipc::tcp_component;

tcp_component::tcp_component()
{
}

tcp_component::~tcp_component()
{
}

void tcp_component::on_init(std::unordered_map<std::string, std::pair<std::string, int>> targets)
{
    m_targets = targets;
}

int tcp_component::event_loop(std::string ip, int port)
{
    m_server_ip = ip;
    m_server_port = port;

    // init targets connection
}

int tcp_component::send(std::string &id, std::string &message)
{
}

int tcp_component::recv(std::string &id, std::string &message)
{
}
