#include "parser.h"
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <stdexcept>

using avant::json::json;
using avant::json::parser;

namespace avant::json
{
    // True for a signed char value that is a decimal digit.
    static bool is_digit(char c)
    {
        return c >= '0' && c <= '9';
    }

    // True when c can start the exponent part of a number.
    static bool is_exponent_char(char c)
    {
        return c == 'e' || c == 'E';
    }

    // A JSON number is: -? int frac? exp?  where int is 0 | [1-9]digits.
    // Return true if [pos..len) holds exactly one number; on success set
    // number_len to its length and sign to the leading '-' (or false).
    static bool scan_number(const std::string &str, std::size_t pos, std::size_t len, bool &sign, std::size_t &number_len)
    {
        number_len = 0;
        sign = false;

        std::size_t i = pos;
        if (i < len && str[i] == '-')
        {
            sign = true;
            i++;
        }

        // integer part: 0 | [1-9][0-9]*
        if (i >= len)
        {
            return false;
        }
        if (str[i] == '0')
        {
            i++;
            if (i < len && is_digit(str[i]))
            {
                return false; // leading zero not permitted (e.g. "01")
            }
        }
        else if (i < len && str[i] >= '1' && str[i] <= '9')
        {
            i++;
            while (i < len && is_digit(str[i]))
            {
                i++;
            }
        }
        else
        {
            return false; // no valid first digit
        }

        // fractional part: . [0-9]+
        if (i < len && str[i] == '.')
        {
            i++;
            if (i >= len || !is_digit(str[i]))
            {
                return false; // at least one digit required after '.'
            }
            while (i < len && is_digit(str[i]))
            {
                i++;
            }
        }

        // exponent part: e/E [+-]? [0-9]+
        if (i < len && is_exponent_char(str[i]))
        {
            i++;
            if (i < len && (str[i] == '+' || str[i] == '-'))
            {
                i++;
            }
            if (i >= len || !is_digit(str[i]))
            {
                return false; // at least one digit required after e/E
            }
            while (i < len && is_digit(str[i]))
            {
                i++;
            }
        }

        number_len = i - pos;
        return true;
    }
}

void parser::load(const std::string &str)
{
    m_str = str;
    m_idx = 0;
}

void parser::skip_white_spaces()
{
    while (m_idx < m_str.size() && (m_str[m_idx] == ' ' || m_str[m_idx] == '\r' || m_str[m_idx] == '\n' || m_str[m_idx] == '\t'))
    {
        m_idx++;
    }
}

char parser::get_next_token()
{
    skip_white_spaces();
    if (m_idx == m_str.size())
    {
        throw std::logic_error("unexpected end of input");
    }
    return m_str[m_idx++];
}

json parser::parse_null()
{
    if (m_str.compare(m_idx, 4, "null") == 0)
    {
        m_idx += 4;
        return json();
    }
    throw std::logic_error("parse null error");
}

json parser::parse_bool()
{
    if (m_str.compare(m_idx, 4, "true") == 0)
    {
        m_idx += 4;
        return json(true);
    }
    if (m_str.compare(m_idx, 5, "false") == 0)
    {
        m_idx += 5;
        return json(false);
    }
    throw std::logic_error("parse bool error");
}

