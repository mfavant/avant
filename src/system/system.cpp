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
#include "system/config_mgr.h"

using avant::utility::singleton;
using namespace avant::server;
using namespace avant::system;
using namespace avant::inifile;
using namespace avant::log;
using std::string;

int system::init()
{
    // load config ini file
    singleton<config_mgr>::instance()->set_root_path(get_root_path());
    if (0 != singleton<config_mgr>::instance()->init(get_root_path() + "/config/main.ini"))
    {
        std::cerr << "system::init() config_mgr init error" << std::endl;
        return -1;
    }

    // daemon
    if (singleton<config_mgr>::instance()->get_daemon())
    {
        create_daemon();
    }

    // coredump config
    set_sys_limits();

    // signal config
    signal_conf();

    // logger
    const string log_dir_path = singleton<config_mgr>::instance()->get_root_path() + "/log";
    DIR *dp = opendir(log_dir_path.c_str());
    if (dp == nullptr)
    {
        mkdir(log_dir_path.c_str(), 0755); // create dir
    }
    else
    {
        closedir(dp);
    }
    logger::instance().open(m_root_path + "/log/", singleton<config_mgr>::instance()->get_log_level());

    // server
    auto m_server = singleton<server::server>::instance();
    m_server->config(singleton<config_mgr>::instance());

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
    signal(SIGINT, &system::signal_int);
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

void system::signal_int(int sig)
{
    inifile::inifile *ini = singleton<inifile::inifile>::instance();
    const int daemon = (*ini)["server"]["daemon"];

    if (!daemon)
    {
        singleton<server::server>::instance()->to_stop();
    }
}