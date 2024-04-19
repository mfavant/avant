#include <list>
#include <string>
#include <unordered_map>
#include <map>
#include <mutex>
#include <tuple>

namespace avant
{
    namespace ipc
    {
        class tcp_component
        {
        public:
            tcp_component();
            ~tcp_component();

            void on_init(std::unordered_map<std::string, std::pair<std::string, int>> targets);
            int event_loop(std::string ip, int port);

            int send(std::string &id, std::string &message);
            int recv(std::string &id, std::string &message);

        private:
            std::unordered_map<std::string, std::pair<std::string, int>> m_targets;
            std::unordered_map<std::string, std::pair<std::string, int>> m_clients;
            std::map<int, std::string> m_connections_fd2id;
            std::map<std::string, int> m_connections_id2fd;

            std::map<int, uint64_t> m_latest_active;

            std::mutex m_read_lock;
            std::map<std::string, std::tuple<uint64_t, std::string>> m_read_buffers;
            std::mutex m_write_lock;
            std::map<std::string, std::tuple<uint64_t, std::string>> m_write_buffers;

            int m_server_fd{0};
            std::string m_server_ip;
            int m_server_port{0};
        };
    }
}
