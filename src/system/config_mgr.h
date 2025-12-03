#pragma once
#include <string>
#include <avant-inifile/inifile.h>

namespace avant::system
{
    class config_mgr
    {
    public:
        config_mgr();
        ~config_mgr();

        // Load configuration from a file
        int init(const std::string &file_path);

        const std::string get_app_id() const { return m_app_id; }
        const std::string get_ip() const { return m_ip; }
        const int get_port() const { return m_port; }
        const int get_worker_cnt() const { return m_worker_cnt; }
        const int get_max_client_cnt() const { return m_max_client_cnt; }
        const int get_epoll_wait_time() const { return m_epoll_wait_time; }
        const int get_accept_per_tick() const { return m_accept_per_tick; }
        const std::string get_task_type() const { return m_task_type; }
        const std::string get_http_static_dir() const { return m_http_static_dir; }
        const std::string get_lua_dir() const { return m_lua_dir; }
        const std::string get_crt_pem() const { return m_crt_pem; }
        const std::string get_key_pem() const { return m_key_pem; }
        const int get_use_ssl() const { return m_use_ssl; }
        const int get_daemon() const { return m_daemon; }
        const int get_log_level() const { return m_log_level; }
        const int get_max_ipc_conn_num() const { return m_max_ipc_conn_num; }
        const std::string get_ipc_json_path() const { return m_ipc_json_path; }
        const std::string get_other_udp_svr_ip() const { return m_other_udp_svr_ip; }
        const int get_other_udp_svr_port() const { return m_other_udp_svr_port; }
        const int get_other_udp_svr_max_loop() const { return m_other_udp_svr_max_loop; }

        void set_root_path(std::string val)
        {
            m_root_path = val;
        }
        const std::string get_root_path() { return m_root_path; }

    private:
        std::string m_app_id;
        std::string m_ip;
        int m_port;
        int m_worker_cnt;
        int m_max_client_cnt;
        int m_epoll_wait_time;
        int m_accept_per_tick;
        std::string m_task_type;
        std::string m_http_static_dir;
        std::string m_lua_dir;
        std::string m_crt_pem;
        std::string m_key_pem;
        int m_use_ssl;
        int m_daemon;
        int m_log_level;
        int m_max_ipc_conn_num;
        std::string m_ipc_json_path;
        std::string m_other_udp_svr_ip;
        int m_other_udp_svr_port;
        int m_other_udp_svr_max_loop;

        avant::inifile::inifile *m_ini;
        std::string m_root_path;
    };
}
