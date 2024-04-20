#pragma once

#include <iostream>
#include <vector>
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
        void cache_swap();
        std::string &push();
        const std::string &first();
        void pop();
        bool empty();

    public:
        std::vector<std::string> data1;
        std::vector<std::string> data2;
        size_t reader_used_len;
        size_t writer_used_len;
        size_t reader_idx;
        size_t writer_idx;
        std::vector<std::string> *data_reader;
        std::vector<std::string> *data_writer;
        size_t read_out_idx;
    };
}