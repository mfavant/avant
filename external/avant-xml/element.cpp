#include "element.h"

#include <sstream>
#include <algorithm>

using namespace avant::xml;

element::element(const string &name) : m_name(name)
{
}

element::element(const string &name, const string &text) : m_name(name), m_text(text)
{
}

const string &element::name() const
{
    return m_name;
}

void element::name(const string &name)
{
    m_name = name;
}

const string &element::text() const
{
    return m_text;
}

void element::text(const string &text)
{
    m_text = text;
}

string &element::attr(const string &key)
{
    return m_attrs[key];
}

void element::attr(const string &key, const string &value)
{
    if (value != "")
    {
        m_attrs[key] = value;
        return;
    }
    auto it = m_attrs.find(key);
    if (it != m_attrs.end()) // finded key
    {
        m_attrs.erase(it); // delete the attribute if value is blank string
    }
}

const element &element::operator[](int index) const
{
    int size = m_children.size();
    if (index >= 0 && index < size)
    {
        return m_children[index]; // return target
    }
    return null(); // return null object
}

const element &element::operator[](const string &name) const
{
    // find target node by name
    for (auto it = m_children.begin(); it != m_children.end(); it++)
    {
        if (it->name() == name)
        {
            return *it; // return target child
        }
    }
    return null(); // return null object
}

void element::append(const element &child)
{
    m_children.push_back(child);
}

int element::size()
{
    return m_children.size();
}

void element::clear()
{
    m_children.clear();
    m_attrs.clear();
    m_name.clear();
    m_text.clear();
}

avant::xml::element::iterator element::begin()
{
    return m_children.begin();
}

avant::xml::element::iterator element::end()
{
    return m_children.end();
}

avant::xml::element::const_iterator element::cbegin() const
{
    return m_children.cbegin();
}

avant::xml::element::const_iterator element::cend() const
{
    return m_children.cend();
}

std::ostream &element::operator<<(std::ostream &os)
{
    os << to_string(*this);
    return os;
}

std::string element::to_string(const element &el)
{
    if (el.m_name == "")
    {
        return "";
    }
    std::ostringstream os;
    // before tag
    os << "<" << el.m_name;
    std::for_each(el.m_attrs.begin(), el.m_attrs.end(), [&os](const std::pair<string, string> &it) -> void
                  { os << " " << it.first << "=\"" << it.second << "\""; });
    os << ">";
    // childs
    if (el.m_children.size() > 0)
    {
        // deep search
        std::for_each(el.m_children.begin(), el.m_children.end(), [&os](const element &it) -> void
                      { os << to_string(it); });
    }
    else
    {
        os << el.m_text;
    }
    // after tag
    os << "</" << el.m_name << ">";
    return os.str();
}

const element &element::null()
{
    static const element null;
    return null;
}