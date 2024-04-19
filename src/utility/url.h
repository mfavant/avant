#pragma once
#include <string>

namespace avant::utility
{
    class url
    {
    public:
        static std::string decode(const std::string &str);
        static std::string encode(const std::string &str);
    };
}