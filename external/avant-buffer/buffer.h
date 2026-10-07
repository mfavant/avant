#pragma once
#include <cstddef>
#include <stdexcept>
#include <memory>

namespace avant
{
    namespace buffer
    {
        class buffer
        {
        public:
            buffer();
            buffer(const buffer &other);
            buffer &operator=(const buffer &other);
            buffer(buffer &&other) noexcept;
            buffer &operator=(buffer &&other) noexcept;
            ~buffer() noexcept;

            [[nodiscard]] std::size_t read(char *dest, std::size_t size) noexcept(false);
            std::size_t write(const char *source, std::size_t size) noexcept(false);
            [[nodiscard]] std::size_t can_readable_size() const noexcept(false);
            [[nodiscard]] std::size_t copy_all(char *out, std::size_t out_len) const noexcept(false);
            [[nodiscard]] bool read_ptr_move_n(std::size_t n) noexcept(false);
            [[nodiscard]] char *force_get_read_ptr() noexcept(false);
            [[nodiscard]] const char *force_get_read_ptr() const noexcept(false);
            [[nodiscard]] char *force_get_write_ptr() noexcept(false);
            [[nodiscard]] const char *force_get_write_ptr() const noexcept(false);
            [[nodiscard]] std::size_t blank_space() const noexcept(false);

            void set_limit_max(std::size_t limit_max);
            [[nodiscard]] std::size_t get_limit_max() const noexcept;
            void clear();

        private:
            static constexpr std::size_t DEFAULT_CAPACITY = 1024;

            bool check_and_write(const char *source, std::size_t size);
            void move_to_before();
            [[nodiscard]] std::size_t after_size() const;
            [[nodiscard]] char *get_read_ptr();
            [[nodiscard]] const char *get_read_ptr() const;
            [[nodiscard]] char *get_write_ptr();
            [[nodiscard]] const char *get_write_ptr() const;
            void ensure_capacity(std::size_t incoming_size);
            void init_with_capacity(std::size_t capacity);
            void adopt(buffer &other);

        private:
            std::size_t m_limit_max{DEFAULT_CAPACITY};
            std::size_t m_capacity{DEFAULT_CAPACITY};
            char *m_read_ptr{nullptr};
            char *m_write_ptr{nullptr};
            std::unique_ptr<char[]> m_buffer;
        };
    }
}
