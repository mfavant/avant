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

using namespace avant::utility;
using namespace avant::system;
using namespace avant::inifile;
using namespace avant::log;
using namespace std;

int system::init()
{
    // load config
    inifile::inifile *ini = singleton<inifile::inifile>::instance();
    ini->load(get_root_path() + "/config/main.ini");

    // const string &ip = (*ini)["server"]["ip"];
    // const int port = (*ini)["server"]["port"];
    // const int threads = (*ini)["server"]["threads"];
    // const int max_conn = (*ini)["server"]["max_conn"];
    // const int wait_time = (*ini)["server"]["wait_time"];
    // const int accept_per_tick = (*ini)["server"]["accept_per_tick"];

    // const string task_type = (*ini)["server"]["task_type"];

    // const string http_static_dir = (*ini)["server"]["http_static_dir"];
    // const string lua_dir = (*ini)["server"]["lua_dir"];

    // const int use_ssl = (*ini)["server"]["use_ssl"];
    // const string crt_pem = (*ini)["server"]["crt.pem"];
    // const string key_pem = (*ini)["server"]["key.pem"];
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
    // LOG_ERROR("Listen IP: %s", ip.c_str());
    // auto m_server = singleton<server::server>::instance();
    // m_server->config(ip,
    //                  port,
    //                  threads,
    //                  max_conn,
    //                  accept_per_tick,
    //                  wait_time,
    //                  task_type,
    //                  http_static_dir,
    //                  lua_dir,
    //                  daemon,
    //                  crt_pem,
    //                  key_pem,
    //                  use_ssl);

    // int ret = singleton<hooks::init>::instance()->run();
    // if (ret != 0)
    // {
    //     LOG_ERROR("singleton<hooks::init>::instance()->run() err[%d]", ret);
    //     return;
    // }
    // m_server->start(); // main thread loop
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
    // singleton<server::server>::instance()->to_stop();
}