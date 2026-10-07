#include "buffer.h"

#include <cstring>

using avant::buffer::buffer;

buffer::buffer()
{
    init_with_capacity(DEFAULT_CAPACITY);
}

buffer::buffer(const buffer &other)
{
    m_limit_max = other.m_limit_max;
    m_capacity = other.m_capacity;
    m_buffer = std::make_unique<char[]>(m_capacity);
    const auto copy_bytes = other.can_readable_size();
    memcpy(m_buffer.get(), other.m_read_ptr, copy_bytes);
    m_read_ptr = m_buffer.get();
    m_write_ptr = m_buffer.get() + copy_bytes;
}

buffer &buffer::operator=(const buffer &other)
{
    if (this != &other)
    {
        m_limit_max = other.m_limit_max;
        m_capacity = other.m_capacity;
        m_buffer = std::make_unique<char[]>(m_capacity);
        const auto copy_bytes = other.can_readable_size();
        memcpy(m_buffer.get(), other.m_read_ptr, copy_bytes);
        m_read_ptr = m_buffer.get();
        m_write_ptr = m_buffer.get() + copy_bytes;
    }
    return *this;
}

buffer::buffer(buffer &&other) noexcept
{
    adopt(other);
}

buffer &buffer::operator=(buffer &&other) noexcept
{
    if (this != &other)
    {
        adopt(other);
    }
    return *this;
}

void buffer::adopt(buffer &other)
{
    if (!other.m_buffer)
    {
        other.init_with_capacity(DEFAULT_CAPACITY);
    }
    m_limit_max = other.m_limit_max;
    m_capacity = other.m_capacity;
    m_read_ptr = other.m_read_ptr;
    m_write_ptr = other.m_write_ptr;
    m_buffer = std::move(other.m_buffer);
    other.m_read_ptr = nullptr;
    other.m_write_ptr = nullptr;
    other.m_capacity = DEFAULT_CAPACITY;
    other.m_limit_max = DEFAULT_CAPACITY;
    other.m_buffer.reset();
}

buffer::~buffer() noexcept = default;

void buffer::init_with_capacity(std::size_t capacity)
{
    m_capacity = capacity;
    m_buffer = std::make_unique<char[]>(capacity);
    m_read_ptr = m_buffer.get();
    m_write_ptr = m_buffer.get();
}

std::size_t buffer::read(char *dest, std::size_t size)
{
    if (dest == nullptr || size == 0 || m_buffer == nullptr)
    {
        return 0;
    }
    const auto can_readable = static_cast<std::size_t>(m_write_ptr - m_read_ptr);
    const auto copy_bytes = size < can_readable ? size : can_readable;
    if (copy_bytes > 0)
    {
        memcpy(dest, m_read_ptr, copy_bytes);
        m_read_ptr = m_read_ptr + copy_bytes;
    }
    return copy_bytes;
}

std::size_t buffer::write(const char *source, std::size_t size)
{
    if (source == nullptr || size == 0)
    {
        throw std::runtime_error("write: source == nullptr || size == 0");
    }
    if (m_buffer == nullptr)
    {
        init_with_capacity(DEFAULT_CAPACITY);
    }
    if (size > m_limit_max)
    {
        throw std::runtime_error("write: size > limit_max");
    }
    if (!check_and_write(source, size))
    {
        move_to_before();
        if (!check_and_write(source, size))
        {
            if (size > m_limit_max)
            {
                throw std::runtime_error("write: size > limit_max");
            }
            ensure_capacity(size);
            check_and_write(source, size);
        }
    }
    return size;
}

bool buffer::check_and_write(const char *source, std::size_t size)
{
    const auto m_after_size = after_size();
    if (size <= m_after_size)
    {
        memcpy(m_write_ptr, source, size);
        m_write_ptr = m_write_ptr + size;
        return true;
    }
    return false;
}

void buffer::move_to_before()
{
    const auto readable = static_cast<std::size_t>(m_write_ptr - m_read_ptr);
    memmove(m_buffer.get(), m_read_ptr, readable);
    m_write_ptr = m_buffer.get() + readable;
    m_read_ptr = m_buffer.get();
}

