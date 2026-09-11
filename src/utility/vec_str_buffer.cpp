#include "utility/vec_str_buffer.h"

using namespace avant::utility;

const char *vec_str_buffer::get_read_ptr() const
{
    return m_data.data();
}

void vec_str_buffer::reserve(size_t bytes)
{
    if (bytes <= m_data.capacity())
    {
        return;
    }
    m_data.reserve(bytes);
}

void vec_str_buffer::move_read_ptr_n(size_t n)
{
    if (n > m_data.size())
    {
        n = m_data.size();
    }
    m_data.erase(0, n);
}

void vec_str_buffer::append(const char *bytes, size_t n)
{
    if (bytes == nullptr)
    {
        return;
    }
    m_data.append(bytes, n);
}

void vec_str_buffer::clear()
{
    m_data.clear();
}

bool vec_str_buffer::empty() const
{
    return m_data.empty();
}

size_t vec_str_buffer::size() const
{
    return m_data.size();
}
