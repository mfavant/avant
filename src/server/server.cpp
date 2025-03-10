#include <iostream>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <thread>
#include <avant-log/logger.h>
#include <chrono>
#include "server/server.h"
#include "socket/socket.h"
#include "utility/singleton.h"
#include "task/task_type.h"
#include "socket/server_socket.h"
#include "hooks/tick.h"
#include "hooks/init.h"
#include "hooks/stop.h"
#include "hooks/reload.h"
#include "connection/http_ctx.h"
#include <unordered_set>
#include "global/tunnel_id.h"
#include "proto/proto_util.h"
#include "connection/websocket_ctx.h"
#include <filesystem>
#include <sstream>
#include <fstream>
#include <stdexcept>
#include <cmath>

using namespace std;
using namespace avant::server;
using namespace avant::socket;
using namespace avant::utility;
using namespace avant::task;
using namespace avant::workers;
using namespace avant::socket;
using namespace avant::global;
using namespace avant::proto;

server::server()
{
}

server::~server()
{
    // release SSL_CTX
    if (get_use_ssl() && m_ssl_context)
    {
        SSL_CTX_free(m_ssl_context);
        m_ssl_context = nullptr;
    }

    if (m_main_worker_tunnel)
    {
        delete[] m_main_worker_tunnel;
        m_main_worker_tunnel = nullptr;
    }
    if (m_workers)
    {
        delete[] m_workers;
        m_workers = nullptr;
    }
    if (m_other)
    {
        delete m_other;
        m_other = nullptr;
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

    int i_ret = tunnel_id::init(m_worker_cnt);
    if (0 != i_ret)
    {
        LOG_ERROR("avant::global::tunnel_id::init(%llu) failed return %d", m_worker_cnt, i_ret);
        return;
    }

    if (m_max_client_cnt <= 0 || m_max_client_cnt > 8388607)
    {
        LOG_ERROR("m_max_client_cnt <= 0 || m_max_client_cnt > 8388607 %d", m_max_client_cnt);
        return;
    }

    if (m_worker_cnt <= 0 || m_worker_cnt > 511)
    {
        LOG_ERROR("m_worker_cnt <= 0 || m_worker_cnt > 511 %d", m_worker_cnt);
        return;
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

void server::set_max_ipc_conn_num(size_t max_ipc_conn_num)
{
    this->m_max_ipc_conn_num = max_ipc_conn_num;
}

void server::set_ipc_json_path(std::string ipc_json_path)
{
    this->m_ipc_json_path = ipc_json_path;
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
                    size_t max_ipc_conn_num,
                    std::string ipc_json_path,
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
    set_max_ipc_conn_num(max_ipc_conn_num);
    set_ipc_json_path(ipc_json_path);
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
    if (stop_flag)
    {
        return;
    }
    // main thread stop_flag
    stop_flag = true;
    // worker thread stop_flag
    for (size_t i = 0; i < m_worker_cnt; i++)
    {
        m_workers[i].to_stop = true;
    }
}

SSL_CTX *server::get_ssl_ctx()
{
    return m_ssl_context;
}

void server::on_start_load_ipc_json_file()
{
    // load ipc json
    {
        const std::filesystem::path file_path(this->m_ipc_json_path);
        std::ifstream file_stream(file_path);
        if (!file_stream)
        {
            LOG_ERROR("could not open file %d", this->m_ipc_json_path.c_str());
            throw std::runtime_error(std::string("could not open file ") + this->m_ipc_json_path);
        }
        std::stringstream buffer;
        buffer << file_stream.rdbuf();
        this->m_ipc_json.parse(buffer.str());
        file_stream.close();
    }

    // Print ipc.json file content using LOG
    auto json_iter = this->m_ipc_json.begin();
    int counter = 0;
    for (; json_iter != this->m_ipc_json.end(); json_iter++)
    {
        auto &json_item = *json_iter;
        std::string app_id = json_item["app_id"].as_string();
        std::string ip = json_item["ip"].as_string();
        int port = json_item["port"].as_integer();
        LOG_ERROR("IPC JSON %d app_id[%s] ip[%s] port[%d]", counter++, app_id.c_str(), ip.c_str(), port);
    }
}

void server::on_start()
{
    // load ipc json
    on_start_load_ipc_json_file();

    // time
    this->m_server_loop_time.update();
    this->m_latest_tick_time = this->m_server_loop_time.get_seconds();
    this->m_gid_seq = 0;

    // main m_epoller
    int iret = 0;
    {
        iret = m_epoller.create(m_max_client_cnt + 10);
        if (iret != 0)
        {
            LOG_ERROR("m_epoller.create(%d) iret[%d]", (m_max_client_cnt + 10), iret);
            return;
        }
    }

    // main_connection_mgr
    {
        iret = m_main_connection_mgr.init(m_worker_cnt * 4);
        if (iret != 0)
        {
            LOG_ERROR("m_main_connection_mgr.init(%d) failed[%d]", (m_worker_cnt * 4), iret);
            return;
        }
    }

    // m_curr_connection_num
    {
        m_curr_connection_num.reset(new std::atomic<int>(0));
        if (!m_curr_connection_num)
        {
            LOG_ERROR("new std::atomic<int> m_curr_connection_num err");
            return;
        }
    }

    // main_worker_tunnel
    {
        m_main_worker_tunnel = new (std::nothrow) avant::socket::socket_pair[m_worker_cnt];
        if (!m_main_worker_tunnel)
        {
            LOG_ERROR("new socket_pair err");
            return;
        }
        // init tunnel
        for (size_t i = 0; i < m_worker_cnt; i++)
        {
            iret = m_main_worker_tunnel[i].init();
            if (iret != 0)
            {
                LOG_ERROR("m_main_worker_tunnel[%d] init failed", i);
                return;
            }
        }
        for (size_t i = 0; i < m_worker_cnt; i++)
        {
            if (0 != m_epoller.add(m_main_worker_tunnel[i].get_me(), nullptr, event::event_poller::RWE, false))
            {
                LOG_ERROR("main_epoller.add m_workers.main_worker_tunnel->get_me() failed %d", errno);
                return;
            }
            // main alloc connection for tunnel
            {
                iret = m_main_connection_mgr.alloc_connection(m_main_worker_tunnel[i].get_me(), server_gen_gid());
                if (iret != 0)
                {
                    LOG_ERROR("m_main_connection_mgr.alloc_connection for m_main_worker_tunnel return %d", iret);
                    return;
                }
                connection::connection *tunnel_conn = m_main_connection_mgr.get_conn(m_main_worker_tunnel[i].get_me());
                tunnel_conn->recv_buffer.reserve(10485760); // 10MB
                tunnel_conn->send_buffer.reserve(10485760); // 10MB
                tunnel_conn->is_ready = true;
            }
            m_main_worker_tunnel_fd2index.emplace(m_main_worker_tunnel[i].get_me(), i);
        }
    }

    // worker init
    {
        workers::worker *worker_arr = new workers::worker[m_worker_cnt];
        m_workers = worker_arr;
        if (!worker_arr)
        {
            LOG_ERROR("new worker::worker failed");
            return;
        }

        uint64_t worker_max_client_cnt = std::ceil((double)m_max_client_cnt / (double)m_worker_cnt) + m_worker_cnt;

        worker::worker::http_static_dir = m_http_static_dir;
        for (size_t i = 0; i < m_worker_cnt; i++)
        {
            m_workers[i].worker_id = i;
            m_workers[i].curr_connection_num = m_curr_connection_num;
            m_workers[i].worker_connection_num.store(0);
            m_workers[i].main_worker_tunnel = &m_main_worker_tunnel[i];
            m_workers[i].epoll_wait_time = m_epoll_wait_time;
            m_workers[i].use_ssl = m_use_ssl;
            m_workers[i].ssl_context = m_ssl_context;
            m_workers[i].type = avant::task::str2task_type(m_task_type);

            connection::connection_mgr *new_connection_mgr = new (std::nothrow) connection::connection_mgr;
            if (!new_connection_mgr)
            {
                LOG_ERROR("new (std::nothrow) connection::connection_mgr failed");
                return;
            }
            std::shared_ptr<connection::connection_mgr> new_connection_mgr_shared_ptr(new_connection_mgr);

            iret = new_connection_mgr->init(worker_max_client_cnt);
            if (iret != 0)
            {
                LOG_ERROR("new_connection_mgr->init(%d) failed", worker_max_client_cnt);
                return;
            }
            m_workers[i].worker_connection_mgr = new_connection_mgr_shared_ptr;
            iret = m_workers[i].epoller.create(worker_max_client_cnt);
            if (iret != 0)
            {
                LOG_ERROR("m_epoller.create(%d) iret[%d]", worker_max_client_cnt, iret);
                return;
            }

            // tunnel to worker_epoller
            if (0 != m_workers[i].epoller.add(m_workers[i].main_worker_tunnel->get_other(), nullptr, event::event_poller::RWE, false))
            {
                LOG_ERROR("m_workers.epoller.add m_workers.main_worker_tunnel->get_other() failed");
                return;
            }
            // worker alloc connection for tunnel
            iret = m_workers[i].worker_connection_mgr->alloc_connection(m_workers[i].main_worker_tunnel->get_other(), server_gen_gid());
            if (iret != 0)
            {
                LOG_ERROR("worker_connection_mgr.alloc_connection return %d", iret);
                return;
            }
            connection::connection *tunnel_conn = m_workers[i].worker_connection_mgr->get_conn(m_workers[i].main_worker_tunnel->get_other());
            tunnel_conn->recv_buffer.reserve(10485760); // 10MB
            tunnel_conn->send_buffer.reserve(10485760); // 10MB
            tunnel_conn->is_ready = true;
        }
    }

    // m_main_other_tunnel
    {
        iret = m_main_other_tunnel.init();
        if (iret != 0)
        {
            LOG_ERROR("main m_main_other_tunnel failed iret=%d", iret);
            return;
        }
        if (0 != m_epoller.add(m_main_other_tunnel.get_me(), nullptr, event::event_poller::RWE, false))
        {
            LOG_ERROR("main_epoller.add m_main_other_tunnel.get_me() failed %d", errno);
            return;
        }
        // main alloc connection for tunnel
        {
            iret = m_main_connection_mgr.alloc_connection(m_main_other_tunnel.get_me(), server_gen_gid());
            if (iret != 0)
            {
                LOG_ERROR("m_main_connection_mgr.alloc_connection for m_main_other_tunnel return %d", iret);
                return;
            }
            connection::connection *tunnel_conn = m_main_connection_mgr.get_conn(m_main_other_tunnel.get_me());
            tunnel_conn->recv_buffer.reserve(10485760); // 10MB
            tunnel_conn->send_buffer.reserve(10485760); // 10MB
            tunnel_conn->is_ready = true;
        }
    }

    // other init
    {
        workers::other *other_ptr = new workers::other;
        m_other = other_ptr;
        if (!m_other)
        {
            LOG_ERROR("new workers::other failed");
            return;
        }
        m_other->max_ipc_conn_num = this->m_max_ipc_conn_num;
        m_other->epoll_wait_time = this->m_epoll_wait_time;
        m_other->main_other_tunnel = &m_main_other_tunnel;
        m_other->ipc_json = this->m_ipc_json;
        m_other->app_id = this->m_app_id;
        m_other->accept_per_tick = this->m_accept_per_tick;

        connection::connection_mgr *new_connection_mgr = new (std::nothrow) connection::connection_mgr;
        if (!new_connection_mgr)
        {
            LOG_ERROR("new (std::nothrow) connection::connection_mgr failed");
            return;
        }

        std::shared_ptr<connection::connection_mgr> new_connection_mgr_shared_ptr(new_connection_mgr);
        iret = new_connection_mgr->init(m_other->max_ipc_conn_num);
        if (iret != 0)
        {
            LOG_ERROR("new_connection_mgr->init failed");
            return;
        }
        m_other->ipc_connection_mgr = new_connection_mgr_shared_ptr;
        iret = m_other->epoller.create(m_other->max_ipc_conn_num);
        if (iret != 0)
        {
            LOG_ERROR("m_epoller.create(%d) iret[%d]", (m_other->max_ipc_conn_num), iret);
            return;
        }

        // tunnel to other_epoller
        if (0 != m_other->epoller.add(m_other->main_other_tunnel->get_other(), nullptr, event::event_poller::RWE, false))
        {
            LOG_ERROR("m_other->epoller.add(m_other->main_other_tunnel->get_other() failed");
            return;
        }

        // other alloc connection for tunnel
        iret = m_other->ipc_connection_mgr->alloc_connection(m_other->main_other_tunnel->get_other(), 0);
        if (iret != 0)
        {
            LOG_ERROR("ipc_connection_mgr->alloc_connection return %d", iret);
            return;
        }
        connection::connection *tunnel_conn = m_other->ipc_connection_mgr->get_conn(m_other->main_other_tunnel->get_other());
        tunnel_conn->recv_buffer.reserve(10485760); // 10MB
        tunnel_conn->send_buffer.reserve(10485760); // 10MB
        tunnel_conn->is_ready = true;

        iret = m_other->init_call_by_server();
        if (iret != 0)
        {
            LOG_ERROR("m_other->init_call_by_server() return %d", iret);
            return;
        }
    }

    // listen_socket init
    {
        server_socket *listen_socket = new (std::nothrow) server_socket(m_ip, m_port, m_max_client_cnt);
        if (!listen_socket)
        {
            LOG_ERROR("new listen socket object failed");
            return;
        }
        this->m_server_listen_socket.reset(listen_socket);

        LOG_ERROR("IP %s PORT %d", m_ip.c_str(), m_port);
        if (0 > this->m_server_listen_socket->get_fd())
        {
            LOG_ERROR("listen_socket failed get_fd() < 0");
            return;
        }
        if (0 != m_epoller.add(this->m_server_listen_socket->get_fd(), nullptr, event::event_poller::RWE, false))
        {
            LOG_ERROR("listen_socket m_epoller add failed");
            return;
        }
    }

    // http-parser setting
    {
        connection::http_ctx::init_http_settings();
    }
    // ws-http-parser setting
    {
        connection::websocket_ctx::init_ws_http_settings();
    }

    // main_event_loop begin
    {
        hooks::init::on_main_init(*this);
    }
    // worker thread start
    {
        for (size_t i = 0; i < m_worker_cnt; i++)
        {
            std::thread t(std::ref(m_workers[i]));
            t.detach();
        }
    }
    // other thread start
    {
        std::thread t(std::ref(*m_other));
        t.detach();
    }

    // main event_loop
    {
        while (true)
        {
            int num = m_epoller.wait(m_epoll_wait_time);

            if (num < 0)
            {
                if (errno == EINTR)
                {
                    continue;
                }
                else
                {
                    LOG_ERROR("main epoller.wait return %d errno %d %s", num, errno, strerror(errno));
                    to_stop();
                    continue;
                }
            }

            // time update
            {
                hooks::tick::on_main_tick(*this); // maybe change errno
                this->m_server_loop_time.update();
                uint64_t now_tick_time = this->m_server_loop_time.get_seconds();
                if (this->m_latest_tick_time != now_tick_time)
                {
                    // int curr_connection_num = m_curr_connection_num->load();
                    // LOG_ERROR("curr_connection_num %d", curr_connection_num);

                    if (stop_flag)
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
                            m_other->to_stop = true;
                        }
                        // checking other thread stoped
                        if (!m_other->is_stoped)
                        {
                            flag = false;
                        }
                        if (flag)
                        {
                            break;
                        }
                    }
                    this->m_gid_seq = 0;
                    this->m_latest_tick_time = now_tick_time;
                }
            }

            for (int i = 0; i < num; i++)
            {
                int evented_fd = m_epoller.m_events[i].data.fd;
                uint32_t event_come = m_epoller.m_events[i].events;
                // listen_fd
                if (evented_fd == this->m_server_listen_socket->get_fd())
                {
                    std::vector<int> clients_fd;
                    std::vector<uint64_t> gids;
                    for (size_t loop = 0; loop < m_accept_per_tick; loop++)
                    {
                        int new_client_fd = this->m_server_listen_socket->accept();
                        if (new_client_fd < 0)
                        {
                            break;
                        }
                        else
                        {
                            if (m_curr_connection_num->load() >= (int)m_max_client_cnt)
                            {
                                LOG_ERROR("m_curr_connection_num >= m_max_client_cnt");
                                ::close(new_client_fd);
                                break;
                            }
                            m_curr_connection_num->fetch_add(1);
                            clients_fd.push_back(new_client_fd);
                            gids.push_back(server_gen_gid());
                        }
                    }
                    // new_client_fd come here
                    if (!clients_fd.empty())
                    {
                        on_listen_event(clients_fd, gids);
                    }
                }
                // worker_tunnel_fd
                else if (m_main_worker_tunnel_fd2index.find(evented_fd) != m_main_worker_tunnel_fd2index.end())
                {
                    on_tunnel_event(m_main_worker_tunnel[m_main_worker_tunnel_fd2index[evented_fd]], event_come);
                }
                // main_other tunnel
                else if (m_main_other_tunnel.get_me() == evented_fd)
                {
                    on_tunnel_event(m_main_other_tunnel, event_come);
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

    // main event_loop stoped
    {
        hooks::stop::on_main_stop(*this);
    }
}

// 32bit(timestamp) 23bit(m_gid_seq) 9bit(worker_idx)
uint64_t server::server_gen_gid()
{
    uint64_t gen_gid_seq = this->m_gid_seq++;
    gen_gid_seq = gen_gid_seq & 0x7FFFFF;
    gen_gid_seq = gen_gid_seq << 9;

    return (this->m_latest_tick_time << 32) | gen_gid_seq;
}

void server::on_listen_event(std::vector<int> vec_new_client_fd, std::vector<uint64_t> vec_gid)
{
    for (size_t loop = 0; loop < vec_new_client_fd.size(); ++loop)
    {
        const int &new_client_fd = vec_new_client_fd[loop];
        const uint64_t &gid = vec_gid[loop];

        uint64_t target_worker_idx = 0;
        int worker_connection_num = 0;

        for (uint64_t worker_idx = 0; worker_idx < this->m_worker_cnt; worker_idx++)
        {
            if (target_worker_idx == worker_idx)
            {
                worker_connection_num = m_workers[worker_idx].worker_connection_num.load();
                continue;
            }
            int temp_val = m_workers[worker_idx].worker_connection_num.load();
            if (temp_val <= worker_connection_num)
            {
                worker_connection_num = temp_val;
                target_worker_idx = worker_idx;
            }
        }

        target_worker_idx = target_worker_idx & 0x1FF; // 9 bits

        m_workers[target_worker_idx].worker_connection_num.fetch_add(1);

        int target_worker_tunnel_id = tunnel_id::get().get_worker_tunnel_id(target_worker_idx);

        ProtoTunnelMain2WorkerNewClient main2worker_new_client;
        main2worker_new_client.set_fd(new_client_fd);
        uint64_t real_gid = gid | target_worker_idx;
        main2worker_new_client.set_gid(real_gid);

        ProtoPackage need_forward_package;
        proto::pack_package(need_forward_package, main2worker_new_client, ProtoCmd::PROTO_CMD_TUNNEL_MAIN2WORKER_NEW_CLIENT);
        // LOG_ERROR("need_forward_package %d", need_forward_package.cmd());

        int forward_ret = tunnel_forward(tunnel_id::get().get_main_tunnel_id(), target_worker_tunnel_id, need_forward_package);

        if (0 != forward_ret)
        {
            LOG_ERROR("tunnel_forward err %d target_worker_tunnel_id %d", forward_ret, target_worker_tunnel_id);
            ::close(new_client_fd);
            m_curr_connection_num->fetch_sub(1);
        }
    }
}

void server::on_tunnel_event(avant::socket::socket_pair &tunnel, uint32_t event)
{
    // find conn
    connection::connection *tunnel_conn = m_main_connection_mgr.get_conn(tunnel.get_me());
    if (!tunnel_conn)
    {
        LOG_ERROR("on_tunnel_event tunnel_conn is null, fd[%d]", tunnel.get_me());
        return;
    }

    avant::socket::socket &sock = tunnel.get_me_socket();

    // check if there is any content that needs to be read
    if (event & event::event_poller::READ)
    {
        constexpr int buffer_size = 1024000;
        char buffer[buffer_size]{0};
        int buffer_used_idx{0};

        while (buffer_used_idx < buffer_size)
        {
            int len = 0;
            int oper_errno = 0;
            len = sock.recv(buffer + buffer_used_idx, buffer_size - buffer_used_idx, oper_errno);
            if (len > 0)
            {
                buffer_used_idx += len;
            }
            else
            {
                if (oper_errno != EAGAIN && oper_errno != EINTR && oper_errno != EWOULDBLOCK)
                {
                    LOG_ERROR("on_tunnel_event tunnel_conn oper_errno %d", oper_errno);
                    to_stop();
                }
                break;
            }
        }
        if (buffer_used_idx > 0)
        {
            tunnel_conn->recv_buffer.append(buffer, buffer_used_idx);
        }

        // parser protocol
        while (!tunnel_conn->recv_buffer.empty())
        {
            uint64_t data_size = 0;
            if (tunnel_conn->recv_buffer.size() >= sizeof(data_size))
            {
                data_size = *((uint64_t *)tunnel_conn->recv_buffer.get_read_ptr());
                data_size = avant::proto::toh64(data_size);
                if (data_size + sizeof(data_size) > tunnel_conn->recv_buffer.size())
                {
                    break;
                }
            }
            else
            {
                break;
            }

            if (data_size == 0)
            {
                tunnel_conn->recv_buffer.move_read_ptr_n(sizeof(data_size));
                break;
            }

            ProtoPackage protoPackage;
            if (!protoPackage.ParseFromArray(tunnel_conn->recv_buffer.get_read_ptr() + sizeof(data_size), data_size))
            {
                LOG_ERROR("server parseFromArray err %llu", data_size);
                tunnel_conn->recv_buffer.move_read_ptr_n(sizeof(data_size) + data_size);
                break;
            }

            on_tunnel_process(protoPackage);
            tunnel_conn->recv_buffer.move_read_ptr_n(sizeof(data_size) + data_size);
        }
    }

    // check if there is any content that needs to be sent
    while (event & event::event_poller::WRITE)
    {
        try_send_flush_tunnel(tunnel);
        break; // important
    }
}

void server::on_tunnel_process(ProtoPackage &message)
{
    auto cmd = message.cmd();
    if (cmd != ProtoCmd::PROTO_CMD_TUNNEL_PACKAGE)
    {
        LOG_ERROR("cmd %d != ProtoCmd::PROTO_CMD_TUNNEL_PACKAGE", cmd);
        return;
    }

    ProtoTunnelPackage tunnelPackage;
    if (!tunnelPackage.ParseFromString(message.protocol()))
    {
        LOG_ERROR("tunnelPackage.ParseFromString(message.protocol()) err");
        return;
    }

    auto source = tunnelPackage.sourcetunnelsid();
    auto &target = tunnelPackage.targettunnelsid();

    // count all targets
    std::unordered_set<int> all_target;

    // from other tunnel
    if (source == tunnel_id::get().get_other_tunnel_id())
    {
        // send to all worker
        if (target.empty())
        {
            for (int i = tunnel_id::get().get_worker_tunnel_id_min(); i <= tunnel_id::get().get_worker_tunnel_id_max(); i++)
            {
                all_target.insert(i);
            }
        }
        else // send to special worker
        {
            for (int i = 0; i < target.size(); i++)
            {
                if (target.Get(i) == tunnel_id::get().get_main_tunnel_id())
                {
                    continue;
                }
                if (target.Get(i) >= tunnel_id::get().get_worker_tunnel_id_min() && target.Get(i) <= tunnel_id::get().get_worker_tunnel_id_max())
                {
                    all_target.insert(target.Get(i));
                    continue;
                }
                LOG_ERROR("undefine logic");
            }
        }
    }
    // from worker
    else if (source >= tunnel_id::get().get_worker_tunnel_id_min() && source <= tunnel_id::get().get_worker_tunnel_id_max())
    {
        // send to all worker exclude other
        if (target.empty())
        {
            // include from_worker self
            for (int i = tunnel_id::get().get_worker_tunnel_id_min(); i <= tunnel_id::get().get_worker_tunnel_id_max(); i++)
            {
                all_target.insert(i);
            }
        }
        else
        {
            for (int i = 0; i < target.size(); i++)
            {
                if (target.Get(i) == tunnel_id::get().get_main_tunnel_id())
                {
                    continue;
                }
                if (target.Get(i) == tunnel_id::get().get_other_tunnel_id())
                {
                    all_target.insert(target.Get(i));
                    continue;
                }
                if (target.Get(i) >= tunnel_id::get().get_worker_tunnel_id_min() && target.Get(i) <= tunnel_id::get().get_worker_tunnel_id_max())
                {
                    all_target.insert(target.Get(i));
                    continue;
                }
                LOG_ERROR("undefine logic");
            }
        }
    }
    // from undefine
    else
    {
        LOG_ERROR("server::on_tunnel_process source logic undefine");
        return;
    }

    for (int target_tunnel_id : all_target)
    {
        tunnel_forward(source, target_tunnel_id, *tunnelPackage.mutable_innerprotopackage());
    }
}

void server::try_send_flush_tunnel(avant::socket::socket_pair &tunnel)
{
    // LOG_ERROR("try_send_flush_tunnel %d", tunnel.get_me());
    // find conn
    connection::connection *tunnel_conn = m_main_connection_mgr.get_conn(tunnel.get_me());
    if (!tunnel_conn)
    {
        LOG_ERROR("try_send_flush_tunnel tunnel_conn is null, fd[%d]", tunnel.get_me());
        return;
    }

    avant::socket::socket &sock = tunnel.get_me_socket();

    if (tunnel_conn->send_buffer.empty())
    {
        m_epoller.mod(sock.get_fd(), nullptr, event::event_poller::RE, false);
        return;
    }

    while (!tunnel_conn->send_buffer.empty())
    {
        int oper_errno = 0;
        int len = sock.send(tunnel_conn->send_buffer.get_read_ptr(), tunnel_conn->send_buffer.size(), oper_errno);
        if (len > 0)
        {
            // LOG_ERROR("flush bytes %d", len);
            tunnel_conn->send_buffer.move_read_ptr_n(len);
        }
        else
        {
            if (oper_errno != EAGAIN && oper_errno != EINTR && oper_errno != EWOULDBLOCK)
            {
                LOG_ERROR("try_send_flush_tunnel tunnel_conn oper_errno %d", oper_errno);
                to_stop();
            }
            else
            {
                m_epoller.mod(sock.get_fd(), nullptr, event::event_poller::RWE, false);
            }
            break;
        }
    }
}

int server::tunnel_forward(int source_tunnelid, int dest_tunnel_id, ProtoPackage &message, bool flush /*= true*/)
{
    if (!tunnel_id::get().is_tunnel_id(source_tunnelid))
    {
        LOG_ERROR("source_tunnel_id %d is not tunnel_id message cmd %d", source_tunnelid, message.cmd());
        return -1;
    }
    if (!tunnel_id::get().is_tunnel_id(dest_tunnel_id))
    {
        LOG_ERROR("dest_tunnel_id %d is not tunnel_id message cmd %d", dest_tunnel_id, message.cmd());
        return -2;
    }

    // find target tunnel
    connection::connection *dest_tunnel_conn_ptr = nullptr;

    if (tunnel_id::get().is_worker_tunnel_id(dest_tunnel_id))
    {
        dest_tunnel_conn_ptr = get_main2worker_tunnel(dest_tunnel_id);
    }
    else if (tunnel_id::get().get_other_tunnel_id() == dest_tunnel_id)
    {
        dest_tunnel_conn_ptr = get_main2other_tunnel();
    }

    if (!dest_tunnel_conn_ptr)
    {
        LOG_ERROR("dest_tunnel_id %d connection not be found", dest_tunnel_id);
        return -3;
    }

    ProtoTunnelPackage package;
    package.set_sourcetunnelsid(source_tunnelid);
    package.add_targettunnelsid(dest_tunnel_id);
    package.mutable_innerprotopackage()->CopyFrom(message);

    std::string data;
    proto::pack_package(data, package, ProtoCmd::PROTO_CMD_TUNNEL_PACKAGE);
    // LOG_ERROR("forward datasize %llu cmd %d", data.size(), package.innerprotopackage().cmd());

    dest_tunnel_conn_ptr->send_buffer.append(data.c_str(), data.size());
    if (flush)
    {
        // worker_tunnel_fd
        if (m_main_worker_tunnel_fd2index.find(dest_tunnel_conn_ptr->fd) != m_main_worker_tunnel_fd2index.end())
        {
            // LOG_ERROR("main to worker");
            try_send_flush_tunnel(m_main_worker_tunnel[m_main_worker_tunnel_fd2index[dest_tunnel_conn_ptr->fd]]);
        }
        // main_other tunnel
        else if (m_main_other_tunnel.get_me() == dest_tunnel_conn_ptr->fd)
        {
            // LOG_ERROR("main to other");
            try_send_flush_tunnel(m_main_other_tunnel);
        }
        else
        {
            LOG_ERROR("flush error know fd %d, it's not worker and other tunnel fd", dest_tunnel_conn_ptr->fd);
            return -4;
        }
    }
    else
    {
        m_epoller.mod(dest_tunnel_conn_ptr->fd, nullptr, event::event_poller::RWE, false);
    }
    return 0;
}

avant::connection::connection *server::get_main2worker_tunnel(int worker_tunnel_id)
{
    if (!tunnel_id::get().is_worker_tunnel_id(worker_tunnel_id))
    {
        return nullptr;
    }
    return m_main_connection_mgr.get_conn(m_main_worker_tunnel[worker_tunnel_id].get_me());
}

avant::connection::connection *server::get_main2other_tunnel()
{
    return m_main_connection_mgr.get_conn(m_main_other_tunnel.get_me());
}

void server::cmd_reload()
{
    hooks::reload::on_cmd_reload(*this);
}