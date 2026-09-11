#pragma once

#include <cstddef>
#include <string>

namespace avant::utility
{
    class vec_str_buffer
    {
    public:
        vec_str_buffer() = default;

        [[nodiscard]] const char *get_read_ptr() const;
        void reserve(size_t bytes);
        void move_read_ptr_n(size_t n);
        void append(const char *bytes, size_t n);
        void clear();
        [[nodiscard]] bool empty() const;
        [[nodiscard]] size_t size() const;

    private:
        std::string m_data;
    };
}