json parser::parse_string()
{
    // The opening '"' has already been consumed by parse(); read the body.
    std::string out;
    while (true)
    {
        if (m_idx >= m_str.size())
        {
            throw std::logic_error("unexpected end of input in string");
        }
        char ch = m_str[m_idx++];
        if (ch == '"')
        {
            return json(out);
        }
        if (ch != '\\')
        {
            // RFC 8259 s4: U+0000..U+001F must be escaped in strings.
            if (static_cast<unsigned char>(ch) < 0x20)
            {
                throw std::logic_error("unescaped control character in string");
            }
            out += ch;
            continue;
        }
        // escape sequence
        if (m_idx >= m_str.size())
        {
            throw std::logic_error("unexpected end of input in string escape");
        }
        char esc = m_str[m_idx++];
        switch (esc)
        {
        case '"':
            out += '"';
            break;
        case '\\':
            out += '\\';
            break;
        case '/':
            out += '/';
            break;
        case 'b':
            out += '\b';
            break;
        case 'f':
            out += '\f';
            break;
        case 'n':
            out += '\n';
            break;
        case 'r':
            out += '\r';
            break;
        case 't':
            out += '\t';
            break;
        case 'u':
        {
            if (m_idx + 4 > m_str.size())
            {
                throw std::logic_error("unexpected end of input in \\u escape");
            }
            // Parse exactly four hex digits; validate them (stoul would accept a
            // shorter run and leave the trailing byte unconsumed, corrupting the
            // following characters).
            unsigned int hi = 0;
            for (int k = 0; k < 4; k++)
            {
                const char d = m_str[m_idx + static_cast<std::size_t>(k)];
                const unsigned int nib =
                    static_cast<unsigned int>((d >= '0' && d <= '9') ? (d - '0') : ((d >= 'a' && d <= 'f') ? (d - 'a' + 10) : ((d >= 'A' && d <= 'F') ? (d - 'A' + 10) : 16)));
                if (nib >= 16)
                {
                    throw std::logic_error("invalid \\u escape");
                }
                hi = (hi << 4) | nib;
            }
            m_idx += 4;

            // A high surrogate (0xD800-0xDBFF) must be followed by a low
            // surrogate (0xDC00-0xDFFF) to form a non-BMP code point. Reassemble
            // the pair; a lone surrogate is invalid and rejected.
            unsigned int cp = hi;
            if (hi >= 0xD800u && hi <= 0xDBFFu)
            {
                if (m_idx + 2 > m_str.size() || m_str[m_idx] != '\\' || m_str[m_idx + 1] != 'u')
                {
                    throw std::logic_error("unpaired high surrogate in \\u escape");
                }
                m_idx += 2;
                if (m_idx + 4 > m_str.size())
                {
                    throw std::logic_error("unexpected end of input in low surrogate");
                }
                unsigned int lo = 0;
                for (int k = 0; k < 4; k++)
                {
                    const char d = m_str[m_idx + static_cast<std::size_t>(k)];
                    const unsigned int nib =
                        static_cast<unsigned int>((d >= '0' && d <= '9') ? (d - '0') : ((d >= 'a' && d <= 'f') ? (d - 'a' + 10) : ((d >= 'A' && d <= 'F') ? (d - 'A' + 10) : 16)));
                    if (nib >= 16)
                    {
                        throw std::logic_error("invalid \\u escape");
                    }
                    lo = (lo << 4) | nib;
                }
                if (lo < 0xDC00u || lo > 0xDFFFu)
                {
                    throw std::logic_error("expected low surrogate after high surrogate");
                }
                m_idx += 4;
                cp = 0x10000u + ((hi - 0xD800u) << 10) + (lo - 0xDC00u);
            }
            else if (hi >= 0xDC00u && hi <= 0xDFFFu)
            {
                throw std::logic_error("unpaired low surrogate in \\u escape");
            }

            // Encode the code point as UTF-8.
            if (cp < 0x80u)
            {
                out += static_cast<char>(cp);
            }
            else if (cp < 0x800u)
            {
                out += static_cast<char>(0xC0u | (cp >> 6));
                out += static_cast<char>(0x80u | (cp & 0x3Fu));
            }
            else if (cp < 0x10000u)
            {
                out += static_cast<char>(0xE0u | (cp >> 12));
                out += static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu));
                out += static_cast<char>(0x80u | (cp & 0x3Fu));
            }
            else
            {
                out += static_cast<char>(0xF0u | (cp >> 18));
                out += static_cast<char>(0x80u | ((cp >> 12) & 0x3Fu));
                out += static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu));
                out += static_cast<char>(0x80u | (cp & 0x3Fu));
            }
            break;
        }
        default:
            throw std::logic_error("not support escaped character in string");
        }
    }
}

