#pragma once

#include <string>
#include "json.h"

namespace avant::json
{
    class parser
    {
    public:
        parser();
        ~parser();
        void load(const std::string &str);
        json parse();

    private:
        void skip_white_spaces();
        char get_next_token();
        json parse_null();
        json parse_bool();
        json parse_number();
        json parse_string();

        bool in_range(long x, long lower, long upper)
        {
            return (x >= lower && x <= upper);
        }

    private:
        std::string m_str{};
        size_t m_idx{0};
    };
};