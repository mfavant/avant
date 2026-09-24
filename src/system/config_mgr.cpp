#include "config_mgr.h"
#include <iostream>
#include <string>
#include <avant-inifile/inifile.h>
#include <avant-log/logger.h>
#include "task/task_type.h"
#include <format>

using config_mgr = avant::system::config_mgr;

config_mgr::config_mgr()
{
}

config_mgr::~config_mgr()
{
}

int config_mgr::init(const std::string &file_path)
{
    if (!this->m_ini.load(get_root_path() + file_path))
    {
        std::cerr << "Failed to load configuration file: " << get_root_path() + file_path << std::endl;
        return -1;
    }

    const std::string const_app_id = this->m_ini["server"]["app_id"];
    this->m_app_id = const_app_id;

    const std::string const_ip = this->m_ini["server"]["ip"];
    this->m_ip = const_ip;

    const int const_port = this->m_ini["server"]["port"];
    this->m_port = const_port;

    const int const_worker_cnt = this->m_ini["server"]["worker_cnt"];
    this->m_worker_cnt = const_worker_cnt;

    const int const_max_client_cnt = this->m_ini["server"]["max_client_cnt"];
    this->m_max_client_cnt = const_max_client_cnt;

    const int const_epoll_wait_time = this->m_ini["server"]["epoll_wait_time"];
    this->m_epoll_wait_time = const_epoll_wait_time;

    const int const_accept_per_tick = this->m_ini["server"]["accept_per_tick"];
    this->m_accept_per_tick = const_accept_per_tick;

    const std::string const_task_type = this->m_ini["server"]["task_type"];
    this->m_task_type = const_task_type;

    const std::string const_http_static_dir = this->m_ini["server"]["http_static_dir"];
    this->m_http_static_dir = const_http_static_dir;

    const std::string const_lua_dir = this->m_ini["server"]["lua_dir"];
    this->m_lua_dir = const_lua_dir;

    const std::string const_crt_pem = this->m_ini["server"]["crt.pem"];
    this->m_crt_pem = const_crt_pem;

    const std::string const_key_pem = this->m_ini["server"]["key.pem"];
    this->m_key_pem = const_key_pem;

    const int const_use_ssl = this->m_ini["server"]["use_ssl"];
    this->m_use_ssl = const_use_ssl;

    const int const_daemon = this->m_ini["server"]["daemon"];
    this->m_daemon = const_daemon;

    const int const_log_level = this->m_ini["server"]["log_level"];
    this->m_log_level = const_log_level;

    const std::string other_udp_svr_ip = this->m_ini["server"]["other_udp_svr_ip"];
    this->m_other_udp_svr_ip = other_udp_svr_ip;

    const int other_udp_svr_port = this->m_ini["server"]["other_udp_svr_port"];
    this->m_other_udp_svr_port = other_udp_svr_port;

    const int other_udp_svr_max_loop = this->m_ini["server"]["other_udp_svr_max_loop"];
    this->m_other_udp_svr_max_loop = other_udp_svr_max_loop;

    const int const_max_ipc_conn_num = this->m_ini["ipc"]["max_ipc_conn_num"];
    this->m_max_ipc_conn_num = const_max_ipc_conn_num;

    const std::string const_ipc_json_path = this->m_ini["ipc"]["ipc_json_path"];
    this->m_ipc_json_path = const_ipc_json_path;

    if (this->m_ini.has("server", "app_id") && this->m_app_id.empty())
    {
        std::cerr << std::format("config_mgr::init [server]app_id is empty") << std::endl;
        return -1;
    }
    if (this->m_ini.has("server", "ip") && this->m_ip.empty())
    {
        std::cerr << std::format("config_mgr::init [server]ip is empty") << std::endl;
        return -1;
    }
    if (this->m_port <= 0 || this->m_port > PORT_MAX)
    {
        std::cerr << std::format("config_mgr::init [server]port {} out of range [1, PORT_MAX]", this->m_port) << std::endl;
        return -1;
    }
    if (this->m_worker_cnt <= 0 || this->m_worker_cnt > WORKER_NUM_MAX)
    {
        std::cerr << std::format("config_mgr::init [server]worker_cnt {} out of range [1, WORKER_NUM_MAX]", this->m_worker_cnt) << std::endl;
        return -1;
    }
    if (this->m_max_client_cnt <= 0 || this->m_max_client_cnt > CLIENT_CNT_MAX)
    {
        std::cerr << std::format("config_mgr::init [server]max_client_cnt {} out of range [1, CLIENT_CNT_MAX]", this->m_max_client_cnt) << std::endl;
        return -1;
    }
    if (this->m_epoll_wait_time <= 0)
    {
        std::cerr << std::format("config_mgr::init [server]epoll_wait_time {} must be > 0", this->m_epoll_wait_time) << std::endl;
        return -1;
    }
    if (this->m_accept_per_tick <= 0)
    {
        std::cerr << std::format("config_mgr::init [server]accept_per_tick {} must be > 0", this->m_accept_per_tick) << std::endl;
        return -1;
    }
    if (this->m_ini.has("server", "task_type") && this->m_task_type.empty())
    {
        std::cerr << std::format("config_mgr::init [server]task_type is empty") << std::endl;
        return -1;
    }
    if (this->m_ini.has("server", "task_type") && avant::task::str2task_type(this->m_task_type) == avant::task::task_type::NONE)
    {
        std::cerr << std::format("config_mgr::init [server]task_type {} not recognized", this->m_task_type) << std::endl;
        return -1;
    }
    if (this->m_ini.has("server", "http_static_dir") && this->m_http_static_dir.empty())
    {
        std::cerr << std::format("config_mgr::init [server]http_static_dir is empty") << std::endl;
        return -1;
    }
    if (this->m_ini.has("server", "lua_dir") && this->m_lua_dir.empty())
    {
        std::cerr << std::format("config_mgr::init [server]lua_dir is empty") << std::endl;
        return -1;
    }
    if (this->m_ini.has("server", "crt.pem") && this->m_crt_pem.empty() && this->m_use_ssl == 1)
    {
        std::cerr << std::format("config_mgr::init [server]crt.pem is empty") << std::endl;
        return -1;
    }
    if (this->m_ini.has("server", "key.pem") && this->m_key_pem.empty() && this->m_use_ssl == 1)
    {
        std::cerr << std::format("config_mgr::init [server]key.pem is empty") << std::endl;
        return -1;
    }
    if (this->m_use_ssl != 0 && this->m_use_ssl != 1)
    {
        std::cerr << std::format("config_mgr::init [server]use_ssl {} must be 0 or 1", this->m_use_ssl) << std::endl;
        return -1;
    }
    if (this->m_daemon != 0 && this->m_daemon != 1)
    {
        std::cerr << std::format("config_mgr::init [server]daemon {} must be 0 or 1", this->m_daemon) << std::endl;
        return -1;
    }

    if (this->m_log_level < log::logger::DEBUG || this->m_log_level >= log::logger::FLAG_COUNT)
    {
        std::cerr << std::format("config_mgr::init [server]log_level {} out of range", this->m_log_level) << std::endl;
        return -1;
    }
    if (this->m_ini.has("server", "other_udp_svr_ip") && this->m_other_udp_svr_ip.empty())
    {
        std::cerr << std::format("config_mgr::init [server]other_udp_svr_ip is empty") << std::endl;
        return -1;
    }
    if (this->m_other_udp_svr_port < 0 || this->m_other_udp_svr_port > PORT_MAX)
    {
        std::cerr << std::format("config_mgr::init [server]other_udp_svr_port {} out of range", this->m_other_udp_svr_port) << std::endl;
        return -1;
    }
    if (this->m_other_udp_svr_max_loop <= 0)
    {
        std::cerr << std::format("config_mgr::init [server]other_udp_svr_max_loop {} must be > 0", this->m_other_udp_svr_max_loop) << std::endl;
        return -1;
    }
    if (this->m_max_ipc_conn_num <= 0)
    {
        std::cerr << std::format("config_mgr::init [ipc]max_ipc_conn_num {} must be > 0", this->m_max_ipc_conn_num) << std::endl;
        return -1;
    }
    if (this->m_ini.has("ipc", "ipc_json_path") && this->m_ipc_json_path.empty())
    {
        std::cerr << std::format("config_mgr::init [ipc]ipc_json_path is empty") << std::endl;
        return -1;
    }

    return 0;
}