json parser::parse_number()
{
    // get_next_token() consumed the first character ('-' or a digit), but
    // parse() rewound m_idx by one before calling us, so m_idx points at the
    // number's first character: the whole number (sign included) starts here.
    const std::size_t pos = m_idx;
    const std::size_t len = m_str.size();

    bool sign = false;
    std::size_t number_len = 0;
    if (!scan_number(m_str, pos, len, sign, number_len))
    {
        throw std::logic_error("invalid number");
    }

    const std::size_t start = pos;
    const std::size_t end = pos + number_len;
    m_idx = end;

    // Integer if there is no fractional/exponent part.
    const std::size_t base = sign ? pos + 1 : pos;
    std::size_t i = base;
    while (i < end && is_digit(m_str[i]))
    {
        i++;
    }
    const bool has_frac_or_exp = (i < end);

    if (!has_frac_or_exp)
    {
        // long long holds at most 19 significant digits (9223372036854775807).
        // More than 19 can never fit; exactly 19 may still overflow (e.g.
        // 9223372036854775808), which strtoll reports via ERANGE without
        // undefined behavior.
        if (i - base > 19)
        {
            throw std::logic_error("number out of range");
        }
        errno = 0;
        const long long value = std::strtoll(m_str.c_str() + start, nullptr, 10);
        if (errno == ERANGE)
        {
            throw std::logic_error("number out of range");
        }
        return json(value);
    }

    // Floating point. strtod + finite check guards against 1e999 (inf).
    const double value = std::strtod(m_str.c_str() + start, nullptr);
    if (!std::isfinite(value))
    {
        throw std::logic_error("number out of range");
    }
    return json(value);
}

json parser::parse()
{
    json value = parse(0);
    // Reject trailing content: a valid JSON document is exactly one value.
    skip_white_spaces();
    if (m_idx != m_str.size())
    {
        throw std::logic_error("unexpected content after JSON value");
    }
    return value;
}

json parser::parse(std::size_t depth)
{
    char ch = get_next_token();
    if (ch == 'n')
    {
        m_idx--;
        return parse_null();
    }
    if (ch == 't' || ch == 'f')
    {
        m_idx--;
        return parse_bool();
    }
    if (ch == '-' || (ch >= '0' && ch <= '9'))
    {
        m_idx--;
        return parse_number();
    }
    if (ch == '"')
    {
        return parse_string();
    }
    if (ch == '[')
    {
        if (depth + 1 > K_MAX_DEPTH)
        {
            throw std::logic_error("maximum nesting depth exceeded");
        }
        json arr(json::json_type::json_array);
        ch = get_next_token();
        if (ch == ']')
        {
            return arr;
        }
        while (true)
        {
            m_idx--;
            arr.append(parse(depth + 1));
            ch = get_next_token();
            if (ch == ']')
            {
                break;
            }
            if (ch != ',')
            {
                throw std::logic_error("expected ',' in array");
            }
            ch = get_next_token();
        }
        return arr;
    }

    if (ch == '{')
    {
        if (depth + 1 > K_MAX_DEPTH)
        {
            throw std::logic_error("maximum nesting depth exceeded");
        }
        json obj(json::json_type::json_object);
        ch = get_next_token();
        if (ch == '}')
        {
            return obj;
        }
        while (true)
        {
            if (ch != '"')
            {
                throw std::logic_error("expected '\"' in object");
            }
            std::string key = static_cast<std::string>(parse_string());
            ch = get_next_token();
            if (ch != ':')
            {
                throw std::logic_error("expected ':' in object");
            }
            obj[key] = parse(depth + 1);
            ch = get_next_token();
            if (ch == '}')
            {
                break;
            }
            if (ch != ',')
            {
                throw std::logic_error("expected ',' in object");
            }
            ch = get_next_token();
        }
        return obj;
    }

    throw std::logic_error("unexpected character in parse json");
}
