#include "json.h"
#include "parser.h"
#include <stdexcept>
#include <sstream>

using std::istringstream;
using std::ostringstream;
using namespace avant::json;

json::json()
{
}

json::json(json::json_type type) : m_json_type(type)
{
}

json::json(bool value)
{
    m_json_type = json_type::json_bool;
    m_bool_value = value;
}

json::json(long long value)
{
    m_json_type = json_type::json_integer;
    m_integer_value = value;
}

json::json(double value)
{
    m_json_type = json_type::json_double;
    m_double_value = value;
}

json::json(const char *value)
{
    m_json_type = json_type::json_string;
    m_string_value = value;
}

json::json(const std::string &value)
{
    m_json_type = json_type::json_string;
    m_string_value = value;
}

json::json(const json &other)
{
    copy(other);
}

json::~json()
{
    clear();
}

json::json_type json::type() const
{
    return m_json_type;
}

bool json::is_null() const
{
    return m_json_type == json_type::json_null;
}

bool json::is_bool() const
{
    return m_json_type == json_type::json_bool;
}

bool json::is_integer() const
{
    return m_json_type == json_type::json_integer;
}

bool json::is_double() const
{
    return m_json_type == json_type::json_double;
}

bool json::is_string() const
{
    return m_json_type == json_type::json_string;
}

bool json::is_array() const
{
    return m_json_type == json_type::json_array;
}

bool json::is_object() const
{
    return m_json_type == json_type::json_object;
}

bool json::as_bool() const
{
    switch (m_json_type)
    {
    case json_type::json_bool:
        return m_bool_value;
    default:
        break;
    }
    throw std::logic_error("function json::as_bool value type error");
}

long long json::as_integer() const
{
    switch (m_json_type)
    {
    case json_type::json_integer:
        return m_integer_value;
    default:
        break;
    }
    throw std::logic_error("function json::as_integet value type error");
}

double json::as_double() const
{
    switch (m_json_type)
    {
    case json_type::json_double:
        return m_double_value;
    default:
        break;
    }
    throw std::logic_error("function json::as_double value type error");
}

std::string json::as_string() const
{
    switch (m_json_type)
    {
    case json_type::json_string:
        return m_string_value;
    default:
        break;
    }
    throw std::logic_error("function json::as_string value type error");
}

void json::copy(const json &other)
{
    clear();
    m_json_type = other.type();
    switch (other.type())
    {
    case json_type::json_array:
        m_array_value = other.m_array_value;
        break;
    case json_type::json_bool:
        m_bool_value = other.m_bool_value;
        break;
    case json_type::json_double:
        m_double_value = other.m_double_value;
        break;
    case json_type::json_integer:
        m_integer_value = other.m_integer_value;
        break;
    case json_type::json_null:
        break;
    case json_type::json_object:
        m_object_value = other.m_object_value;
        break;
    case json_type::json_string:
        m_string_value = other.m_string_value;
        break;
    default:
        break;
    }
}

// array size
size_t json::size() const
{
    switch (m_json_type)
    {
    case json_type::json_array:
        return m_array_value.size();
    case json_type::json_object:
        return m_object_value.size();
    default:
        break;
    }
    throw std::logic_error("function json::size value type error");
}

bool json::empty() const
{
    switch (m_json_type)
    {
    case json_type::json_null:
        return true;
    case json_type::json_array:
        return m_array_value.empty();
    case json_type::json_object:
        return m_object_value.empty();
    default:
        break;
    }
    throw std::logic_error("function json::empty value type error");
}

void json::clear()
{
    m_array_value.clear();
    m_bool_value = false;
    m_double_value = 0.;
    m_integer_value = 0;
    m_object_value.clear();
    m_string_value.clear();
}

bool json::remove(size_t index)
{
    if (m_json_type != json_type::json_array)
    {
        return false;
    }
    size_t size = m_array_value.size();
    if (index < 0 || index >= size)
    {
        return false;
    }
    m_array_value.erase(m_array_value.begin() + index);
    return true;
}

bool json::remove(const char *key)
{
    std::string name = key;
    return remove(name);
}

