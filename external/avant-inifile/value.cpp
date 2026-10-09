#include <cctype>
#include <cfloat>
#include <climits>
#include <cmath>
#include <cstddef>
#include <exception>
#include <string>

#include "value.h"

using namespace avant::inifile;

namespace avant::inifile
{
    static std::string to_lower(const std::string &s)
    {
        std::string t;
        t.reserve(s.size());
        for (const char c : s)
        {
            t.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        }
        return t;
    }
}

value::value(bool value)
{
    *this = value;
}

value::value(int value)
{
    *this = value;
}

value::value(double value)
{
    *this = value;
}

value::value(const std::string &value) : m_value(value)
{
}

value::value(const char *value) : m_value(value == nullptr ? "" : value)
{
}

value &value::operator=(bool value)
{
    m_value = value ? "true" : "false";
    return *this;
}

value &value::operator=(int value)
{
    m_value = std::to_string(value);
    return *this;
}

value &value::operator=(double value)
{
    m_value = std::to_string(value);
    return *this;
}

value &value::operator=(const std::string &value)
{
    m_value = value;
    return *this;
}

value &value::operator=(const char *value)
{
    m_value = value == nullptr ? "" : value;
    return *this;
}

value::operator bool() const
{
    // case-insensitive: "1"/"2"/"true"/"yes"/"on" are truthy, anything else
    // (including "0", "false", "no", "off", garbage, empty) is falsy
    const std::string s = to_lower(m_value);
    return s == "1" || s == "2" || s == "true" || s == "yes" || s == "on";
}

value::operator int() const
{
    // count only the leading digit run (what stoll would consume) so a decimal
    // like "123456789.123456789" is not mistaken for a magnitude overflow;
    // an oversized digit run saturates instead of wrapping through a 32-bit
    // `long` (stol silently overflows when `long` is 32 bits)
    const bool negative = (!m_value.empty() && m_value[0] == '-');
    std::size_t i = (!m_value.empty() && (m_value[0] == '-' || m_value[0] == '+')) ? 1 : 0;
    std::size_t digit_count = 0;
    for (; i < m_value.size() && std::isdigit(static_cast<unsigned char>(m_value[i])); ++i)
    {
        ++digit_count;
    }
    if (digit_count > 10)
    {
        return negative ? INT_MIN : INT_MAX;
    }

    try
    {
        std::size_t consumed = 0;
        const long long result = std::stoll(m_value, &consumed);
        if (result > INT_MAX)
        {
            return INT_MAX;
        }
        if (result < INT_MIN)
        {
            return INT_MIN;
        }
        return static_cast<int>(result);
    }
    catch (const std::exception &)
    {
        return 0;
    }
}

value::operator double() const
{
    // the only inputs that can overflow `double` are non-finite magnitudes;
    // `stod` already returns +/-HUGE_VAL and sets errno for those, and throws
    // `out_of_range` is implementation-defined — clamp explicitly either way
    try
    {
        std::size_t consumed = 0;
        const double result = std::stod(m_value, &consumed);
        if (result > DBL_MAX)
        {
            return DBL_MAX;
        }
        if (result < -DBL_MAX)
        {
            return -DBL_MAX;
        }
        if (!std::isfinite(result))
        {
            return result > 0 ? DBL_MAX : -DBL_MAX;
        }
        return result;
    }
    catch (const std::exception &)
    {
        return 0.0;
    }
}

value::operator std::string() const
{
    return m_value;
}

bool value::operator==(const value &other) const
{
    const std::string &lhs = this->m_value;
    const std::string &rhs = other.m_value;
    if (lhs.empty() || rhs.empty())
    {
        return lhs == rhs;
    }

    auto is_numeric = [](const std::string &s) -> bool
    {
        if (s.empty())
        {
            return false;
        }
        std::size_t i = (s[0] == '-' || s[0] == '+') ? 1 : 0;
        if (i >= s.size())
        {
            return false;
        }
        bool has_digit = false;
        for (; i < s.size(); ++i)
        {
            const char c = s[i];
            if (std::isdigit(static_cast<unsigned char>(c)))
            {
                has_digit = true;
            }
            else if (c == '.' || c == 'e' || c == 'E' || c == '+' || c == '-')
            {
                // allowed inside a number
            }
            else
            {
                return false;
            }
        }
        return has_digit;
    };

    if (is_numeric(lhs) && is_numeric(rhs))
    {
        // compare parsed numbers so that "3" == "3.0"
        return static_cast<double>(*this) == static_cast<double>(other);
    }

    // keep in sync with operator bool: same accepted set, case-insensitive
    // ("2" is the only digit operator bool treats as truthy beyond 0/1)
    auto in_bool_set = [](const std::string &s) -> bool
    {
        const std::string t = to_lower(s);
        return t == "0" || t == "1" || t == "2" || t == "false" || t == "true" || t == "no" || t == "yes" || t == "off" || t == "on";
    };
    if (in_bool_set(lhs) && in_bool_set(rhs))
    {
        return static_cast<bool>(*this) == static_cast<bool>(other);
    }

    return lhs == rhs;
}
