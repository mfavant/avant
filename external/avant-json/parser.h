#pragma once

#include <cstddef>
#include <string>

#include "json.h"

namespace avant::json
{
    class parser
    {
    public:
        // Maximum nesting depth of a document this parser will accept.
        // A nested `[` or `{` at a depth beyond this throws, which bounds the
        // recursion to a known limit and defeats stack-overflow DoS via deep
        // nesting. 512 levels is far above any realistic document.
        static constexpr std::size_t kMaxDepth = 512;

        parser() = default;
        // Move-only: declaring the move ctor already deletes the implicit copy
        // ctor; delete the copy ops explicitly so the class is consistently
        // non-copyable (the implicit copy *assignment* would otherwise remain).
        parser(const parser &) = delete;
        parser(parser &&other) noexcept
            : m_str(std::move(other.m_str)), m_idx(other.m_idx)
        {
        }
        parser &operator=(const parser &) = delete;
        parser &operator=(parser &&other) noexcept
        {
            m_str = std::move(other.m_str);
            m_idx = other.m_idx;
            return *this;
        }
        ~parser() noexcept = default;

        // Not noexcept: the string copy can throw std::bad_alloc.
        void load(const std::string &str);
        [[nodiscard]] json parse();

    private:
        void skip_white_spaces();
        char get_next_token();
        json parse_null();
        json parse_bool();
        json parse_number();
        json parse_string();
        json parse(std::size_t depth);

    private:
        std::string m_str{};
        std::size_t m_idx{0};
    };
}
