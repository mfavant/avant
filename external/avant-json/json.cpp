#include "json.h"
#include "parser.h"

#include <climits>
#include <cmath>
#include <cstdio>
#include <stdexcept>

using namespace avant::json;

namespace avant::json
{
    // Append the escaped body of a JSON string (without the surrounding
    // quotes) to out. Escapes the seven required characters (RFC 8259 s2)
    // plus every control character U+0000..U+001F as \u00xx; all other
    // bytes (including raw UTF-8) pass through unmodified.
    static void append_escaped_body(const std::string &value, std::string &out)
    {
        for (const char ch : value)
        {
            switch (ch)
            {
            case '"':
                out += "\\\"";
                break;
            case '\\':
                out += "\\\\";
                break;
            case '\b':
                out += "\\b";
                break;
            case '\f':
                out += "\\f";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            default:
                if (static_cast<unsigned char>(ch) < 0x20)
                {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned int>(static_cast<unsigned char>(ch)));
                    out += buf;
                }
                else
                {
                    out += ch;
                }
                break;
            }
        }
    }

    // A complete JSON string token: opening quote, escaped body, closing quote.
    static std::string string_token(const std::string &value)
    {
        std::string out = "\"";
        out.reserve(value.size() + 2);
        append_escaped_body(value, out);
        out += "\"";
        return out;
    }
}

json::json(json::json_type type) : m_json_type(type)
{
}

json::json(bool value) : m_json_type(json_type::json_bool), m_bool_value(value)
{
}

json::json(long long value) : m_json_type(json_type::json_integer), m_integer_value(value)
{
}

json::json(double value) : m_json_type(json_type::json_double), m_double_value(value)
{
    // JSON has no representation for NaN/Inf; refuse at construction rather
    // than producing a node that to_string cannot serialize validly.
    if (!std::isfinite(value))
    {
        throw std::logic_error("function json::json(double) non-finite value");
    }
}

json::json(const char *value) : m_json_type(json_type::json_string), m_string_value(value ? value : "")
{
}

json::json(const std::string &value) : m_json_type(json_type::json_string), m_string_value(value)
{
}

json::json(const json &other)
{
    copy(other);
}

json::json(json &&other) noexcept
    : m_json_type(other.m_json_type),
      m_integer_value(other.m_integer_value),
      m_double_value(other.m_double_value),
      m_bool_value(other.m_bool_value),
      m_string_value(std::move(other.m_string_value)),
      m_array_value(std::move(other.m_array_value)),
      m_object_value(std::move(other.m_object_value))
{
    // Reset the source to null so a moved-from node is always in one
    // predictable state, instead of keeping a stale type tag with an empty
    // payload (or a stale scalar, which is copied rather than moved).
    other.clear();
}

json &json::operator=(const json &other)
{
    if (this != &other)
    {
        *this = json(other);
    }
    return *this;
}

json &json::operator=(json &&other) noexcept
{
    if (this != &other)
    {
        m_json_type = other.m_json_type;
        m_integer_value = other.m_integer_value;
        m_double_value = other.m_double_value;
        m_bool_value = other.m_bool_value;
        m_string_value = std::move(other.m_string_value);
        m_array_value = std::move(other.m_array_value);
        m_object_value = std::move(other.m_object_value);
        // As in the move ctor: leave the source as a null node.
        other.clear();
    }
    return *this;
}

json &json::operator=(bool value)
{
    set(value);
    return *this;
}

json &json::operator=(long long value)
{
    set(value);
    return *this;
}

json &json::operator=(double value)
{
    set(value);
    return *this;
}

json &json::operator=(const std::string &value)
{
    set(value);
    return *this;
}

json &json::operator=(const char *value)
{
    set(std::string(value ? value : ""));
    return *this;
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
    if (m_json_type == json_type::json_bool)
    {
        return m_bool_value;
    }
    throw std::logic_error("function json::as_bool value type error");
}

