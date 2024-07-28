#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <cstring>
#include <iostream>
#include <filesystem>
#include <fcntl.h>
#include <avant-inifile/inifile.h>
#include <avant-log/logger.h>

#include "system/system.h"
#include "utility/singleton.h"
#include "server/server.h"

using avant::utility::singleton;
using namespace avant::server;
using namespace avant::system;
using namespace avant::inifile;
using namespace avant::log;
using namespace std;

int system::init()
{
    // load config ini file
    inifile::inifile *ini = singleton<inifile::inifile>::instance();
    ini->load(get_root_path() + "/config/main.ini");

    const string app_id = (*ini)["server"]["app_id"];
    const string &ip = (*ini)["server"]["ip"];
    const int port = (*ini)["server"]["port"];
    const int worker_cnt = (*ini)["server"]["worker_cnt"];
    const int max_client_cnt = (*ini)["server"]["max_client_cnt"];
    const int epoll_wait_time = (*ini)["server"]["epoll_wait_time"];
    const int accept_per_tick = (*ini)["server"]["accept_per_tick"];
    const string task_type = (*ini)["server"]["task_type"];
    const string http_static_dir = (*ini)["server"]["http_static_dir"];
    const string lua_dir = (*ini)["server"]["lua_dir"];

    const string crt_pem = (*ini)["server"]["crt.pem"];
    const string key_pem = (*ini)["server"]["key.pem"];
    const int use_ssl = (*ini)["server"]["use_ssl"];

    const int daemon = (*ini)["server"]["daemon"];

    // daemon
    if (daemon)
    {
        create_daemon();
    }

    // coredump config
    set_sys_limits();

    // signal config
    signal_conf();

    // logger
    const string log_dir_path = get_root_path() + "/log";
    DIR *dp = opendir(log_dir_path.c_str());
    if (dp == nullptr)
    {
        mkdir(log_dir_path.c_str(), 0755); // create dir
    }
    else
    {
        closedir(dp);
    }
    logger::instance().open(m_root_path + "/log/avant.log");

    // server
    auto m_server = singleton<server::server>::instance();
    m_server->config(app_id,
                     ip,
                     port,
                     worker_cnt,
                     max_client_cnt,
                     epoll_wait_time,
                     accept_per_tick,
                     task_type,
                     http_static_dir,
                     lua_dir,
                     crt_pem,
                     key_pem,
                     use_ssl);

    m_server->start(); // main thread loop
    LOG_ERROR("m_server_start() return");
    return 0;
}

int system::set_sys_limits()
{
    // core dump info
    struct rlimit x;
    int ret = getrlimit(RLIMIT_CORE, &x);
    x.rlim_cur = RLIM_SAVED_MAX;
    x.rlim_max = RLIM_SAVED_MAX;

    ret = setrlimit(RLIMIT_CORE, &x);

    ret = setrlimit(RLIMIT_AS, &x);
    ret = setrlimit(RLIMIT_DATA, &x);
    ret = setrlimit(RLIMIT_FSIZE, &x);
    ret = setrlimit(RLIMIT_NOFILE, &x);

    // ret = getrlimit(RLIMIT_DATA, &x);

    return ret;
}

string system::get_root_path()
{
    if (m_root_path != "")
    {
        return m_root_path;
    }
    char path[1024];
    memset(path, 0, 1024);                            // setting all memory byte to zero
    int cnt = readlink("/proc/self/exe", path, 1024); // system operator
    if (cnt < 0 || cnt >= 1024)
    {
        return "";
    }
    // find last charactor /,change to \0
    for (int i = cnt; i >= 0; --i)
    {
        if (path[i] == '/')
        {
            path[i] = '\0';
            break;
        }
    }
    m_root_path = string(path);
    return m_root_path;
}

void system::signal_conf()
{
    signal(SIGPIPE, SIG_IGN);
    signal(SIGCHLD, SIG_IGN);
    // signal(SIGINT, );
    signal(SIGTERM, &system::signal_term); // to call server::to_stop
    signal(SIGSTOP, &system::signal_term);
    signal(SIGKILL, &system::signal_term);
    signal(SIGUSR1, &system::signal_usr1);
}

void system::create_daemon()
{
    pid_t pid = fork();
    if (pid < 0)
    {
        std::cerr << "server::create_daemon() fork error" << std::endl;
        exit(EXIT_FAILURE);
    }
    if (pid > 0) // father
    {
        exit(EXIT_SUCCESS);
    }
    // child
    setsid();
    int fd = open("/dev/null", O_RDWR);
    if (fd < 0)
    {
        std::cerr << "server::create_daemon() open" << std::endl;
        exit(EXIT_FAILURE);
    }
    dup2(fd, STDIN_FILENO);
    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDERR_FILENO);
    if (fd > 2)
    {
        close(fd);
    }
}

void system::signal_term(int sig)
{
    singleton<server::server>::instance()->to_stop();
}

void system::signal_usr1(int sig)
{
    singleton<server::server>::instance()->cmd_reload();
}