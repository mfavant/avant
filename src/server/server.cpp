#include <iostream>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <thread>
#include <avant-log/logger.h>
#include <chrono>
#include <vector>
#include "server/server.h"
#include "socket/socket.h"
#include "utility/singleton.h"
#include "task/task_type.h"
#include "socket/server_socket.h"
#include "utility/time.h"

using namespace std;
using namespace avant::server;
using namespace avant::socket;
using namespace avant::utility;
using namespace avant::task;
using namespace avant::worker;
using namespace avant::socket;

server::server()
{
}

server::~server()
{
    // release SSL_CTX
    if (get_use_ssl() && m_ssl_context)
    {
        SSL_CTX_free(m_ssl_context);
    }
    if (m_main_worker_tunnel)
    {
        delete[] m_main_worker_tunnel;
    }
    if (m_workers)
    {
        delete[] m_workers;
    }
}

void server::set_listen_info(const std::string &ip, int port)
{
    m_ip = ip;
    m_port = port;
}

void server::start()
{
    LOG_ERROR("server::start ...");

    // init OpenSSL CTX
    if (get_use_ssl())
    {
        LOG_ERROR("OpenSSL_version %s", OpenSSL_version(OPENSSL_VERSION));
        LOG_ERROR("SSLeay_version %s", SSLeay_version(SSLEAY_VERSION));
        SSL_library_init();
        SSL_load_error_strings();
        m_ssl_context = SSL_CTX_new(SSLv23_server_method());
        if (!m_ssl_context)
        {
            LOG_ERROR("SSL_CTX_new error");
            return;
        }
        SSL_CTX_set_options(m_ssl_context, SSL_OP_SINGLE_DH_USE);

        std::string crt_pem_path = get_crt_pem();
        int i_ret = SSL_CTX_use_certificate_file(m_ssl_context, crt_pem_path.c_str(), SSL_FILETYPE_PEM);
        if (1 != i_ret)
        {
            LOG_ERROR("SSL_CTX_use_certificate_file error: %s", ERR_error_string(ERR_get_error(), nullptr));
            return;
        }
        std::string key_pem_path = get_key_pem();
        i_ret = SSL_CTX_use_PrivateKey_file(m_ssl_context, key_pem_path.c_str(), SSL_FILETYPE_PEM);
        if (1 != i_ret)
        {
            LOG_ERROR("SSL_CTX_use_PrivateKey_file error: %s", ERR_error_string(ERR_get_error(), nullptr));
            return;
        }
    }

    {
        LOG_ERROR("m_app_id %s", m_app_id.c_str());
        LOG_ERROR("m_ip %s", m_ip.c_str());
        LOG_ERROR("m_port %d", m_port);
        LOG_ERROR("m_worker_cnt %d", m_worker_cnt);
        LOG_ERROR("m_max_client_cnt %d", m_max_client_cnt);
        LOG_ERROR("m_epoll_wait_time %d", m_epoll_wait_time);
        LOG_ERROR("m_accept_per_tick %d", m_accept_per_tick);
        LOG_ERROR("m_http_static_dir %s", m_http_static_dir.c_str());
        LOG_ERROR("m_lua_dir %s", m_lua_dir.c_str());
        LOG_ERROR("m_task_type %s", m_task_type.c_str());
        LOG_ERROR("m_use_ssl %d", (int)m_use_ssl);
        LOG_ERROR("m_crt_pem %s", m_crt_pem.c_str());
        LOG_ERROR("m_key_pem %s", m_key_pem.c_str());
    }

    on_start();
}

void server::set_worker_cnt(size_t worker_cnt)
{
    m_worker_cnt = worker_cnt;
}

void server::set_max_client_cnt(size_t max_client_cnt)
{
    m_max_client_cnt = max_client_cnt;
}

void server::set_epoll_wait_time(size_t epoll_wait_time)
{
    m_epoll_wait_time = epoll_wait_time;
}

void server::set_task_type(std::string task_type)
{
    m_task_type = task_type;
}

void server::set_http_static_dir(std::string http_static_dir)
{
    m_http_static_dir = http_static_dir;
}

const std::string &server::get_http_static_dir()
{
    return m_http_static_dir;
}

void server::set_lua_dir(std::string lua_dir)
{
    m_lua_dir = lua_dir;
}

const std::string &server::get_lua_dir()
{
    return m_lua_dir;
}

task_type server::get_task_type()
{
    return str2task_type(m_task_type);
}

void server::set_use_ssl(bool use_ssl)
{
    m_use_ssl = use_ssl;
}

void server::set_crt_pem(std::string crt_pem)
{
    m_crt_pem = crt_pem;
}

