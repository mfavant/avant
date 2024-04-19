#include <sstream>

#include "value.h"

using namespace avant::inifile;

value::value(const bool &value)
{
    *this = value;
}

value::value(const int &value)
{
    *this = value;
}

value::value(const double &value)
{
    *this = value;
}

value::value(const string &value) : m_value(value)
{
}

value &value::operator=(const bool &value)
{
    m_value = value ? "true" : "false";
    return *this;
}

value &value::operator=(const int &value)
{
    m_value = to_string(value);
    return *this;
}

value &value::operator=(const double &value)
{
    m_value = to_string(value);
    return *this;
}

value &value::operator=(const string &value)
{
    m_value = value;
    return *this;
}

value::operator bool() const
{
    return m_value == "true" ? true : false;
}

value::operator int() const
{
    return std::atoi(m_value.c_str());
}

value::operator double() const
{
    return std::atof(m_value.c_str());
}

value::operator string() const
{
    return m_value;
}

bool value::operator==(const value &other) const
{
    string other_value = other;
    return m_value == other_value;
}