bool json::remove(const std::string &key)
{
    if (type() != json_type::json_object)
    {
        return false;
    }
    std::map<std::string, json>::iterator it = m_object_value.find(key);
    if (it == m_object_value.end())
    {
        return false;
    }
    m_object_value.erase(it);
    return true;
}

bool json::has(size_t index) const
{
    if (m_json_type != json_type::json_array)
    {
        return false;
    }
    size_t size = m_array_value.size();
    return (index >= 0 && index < size);
}

bool json::has(const char *key) const
{
    if (m_json_type != json_type::json_object)
    {
        return false;
    }
    std::map<std::string, json>::const_iterator it = m_object_value.find(key);
    return (it != m_object_value.end());
}

bool json::has(const std::string &key) const
{
    if (m_json_type != json_type::json_object)
    {
        return false;
    }
    std::map<std::string, json>::const_iterator it = m_object_value.find(key);
    return (it != m_object_value.end());
}

const json &json::get(size_t index) const
{
    if (m_json_type != json_type::json_array)
    {
        throw std::logic_error("function json::get [int] requires array value");
    }
    size_t size = m_array_value.size();
    if (index >= 0 && index < size)
    {
        return m_array_value.at(index);
    }
    return null();
}

const json &json::get(const char *key) const
{
    const std::string name = key;
    return get(name);
}

const json &json::get(const std::string &key) const
{
    if (m_json_type != json_type::json_object)
    {
        throw std::logic_error("function json::get [const string&] requires object value");
    }
    std::map<std::string, json>::const_iterator it = m_object_value.find(key);
    if (it != m_object_value.end())
    {
        return it->second;
    }
    return null();
}

void json::set(const json &other)
{
    copy(other);
}

void json::set(bool value)
{
    clear();
    m_json_type = json_type::json_bool;
    m_bool_value = value;
}

void json::set(long long value)
{
    clear();
    m_json_type = json_type::json_integer;
    m_integer_value = value;
}

void json::set(double value)
{
    clear();
    m_json_type = json_type::json_double;
    m_double_value = value;
}

void json::set(const std::string &value)
{
    clear();
    m_json_type = json_type::json_string;
    m_string_value = value;
}

json &json::append(const json &value)
{
    if (m_json_type == json_type::json_null)
    {
        m_json_type = json_type::json_array;
    }
    if (m_json_type != json_type::json_array)
    {
        throw std::logic_error("function json::append requires array value");
    }
    m_array_value.push_back(value);
    return (*this);
}

json &json::operator=(const json &other)
{
    copy(other);
    return *this;
}

json &json::operator=(bool value)
{
    json other(value);
    copy(other);
    return *this;
}

json &json::operator=(long long value)
{
    json other(value);
    copy(other);
    return *this;
}

json &json::operator=(double value)
{
    json other(value);
    copy(other);
    return *this;
}

json &json::operator=(const std::string &value)
{
    json other(value);
    copy(other);
    return *this;
}

json &json::operator=(const char *value)
{
    json other(value);
    copy(other);
    return *this;
}

bool json::operator==(const json &other)
{
    if (m_json_type != other.m_json_type)
    {
        return false;
    }
    switch (m_json_type)
    {
    case json_type::json_array:
    {
        return false;
    }
    break;
    case json_type::json_bool:
    {
        return (m_bool_value == other.m_bool_value);
    }
    break;
    case json_type::json_double:
    {
        return (m_double_value == other.m_double_value);
    }
    break;
    case json_type::json_integer:
    {
        return (m_integer_value == other.m_integer_value);
    }
    break;
    case json_type::json_null:
    {
        return true;
    }
    break;
    case json_type::json_object:
    {
        return false;
    }
    break;
    case json_type::json_string:
    {
        return (m_string_value == other.m_string_value);
    }
    break;
    default:
        break;
    }
    return false;
}

bool json::operator==(bool value)
{
    json other = value;
    return (*this == other);
}

bool json::operator==(long long value)
{
    json other = value;
    return (*this == other);
}

bool json::operator==(double value)
{
    json other = value;
    return (*this == other);
}

bool json::operator==(const std::string &value)
{
    json other = value;
    return (*this == other);
}