void server::set_key_pem(std::string key_pem)
{
    m_key_pem = key_pem;
}

bool server::get_use_ssl()
{
    return m_use_ssl;
}

std::string server::get_crt_pem()
{
    return m_crt_pem;
}

std::string server::get_key_pem()
{
    return m_key_pem;
}

void server::config(const std::string &app_id,
                    const std::string &ip,
                    int port,
                    size_t worker_cnt,
                    size_t max_client_cnt,
                    size_t epoll_wait_time,
                    size_t accept_per_tick,
                    std::string task_type,
                    std::string http_static_dir,
                    std::string lua_dir,
                    std::string crt_pem /*= ""*/,
                    std::string key_pem /*= ""*/,
                    bool use_ssl /*= false*/)
{
    set_app_id(app_id);
    set_listen_info(ip, port);
    set_worker_cnt(worker_cnt);
    set_max_client_cnt(max_client_cnt);
    set_epoll_wait_time(epoll_wait_time);
    set_accept_per_tick(accept_per_tick);
    set_task_type(task_type);
    set_http_static_dir(http_static_dir);
    set_lua_dir(lua_dir);
    set_crt_pem(crt_pem);
    set_key_pem(key_pem);
    set_use_ssl(use_ssl);
}

bool server::is_stop()
{
    return stop_flag;
}

bool server::on_stop()
{
    if (stop_flag)
    {
        return true; // main process close
    }
    return false;
}

void server::to_stop()
{
    stop_flag = true;
    for (size_t i = 0; i < m_worker_cnt; i++)
    {
        m_workers[i].to_stop = true;
    }
}

SSL_CTX *server::get_ssl_ctx()
{
    return m_ssl_context;
}

