#pragma once

#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <unordered_set>
#include <unordered_map>

namespace avant::utility
{
    class vec_str_buffer
    {
    public:
        vec_str_buffer();
        ~vec_str_buffer();
        const char *get_read_ptr();
        void reserve(size_t bytes);
        void move_read_ptr_n(size_t n);
        void append(const char *bytes, size_t n);
        void clear();
        bool empty();
        size_t size();

    private:
        std::string data;
        size_t before_reserve_bytes{0};
    };
}