long long json::as_integer() const
{
    switch (m_json_type)
    {
    case json_type::json_integer:
        return m_integer_value;
    case json_type::json_double:
        // JSON has no integer/real distinction that consumers should have to
        // reason about: a real that is a whole number and fits in long long
        // coerces losslessly. Non-whole values (3.7) and out-of-range values
        // do not, so they throw rather than being silently truncated or
        // cast out of range. (2^63 is not representable in long long, so the
        // upper bound is a strict <; the lower bound -2^63 == LLONG_MIN is
        // exactly representable, so >= is safe. NaN fails the whole-number
        // check, ±inf fail the range check.)
        if (m_double_value == std::trunc(m_double_value) && m_double_value >= static_cast<double>(LLONG_MIN) && m_double_value < static_cast<double>(LLONG_MAX))
        {
            return static_cast<long long>(m_double_value);
        }
        break;
    default:
        break;
    }
    throw std::logic_error("function json::as_integer value type error");
}

double json::as_double() const
{
    switch (m_json_type)
    {
    case json_type::json_double:
        return m_double_value;
    case json_type::json_integer:
        return static_cast<double>(m_integer_value);
    default:
        break;
    }
    throw std::logic_error("function json::as_double value type error");
}

std::string json::as_string() const
{
    if (m_json_type == json_type::json_string)
    {
        return m_string_value;
    }
    throw std::logic_error("function json::as_string value type error");
}

void json::copy(const json &other)
{
    clear();
    m_json_type = other.m_json_type;
    m_bool_value = other.m_bool_value;
    m_integer_value = other.m_integer_value;
    m_double_value = other.m_double_value;
    m_string_value = other.m_string_value;
    m_array_value = other.m_array_value;
    m_object_value = other.m_object_value;
}

// Number of elements (array) or keys (object); throws for scalars.
std::size_t json::size() const
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

void json::clear() noexcept
{
    m_json_type = json_type::json_null;
    m_integer_value = 0;
    m_double_value = 0.;
    m_bool_value = false;
    m_string_value.clear();
    m_array_value.clear();
    m_object_value.clear();
}

bool json::remove(std::size_t index)
{
    if (m_json_type != json_type::json_array)
    {
        return false;
    }
    if (index >= m_array_value.size())
    {
        return false;
    }
    m_array_value.erase(m_array_value.begin() + static_cast<std::vector<json>::difference_type>(index));
    return true;
}

bool json::remove(const char *key)
{
    return remove(std::string(key ? key : ""));
}

bool json::remove(const std::string &key)
{
    if (type() != json_type::json_object)
    {
        return false;
    }
    auto it = m_object_value.find(key);
    if (it == m_object_value.end())
    {
        return false;
    }
    m_object_value.erase(it);
    return true;
}

bool json::has(std::size_t index) const
{
    if (m_json_type != json_type::json_array)
    {
        return false;
    }
    return index < m_array_value.size();
}

bool json::has(const char *key) const
{
    return has(std::string(key ? key : ""));
}

bool json::has(const std::string &key) const
{
    if (m_json_type != json_type::json_object)
    {
        return false;
    }
    return m_object_value.find(key) != m_object_value.end();
}

const json &json::get(std::size_t index) const
{
    if (m_json_type != json_type::json_array)
    {
        throw std::logic_error("function json::get [int] requires array value");
    }
    if (index < m_array_value.size())
    {
        return m_array_value.at(index);
    }
    return null();
}

const json &json::get(const char *key) const
{
    return get(std::string(key ? key : ""));
}

const json &json::get(const std::string &key) const
{
    if (m_json_type != json_type::json_object)
    {
        throw std::logic_error("function json::get [const string&] requires object value");
    }
    auto it = m_object_value.find(key);
    if (it != m_object_value.end())
    {
        return it->second;
    }
    return null();
}