void server::on_start()
{
    // time
    utility::time server_time;
    server_time.update();
    uint64_t latest_tick_time = server_time.get_seconds();
    uint64_t gid_seq = 0;

    // main m_epoller
    int iret = 0;
    {
        iret = m_epoller.create(m_max_client_cnt * 2);
        if (iret != 0)
        {
            LOG_ERROR("m_epoller.create(%d) iret[%d]", (m_max_client_cnt * 2), iret);
            stop_flag = true;
            return;
        }
    }

    // main_connection_mgr
    {
        iret = m_main_connection_mgr.init(m_max_client_cnt);
        if (iret != 0)
        {
            LOG_ERROR("m_main_connection_mgr.init failed[%d]", iret);
            stop_flag = true;
            return;
        }
    }

    // m_curr_connection_num
    {
        m_curr_connection_num.reset(new std::atomic<int>(0));
        if (!m_curr_connection_num)
        {
            LOG_ERROR("new std::atomic<int> m_curr_connection_num err");
            stop_flag = true;
            return;
        }
    }

    // main_worker_tunnel
    {
        m_main_worker_tunnel = new (std::nothrow) avant::socket::socket_pair[m_worker_cnt];
        if (!m_main_worker_tunnel)
        {
            LOG_ERROR("new socket_pair err");
            stop_flag = true;
            return;
        }
        // init tunnel
        for (size_t i = 0; i < m_worker_cnt; i++)
        {
            iret = m_main_worker_tunnel[i].init();
            if (iret != 0)
            {
                LOG_ERROR("m_main_worker_tunnel[%d] init failed", i);
                stop_flag = true;
                return;
            }
        }
        for (size_t i = 0; i < m_worker_cnt; i++)
        {
            if (0 != m_epoller.add(m_main_worker_tunnel[i].get_me(), nullptr, EPOLLIN | EPOLLERR, true))
            {
                LOG_ERROR("main_epoller.add m_workers.main_worker_tunnel->get_me() failed %d", errno);
                stop_flag = true;
                return;
            }
            // main alloc connection for tunnel
            {
                iret = m_main_connection_mgr.alloc_connection(m_main_worker_tunnel[i].get_me(), gen_gid(latest_tick_time, ++gid_seq));
                if (iret != 0)
                {
                    LOG_ERROR("m_main_connection_mgr.alloc_connection return %d", iret);
                    stop_flag = true;
                    return;
                }
            }
            m_me_worker_tunnel_fd.insert(m_main_worker_tunnel[i].get_me());
        }
    }

    // worker init
    {
        worker::worker *worker_arr = new worker::worker[m_worker_cnt];
        m_workers = worker_arr;
        if (!worker_arr)
        {
            LOG_ERROR("new worker::worker failed");
            return;
        }
        for (size_t i = 0; i < m_worker_cnt; i++)
        {
            m_workers[i].worker_id = i;
            m_workers[i].curr_connection_num = m_curr_connection_num;
            m_workers[i].main_worker_tunnel = &m_main_worker_tunnel[i];

            connection::connection_mgr *new_connection_mgr = new (std::nothrow) connection::connection_mgr;
            if (!new_connection_mgr)
            {
                LOG_ERROR("new (std::nothrow) connection::connection_mgr failed");
                stop_flag = true;
                return;
            }
            std::shared_ptr<connection::connection_mgr> new_connection_mgr_shared_ptr(new_connection_mgr);
            iret = new_connection_mgr->init(m_max_client_cnt);
            if (iret != 0)
            {
                LOG_ERROR("new_connection_mgr->init failed");
                stop_flag = true;
                return;
            }
            m_workers[i].worker_connection_mgr = new_connection_mgr_shared_ptr;
            iret = m_workers[i].epoller.create(m_max_client_cnt * 2);
            if (iret != 0)
            {
                LOG_ERROR("m_epoller.create(%d) iret[%d]", (m_max_client_cnt * 2), iret);
                stop_flag = true;
                return;
            }

            // tunnel to worker_epoller
            if (0 != m_workers[i].epoller.add(m_workers[i].main_worker_tunnel->get_other(), nullptr, EPOLLIN | EPOLLERR, true))
            {
                LOG_ERROR("m_workers.epoller.add m_workers.main_worker_tunnel->get_other() failed");
                stop_flag = true;
                return;
            }
            // worker alloc connection for tunnel
            iret = m_workers[i].worker_connection_mgr->alloc_connection(m_workers[i].main_worker_tunnel->get_other(), gen_gid(latest_tick_time, ++gid_seq));
            if (iret != 0)
            {
                LOG_ERROR("worker_connection_mgr.alloc_connection return %d", iret);
                stop_flag = true;
                return;
            }
        }
    }

    // worker thread start
    {
        for (size_t i = 0; i < m_worker_cnt; i++)
        {
            std::thread t(std::ref(m_workers[i]));
            t.detach();
        }
    }

    // listen_socket init
    server_socket listen_socket(m_ip, m_port, m_max_client_cnt);
    {
        LOG_ERROR("IP %s PORT %d", m_ip.c_str(), m_port);
        if (0 > listen_socket.get_fd())
        {
            LOG_ERROR("listen_socket failed get_fd() < 0");
            stop_flag = true;
            return;
        }
        if (0 != m_epoller.add(listen_socket.get_fd(), nullptr, EPOLLIN | EPOLLERR, true))
        {
            LOG_ERROR("listen_socket m_epoller add failed");
            stop_flag = true;
            return;
        }
    }

    // main_event_loop begin

    while (true)
    {
        int num = m_epoller.wait(10);
        // time update
        {
            server_time.update();
            uint64_t now_tick_time = server_time.get_seconds();
            if (latest_tick_time != now_tick_time)
            {
                bool flag = true;
                // checking all worker stoped
                for (size_t i = 0; i < m_worker_cnt; i++)
                {
                    if (!m_workers[i].is_stoped)
                    {
                        flag = false;
                    }
                }
                if (flag)
                {
                    break;
                }
            }
            gid_seq = 0;
            latest_tick_time = now_tick_time;
        }

        if (num < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            else
            {
                LOG_ERROR("main epoller.wait return %num errno %d", num, errno);
                break;
            }
        }

        for (int i = 0; i < num; i++)
        {
            int evented_fd = m_epoller.m_events[i].data.fd;
            // listen_fd
            if (evented_fd == listen_socket.get_fd())
            {
                std::vector<int> clients_fd;
                for (size_t loop = 0; loop < m_accept_per_tick; loop++)
                {
                    int new_client_fd = listen_socket.accept();
                    if (new_client_fd < 0)
                    {
                        break;
                    }
                    else
                    {
                        *m_curr_connection_num += 1;
                        clients_fd.push_back(new_client_fd);
                    }
                }
                // new_client_fd come here
                if (!clients_fd.empty())
                {
                    LOG_ERROR("new client coming");
                    for (int fd : clients_fd)
                    {
                        ::close(fd);
                        *m_curr_connection_num -= 1;
                    }
                }
            }
            // worker_tunnel_fd
            else if (m_me_worker_tunnel_fd.find(evented_fd) == m_me_worker_tunnel_fd.end())
            {
                LOG_ERROR("main work_tunnel_fd coming");
            }
            else
            {
                LOG_ERROR("main epoller, undefined type fd");
                m_epoller.del(evented_fd, nullptr, 0);
                ::close(evented_fd);
            }
        }
    }
}

uint64_t server::gen_gid(uint64_t time_seconds, uint64_t gid_seq)
{
    return (time_seconds << 32) | gid_seq;
}