#pragma once
#include <string>
#include <signal.h>

namespace avant::system
{
    class system
    {
    public:
        system() = default;
        ~system() = default;
        int init();                  // init avant system
        std::string get_root_path(); // get executable file location path
        static void signal_term(int sig);
        static void signal_usr1(int sig);
        static void signal_int(int sig);

    private:
        int set_sys_limits();
        void signal_conf();
        void create_daemon();

    private:
        std::string m_root_path{};
    };
}