void json::set(const json &other)
{
    *this = other;
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
    if (!std::isfinite(value))
    {
        throw std::logic_error("function json::set non-finite double");
    }
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

bool json::operator==(const json &other) const
{
    if (m_json_type != other.m_json_type)
    {
        return false;
    }
    switch (m_json_type)
    {
    case json_type::json_array:
        if (m_array_value.size() != other.m_array_value.size())
        {
            return false;
        }
        for (std::size_t i = 0; i < m_array_value.size(); i++)
        {
            if (!(m_array_value[i] == other.m_array_value[i]))
            {
                return false;
            }
        }
        return true;
    case json_type::json_bool:
        return m_bool_value == other.m_bool_value;
    case json_type::json_double:
        return m_double_value == other.m_double_value;
    case json_type::json_integer:
        return m_integer_value == other.m_integer_value;
    case json_type::json_null:
        return true;
    case json_type::json_object:
        if (m_object_value.size() != other.m_object_value.size())
        {
            return false;
        }
        for (const auto &entry : m_object_value)
        {
            auto it = other.m_object_value.find(entry.first);
            if (it == other.m_object_value.end() || !(it->second == entry.second))
            {
                return false;
            }
        }
        return true;
    case json_type::json_string:
        return m_string_value == other.m_string_value;
    }
    return false;
}

bool json::operator!=(const json &other) const
{
    return !(*this == other);
}

json &json::operator[](std::size_t index)
{
    if (m_json_type == json_type::json_null)
    {
        m_json_type = json_type::json_array;
    }
    if (m_json_type != json_type::json_array)
    {
        throw std::logic_error("function json::operator [int] requires array value");
    }
    if (index >= m_array_value.size())
    {
        // For SIZE_MAX, index + 1 wraps to 0; resize(0) would destroy the
        // existing elements, so throw instead of "growing".
        if (index + 1 < index)
        {
            throw std::logic_error("function json::operator [int] index out of range");
        }
        m_array_value.resize(index + 1);
    }
    return m_array_value.at(index);
}

const json &json::operator[](std::size_t index) const
{
    return get(index);
}

json &json::operator[](const char *key)
{
    return operator[](std::string(key ? key : ""));
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

json::operator bool() const
{
    if (m_json_type != json_type::json_bool)
    {
        throw std::logic_error("function json::operator (bool) requires bool value");
    }
    return m_bool_value;
}

json::operator long long() const
{
    if (m_json_type != json_type::json_integer)
    {
        throw std::logic_error("function json::operator (long long) requires long long value");
    }
    return m_integer_value;
}

json::operator double() const
{
    if (m_json_type != json_type::json_double)
    {
        throw std::logic_error("function json::operator (double) requires double value");
    }
    return m_double_value;
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
    switch (type())
    {
    case json_type::json_null:
        return "null";
    case json_type::json_bool:
        return m_bool_value ? "true" : "false";
    case json_type::json_integer:
        return std::to_string(m_integer_value);
    case json_type::json_double:
    {
        // JSON has no representation for NaN/Inf; constructing one is refused,
        // but guard here too since to_string is the public serialization path.
        if (!std::isfinite(m_double_value))
        {
            throw std::logic_error("function json::to_string non-finite double is not representable in JSON");
        }
        // std::to_string hard-codes 6 significant digits, which both rounds
        // non-round doubles (0.1 -> "0.100000") and is not guaranteed to
        // round-trip. %.17g always round-trips through strtod and %g already
        // strips trailing zeros. A whole-number double (1234.0, 1e7) formats
        // without a point or exponent and would re-parse as an integer,
        // flipping the node type; append ".0" to keep it a real.
        char buf[32];
        snprintf(buf, sizeof(buf), "%.17g", m_double_value);
        std::string s(buf);
        if (s.find('.') == std::string::npos && s.find('e') == std::string::npos && s.find('E') == std::string::npos)
        {
            s += ".0";
        }
        return s;
    }
    case json_type::json_string:
        return string_token(m_string_value);
    case json_type::json_array:
    {
        std::string out = "[";
        bool first = true;
        for (const auto &value : m_array_value)
        {
            if (!first)
            {
                out += ",";
            }
            first = false;
            out += value.to_string();
        }
        out += "]";
        return out;
    }
    case json_type::json_object:
    {
        std::string out = "{";
        bool first = true;
        for (const auto &entry : m_object_value)
        {
            if (!first)
            {
                out += ",";
            }
            first = false;
            out += "\"";
            append_escaped_body(entry.first, out);
            out += "\"";
            out += ":";
            out += entry.second.to_string();
        }
        out += "}";
        return out;
    }
    }
    return "null";
}
