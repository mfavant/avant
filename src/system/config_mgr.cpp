#include "config_mgr.h"
#include <avant-inifile/inifile.h>

using config_mgr = avant::system::config_mgr;

config_mgr::config_mgr()
{
    this->m_ini = new avant::inifile::inifile();
}

config_mgr::~config_mgr()
{
    if (this->m_ini)
    {
        delete this->m_ini;
    }
}

int config_mgr::init(const std::string &file_path)
{
    if (!this->m_ini->load(file_path))
    {
        std::cerr << "Failed to load configuration file: " << file_path << std::endl;
        return -1;
    }

    const std::string const_app_id = (*this->m_ini)["server"]["app_id"];
    this->m_app_id = const_app_id;

    const std::string const_ip = (*this->m_ini)["server"]["ip"];
    this->m_ip = const_ip;

    const int const_port = (*this->m_ini)["server"]["port"];
    this->m_port = const_port;

    const int const_worker_cnt = (*this->m_ini)["server"]["worker_cnt"];
    this->m_worker_cnt = const_worker_cnt;

    const int const_max_client_cnt = (*this->m_ini)["server"]["max_client_cnt"];
    this->m_max_client_cnt = const_max_client_cnt;

    const int const_epoll_wait_time = (*this->m_ini)["server"]["epoll_wait_time"];
    this->m_epoll_wait_time = const_epoll_wait_time;

    const int const_accept_per_tick = (*this->m_ini)["server"]["accept_per_tick"];
    this->m_accept_per_tick = const_accept_per_tick;

    const std::string const_task_type = (*this->m_ini)["server"]["task_type"];
    this->m_task_type = const_task_type;

    const std::string const_http_static_dir = (*this->m_ini)["server"]["http_static_dir"];
    this->m_http_static_dir = const_http_static_dir;

    const std::string const_lua_dir = (*this->m_ini)["server"]["lua_dir"];
    this->m_lua_dir = const_lua_dir;

    const std::string const_crt_pem = (*this->m_ini)["server"]["crt.pem"];
    this->m_crt_pem = const_crt_pem;

    const std::string const_key_pem = (*this->m_ini)["server"]["key.pem"];
    this->m_key_pem = const_key_pem;

    const int const_use_ssl = (*this->m_ini)["server"]["use_ssl"];
    this->m_use_ssl = const_use_ssl;

    const int const_daemon = (*this->m_ini)["server"]["daemon"];
    this->m_daemon = const_daemon;

    const int const_log_level = (*this->m_ini)["server"]["log_level"];
    this->m_log_level = const_log_level;

    const std::string other_udp_svr_ip = (*this->m_ini)["server"]["other_udp_svr_ip"];
    this->m_other_udp_svr_ip = other_udp_svr_ip;

    const int other_udp_svr_port = (*this->m_ini)["server"]["other_udp_svr_port"];
    this->m_other_udp_svr_port = other_udp_svr_port;

    const int other_udp_svr_max_loop = (*this->m_ini)["server"]["other_udp_svr_max_loop"];
    this->m_other_udp_svr_max_loop = other_udp_svr_max_loop;

    const int const_max_ipc_conn_num = (*this->m_ini)["ipc"]["max_ipc_conn_num"];
    this->m_max_ipc_conn_num = const_max_ipc_conn_num;

    const std::string const_ipc_json_path = (*this->m_ini)["ipc"]["ipc_json_path"];
    this->m_ipc_json_path = const_ipc_json_path;

    return 0;
}