std::size_t buffer::after_size() const
{
    return static_cast<std::size_t>(m_buffer.get() + m_capacity - m_write_ptr);
}

std::size_t buffer::can_readable_size() const
{
    if (m_buffer == nullptr)
    {
        return 0;
    }
    return static_cast<std::size_t>(m_write_ptr - m_read_ptr);
}

std::size_t buffer::copy_all(char *out, std::size_t out_len) const
{
    if (out == nullptr || m_buffer == nullptr)
    {
        return 0;
    }
    const auto all_bytes = static_cast<std::size_t>(m_write_ptr - m_read_ptr);
    if (out_len < all_bytes)
    {
        return 0;
    }
    if (all_bytes > 0)
    {
        memcpy(out, m_read_ptr, all_bytes);
    }
    return all_bytes;
}

bool buffer::read_ptr_move_n(std::size_t n)
{
    if (n == 0)
    {
        return true;
    }
    if (m_buffer == nullptr)
    {
        return false;
    }
    const auto all_bytes = static_cast<std::size_t>(m_write_ptr - m_read_ptr);
    if (n > all_bytes)
    {
        return false;
    }
    m_read_ptr = m_read_ptr + n;
    return true;
}

char *buffer::force_get_read_ptr()
{
    return get_read_ptr();
}

const char *buffer::force_get_read_ptr() const
{
    return get_read_ptr();
}

char *buffer::force_get_write_ptr()
{
    return get_write_ptr();
}

const char *buffer::force_get_write_ptr() const
{
    return get_write_ptr();
}

std::size_t buffer::blank_space() const
{
    if (m_buffer == nullptr)
    {
        return 0;
    }
    const auto u_after_size = after_size();
    const auto consumed = static_cast<std::size_t>(m_read_ptr - m_buffer.get());
    const auto can_promote_bytes = m_limit_max > m_capacity ? m_limit_max - m_capacity : 0;
    return consumed + u_after_size + can_promote_bytes;
}

void buffer::set_limit_max(std::size_t limit_max)
{
    if (limit_max < DEFAULT_CAPACITY)
    {
        limit_max = DEFAULT_CAPACITY;
    }
    if (limit_max > m_limit_max)
    {
        m_limit_max = limit_max;
    }
}

std::size_t buffer::get_limit_max() const noexcept
{
    return m_limit_max;
}

void buffer::clear()
{
    if (m_buffer)
    {
        m_read_ptr = m_buffer.get();
        m_write_ptr = m_buffer.get();
    }
    else
    {
        m_read_ptr = nullptr;
        m_write_ptr = nullptr;
    }
}

char *buffer::get_read_ptr()
{
    if (m_buffer == nullptr)
    {
        init_with_capacity(DEFAULT_CAPACITY);
    }
    return m_read_ptr;
}

const char *buffer::get_read_ptr() const
{
    return m_read_ptr;
}

char *buffer::get_write_ptr()
{
    if (m_buffer == nullptr)
    {
        init_with_capacity(DEFAULT_CAPACITY);
    }
    return m_write_ptr;
}

const char *buffer::get_write_ptr() const
{
    return m_write_ptr;
}

void buffer::ensure_capacity(std::size_t incoming_size)
{
    const auto used = static_cast<std::size_t>(m_write_ptr - m_read_ptr);
    if (used > m_limit_max)
    {
        throw std::runtime_error("write: used exceeds limit_max");
    }
    if (used + incoming_size <= m_capacity)
    {
        return;
    }
    std::size_t new_capacity = m_capacity;
    while (new_capacity < used + incoming_size && new_capacity < m_limit_max)
    {
        new_capacity *= 2;
    }
    if (new_capacity < used + incoming_size)
    {
        new_capacity = used + incoming_size;
    }
    auto new_buffer = std::make_unique<char[]>(new_capacity);
    memcpy(new_buffer.get(), m_buffer.get(), used);
    m_read_ptr = new_buffer.get();
    m_write_ptr = new_buffer.get() + used;
    m_capacity = new_capacity;
    m_buffer = std::move(new_buffer);
}
