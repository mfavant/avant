#include "document.h"

#include <fstream>
#include <sstream>
#include <string>
#include <iostream>
#include <cctype>

#include "element.h"

using namespace avant::xml;

document::document() : m_idx(0)
{
}

void document::load_file(const string &path)
{
    std::ifstream in(path.c_str());
    std::ostringstream buf;
    std::string line;
    while (getline(in, line)) // read xml file
    {
        buf << line;
    }
    load_string(buf.str()); // load xml string
}

void document::load_string(const string &str)
{
    m_str = str;
    m_idx = 0;
}

void document::skip_white_spaces()
{
    char ch = m_str[m_idx];
    while (ch == ' ' || ch == '\r' || ch == '\n' || ch == '\t')
    {
        m_idx++;
        ch = m_str[m_idx];
    }
}

element document::parse()
{
    skip_white_spaces();
    // throwing logic_error if document is empty
    if (m_idx == m_str.size() || m_str[m_idx] == '\0')
    {
        throw std::logic_error("xml document is empty");
    }
    // analysis xml declaration
    if (m_str[m_idx + 0] == '<' &&
        m_str[m_idx + 1] == '?' &&
        m_str[m_idx + 2] == 'x' &&
        m_str[m_idx + 3] == 'm' &&
        m_str[m_idx + 4] == 'l')
    {
        if (!parse_declaration())
        {
            throw std::logic_error("xml declaration is error");
        }
    }
    skip_white_spaces();

    // try to skip comment
    if (m_str[m_idx + 0] == '<' &&
        m_str[m_idx + 1] == '!' &&
        m_str[m_idx + 2] == '-' &&
        m_str[m_idx + 3] == '-')
    {
        if (!parse_comment())
        {
            throw std::logic_error("xml comment is error");
        }
    }

    skip_white_spaces();
    element elem; // document root element
    string name;

    // tag's name must be using alpha or _ to first charactor
    if (m_str[m_idx] == '<' && (isalpha(m_str[m_idx + 1]) || m_str[m_idx + 1] == '_'))
    {
        m_idx++; // skip '<'
        // parse node's name
        name = parse_element_name();
        elem.name(name); // init root node'name
        skip_white_spaces();

        while (m_str[m_idx] != '\0')
        {
            if (m_str[m_idx] == '>')
            {
                m_idx++; // skip >
                string text = parse_element_text();
                skip_white_spaces();
                if (text != "")
                {
                    elem.text(text);
                }
                else
                {
                    element child = parse(); // parse child node
                    elem.append(child);
                }
            }
            else if (m_str[m_idx] == '/')
            {
                if (m_str[m_idx + 1] == '>')
                {
                    m_idx += 2;
                    return elem;
                }
                else
                {
                    throw std::logic_error("xml empty element is error");
                }
            }
            else if (m_str[m_idx] == '<')
            {
                if (m_str[m_idx + 1] == '/')
                { // it's element end tag
                    string end_tag = "</" + name + ">";
                    size_t pos = m_str.find(end_tag, m_idx);
                    if (pos == std::string::npos)
                    {
                        throw std::logic_error("xml error " + name + " end tag not found");
                    }
                    if (pos == m_idx) // it's </name>
                    {
                        m_idx += end_tag.size();
                        return elem;
                    }
                }
                else if (m_str[m_idx + 0] == '<' &&
                         m_str[m_idx + 1] == '!' &&
                         m_str[m_idx + 2] == '-' &&
                         m_str[m_idx + 3] == '-')
                { // it's comment
                    if (!parse_comment())
                    {
                        throw std::logic_error("xml comment is error");
                    }
                }
                else // it's tag start
                {
                    element child = parse();
                    elem.append(child);
                }
            }
            else
            {
                // parse element's attributes
                string key = parse_element_attr_key();
                skip_white_spaces();
                if (m_str[m_idx] != '=')
                {
                    throw std::logic_error("xml element attr is error " + key);
                }
                m_idx++;
                string val = parse_element_attr_val();
                elem.attr(key, val);
            }
            skip_white_spaces();
        }
    }
    return elem;
}

bool document::parse_declaration()
{
    size_t pos = m_str.find("?>", m_idx); // searching first ?> from index m_idx
    if (pos == std::string::npos)
    {
        return false;
    }
    m_idx = pos + 2; // skip <?xml ... ?>
    return true;
}

bool document::parse_comment()
{
    auto pos = m_str.find("-->", m_idx);
    if (pos == std::string::npos)
    {
        return false;
    }
    m_idx = pos + 3;
    return true;
}

string document::parse_element_name()
{
    skip_white_spaces();
    std::stringstream res;
    if (isalpha(m_str[m_idx]) || m_str[m_idx] == '_')
    {
        res.put(m_str[m_idx]); // first char
        m_idx++;
        while (m_str[m_idx] != '\0' && (isalnum(m_str[m_idx]) ||
                                        m_str[m_idx] == '_' ||
                                        m_str[m_idx] == '-' ||
                                        m_str[m_idx] == ':'))
        {
            res.put(m_str[m_idx]);
            m_idx++;
        }
    }
    return res.str();
}

string document::parse_element_text()
{
    skip_white_spaces();
    std::ostringstream res;
    while (m_str[m_idx] != '<')
    {
        res.put(m_str[m_idx]);
        m_idx++;
    }
    return res.str();
}

string document::parse_element_attr_key()
{
    skip_white_spaces();
    std::stringstream res;
    if (isalpha(m_str[m_idx]) || m_str[m_idx] == '_')
    {
        res.put(m_str[m_idx]);
        m_idx++;
        while (isalnum(m_str[m_idx]) ||
               m_str[m_idx] == '_' ||
               m_str[m_idx] == '-' ||
               m_str[m_idx] == ':')
        {
            res.put(m_str[m_idx]);
            m_idx++;
        }
    }
    return res.str();
}

string document::parse_element_attr_val()
{
    skip_white_spaces();
    if (m_str[m_idx] != '"')
    {
        throw std::logic_error("xml element attr value should be in double quotes");
    }
    m_idx++; // skip left "
    std::stringstream res;
    while (m_str[m_idx] != '"')
    {
        res.put(m_str[m_idx]);
        m_idx++;
    }
    m_idx++; // skip right "
    return res.str();
}