bool json::operator==(const char *value)
{
    json other = value;
    return (*this == other);
}

bool json::operator!=(const json &other)
{
    return !(*this == other);
}

bool json::operator!=(bool value)
{
    json other = value;
    return (*this != other);
}

bool json::operator!=(long long value)
{
    json other = value;
    return (*this != other);
}

bool json::operator!=(double value)
{
    json other = value;
    return (*this != other);
}

bool json::operator!=(const std::string &value)
{
    json other = value;
    return (*this != other);
}

bool json::operator!=(const char *value)
{
    json other = value;
    return (*this != other);
}

json &json::operator[](size_t index)
{
    if (m_json_type == json_type::json_null)
    {
        m_json_type = json_type::json_array;
    }
    if (m_json_type != json_type::json_array)
    {
        throw std::logic_error("function json::operator [int] requires array value");
    }
    size_t size = m_array_value.size();
    if (index < 0)
    {
        throw std::logic_error("function json::operator [int] index less 0");
    }
    if (index > 0 && index < size)
    {
        return m_array_value.at(index);
    }
    if (index >= size)
    {
        for (size_t i = size; i <= index; i++)
        {
            m_array_value.push_back(json());
        }
    }
    return m_array_value.at(index);
}

const json &json::operator[](size_t index) const
{
    return get(index);
}

json &json::operator[](const char *key)
{
    std::string name = key;
    return (*this)[name];
}

const json &json::operator[](const char *key) const
{
    return get(key);
}

json &json::operator[](const std::string &key)
{
    if (m_json_type == json_type::json_null)
    {
        m_json_type = json_type::json_object;
    }
    if (m_json_type != json_type::json_object)
    {
        throw std::logic_error("function json::operator [const std::string&] requires object value");
    }
    return m_object_value[key];
}

const json &json::operator[](const std::string &key) const
{
    return get(key);
}

json::operator bool()
{
    if (m_json_type != json_type::json_bool)
    {
        throw std::logic_error("function json::operator (bool) requires bool value");
    }
    return m_bool_value;
}

json::operator long long()
{
    if (m_json_type != json_type::json_integer)
    {
        throw std::logic_error("function json::operator (long long) requires long long value");
    }
    return m_integer_value;
}

json::operator double()
{
    if (m_json_type != json_type::json_double)
    {
        throw std::logic_error("function json::operator (double) requires double value");
    }
    return m_double_value;
}

json::operator std::string()
{
    if (m_json_type != json_type::json_string)
    {
        throw std::logic_error("function json::operator (string) requires string value");
    }
    return m_string_value;
}

json::operator std::string() const
{
    if (m_json_type != json_type::json_string)
    {
        throw std::logic_error("function json::operator (string) requires string value");
    }
    return m_string_value;
}

json const &json::null()
{
    static const json null;
    return null;
}

void json::parse(const std::string &str)
{
    parser parser_object;
    parser_object.load(str);
    *this = parser_object.parse();
}

std::string json::to_string() const
{
    ostringstream os;
    switch (type())
    {
    case json_type::json_null:
        os << "null";
        break;
    case json_type::json_bool:
        if (m_bool_value)
        {
            os << "true";
        }
        else
        {
            os << "false";
        }
        break;
    case json_type::json_integer:
        os << m_integer_value;
        break;
    case json_type::json_double:
        os << m_double_value;
        break;
    case json_type::json_string:
        os << "\"" << m_string_value << "\"";
        break;
    case json_type::json_array:
    {
        os << "[";
        for (std::vector<json>::const_iterator it = m_array_value.begin(); it != m_array_value.end(); it++)
        {
            if (it != m_array_value.begin())
            {
                os << ",";
            }
            os << (*it).to_string();
        }
        os << "]";
    }
    break;
    case json_type::json_object:
    {
        os << "{";
        for (std::map<std::string, json>::const_iterator it = m_object_value.begin(); it != m_object_value.end(); it++)
        {
            if (it != m_object_value.begin())
            {
                os << ",";
            }
            os << "\"" << it->first << "\":" << it->second.to_string();
        }
        os << "}";
        return os.str();
    }
    break;
    default:
        break;
    }
    return os.str();
}
