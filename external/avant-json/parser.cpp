#include "parser.h"
#include <stdexcept>

using avant::json::json;
using avant::json::parser;

parser::parser()
{
}

parser::~parser()
{
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
            return out;
        }
        if (ch != '\\')
        {
            out += ch;
            continue;
        }
        throw std::logic_error("not support escaped characters in string");
    }
}

json parser::parse_number()
{
    size_t pos = m_idx;
    if (m_str[m_idx] == '-')
    {
        m_idx++;
    }

    if (m_str[m_idx] == '0')
    {
        m_idx++;
        if (in_range(m_str[m_idx], '0', '9'))
        {
            throw std::logic_error("leading 0s not permitted in numbers");
        }
    }
    else if (in_range(m_str[m_idx], '1', '9'))
    {
        m_idx++;
        while (in_range(m_str[m_idx], '0', '9'))
        {
            m_idx++;
        }
    }
    else
    {
        throw std::logic_error("invalid character in number");
    }

    if (m_str[m_idx] == '.')
    {
        m_idx++;
        if (!in_range(m_str[m_idx], '0', '9'))
        {
            throw std::logic_error("at least one digit required in fractional part");
        }
        while (in_range(m_str[m_idx], '0', '9'))
        {
            m_idx++;
        }
        return std::atof(m_str.c_str() + pos);
    }
    else
    {
        return std::atoll(m_str.c_str() + pos);
    }
}

json parser::parse()
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
        json arr(json::json_type::json_array);
        ch = get_next_token();
        if (ch == ']')
        {
            return arr;
        }
        while (true)
        {
            m_idx--;
            arr.append(parse());
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
            std::string key = parse_string();
            ch = get_next_token();
            if (ch != ':')
            {
                throw std::logic_error("expected ':' in object");
            }
            obj[key] = parse();
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
