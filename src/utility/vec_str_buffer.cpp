#include "utility/vec_str_buffer.h"

using namespace avant::utility;

vec_str_buffer::vec_str_buffer()
{
}

vec_str_buffer::~vec_str_buffer()
{
}

const char *vec_str_buffer::get_read_ptr()
{
    return data.c_str();
}

void vec_str_buffer::reserve(size_t bytes)
{
    if (bytes <= data.size() || bytes <= before_reserve_bytes)
    {
        return;
    }
    data.reserve(bytes);
    before_reserve_bytes = bytes;
}

void vec_str_buffer::move_read_ptr_n(size_t n)
{
    data.erase(0, n);
}

void vec_str_buffer::append(const char *bytes, size_t n)
{
    data.append(bytes, n);
}

void vec_str_buffer::clear()
{
    data.clear();
}

bool vec_str_buffer::empty()
{
    return data.empty();
}

size_t vec_str_buffer::size()
{
    return data.size();
}