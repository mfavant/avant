#pragma once
#include "utility/vec_str_buffer.h"
#include "socket/socket.h"
#include <cstdint>
#include <memory>
#include <deque>
#include <chrono>

namespace avant::connection
{
    class base_ctx;

    class connection
    {
    private:
        class traffic_data
        {
        public:
            traffic_data(const std::chrono::time_point<std::chrono::steady_clock> &timestamp, size_t bytes);
            ~traffic_data() = default;

            const std::chrono::time_point<std::chrono::steady_clock> &get_timestamp() const
            {
                return this->timestamp;
            }
            size_t bytes{0};
            std::chrono::time_point<std::chrono::steady_clock> timestamp{};
            static constexpr size_t max_traffic_data_seconds = 60;
        };

    public:
        connection();
        ~connection();

        void on_alloc(int fd, uint64_t gid);
        void on_release();

    public:
        avant::utility::vec_str_buffer &get_recv_buffer();
        avant::utility::vec_str_buffer &get_send_buffer();
        int get_fd();
        void set_fd(int fd);
        uint64_t get_gid() const;
        avant::socket::socket &get_socket_obj();
        std::shared_ptr<base_ctx> &get_ctx_ptr();

        bool get_closed_flag() const;
        void set_closed_flag(bool val);

        bool get_is_close() const;
        void set_is_close(bool val);
        bool get_is_ready() const;
        void set_is_ready(bool val);
        void record_sent_bytes(size_t bytes);
        void record_recv_bytes(size_t bytes);

    private:
        avant::utility::vec_str_buffer recv_buffer;
        avant::utility::vec_str_buffer send_buffer;
        int fd{-1};
        uint64_t gid{0};
        avant::socket::socket socket_obj;
        std::shared_ptr<base_ctx> ctx_ptr{nullptr};
        bool closed_flag{false};
        bool is_close{false};
        bool is_ready{false}; // will be setting true on notify app new_connection

    private:
        void clear_timeout_traffic_data(std::deque<traffic_data> &traffic_data_list,
                                        const std::chrono::time_point<std::chrono::steady_clock> &now_timestamp,
                                        size_t seconds);
        void record_traffic_bytes(std::deque<traffic_data> &traffic_data_list, size_t bytes);

    private:
        std::deque<traffic_data> traffic_sent_data_list;
        std::deque<traffic_data> traffic_recv_data_list;
    };
}