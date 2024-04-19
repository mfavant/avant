#include "utility/url.h"
#include <iomanip>
#include <sstream>

using avant::utility::url;

std::string url::decode(const std::string &str)
{
    std::ostringstream decoded;
    std::string::size_type len = str.length();
    for (std::string::size_type i = 0; i < len; ++i)
    {
        if (str[i] == '%' && i + 2 < len) // two char
        {
            int hex_value;
            std::istringstream hex_stream(str.substr(i + 1, 2));
            if (hex_stream >> std::hex >> hex_value)
            {
                decoded << static_cast<char>(hex_value);
                i += 2;
            }
            else
            {
                decoded << str[i];
            }
        }
        else if (str[i] == '+')
        {
            decoded << ' ';
        }
        else
        {
            decoded << str[i];
        }
    }
    return decoded.str();
}

std::string url::encode(const std::string &str)
{
    std::ostringstream encoded;
    encoded << std::hex << std::uppercase;
    for (char c : str)
    {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~' || c == '/')
        {
            encoded << c;
        }
        else
        {
            encoded << '%' << std::setw(2) << static_cast<int>(static_cast<unsigned char>(c));
        }
    }
    return encoded.str();
}
