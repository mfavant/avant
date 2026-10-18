#include "element.h"

#include <sstream>
#include <utility>

namespace avant::xml
{
    element::element(std::string name) : m_name(std::move(name))
    {
    }

    element::element(std::string name, std::string text) : m_name(std::move(name)), m_text(std::move(text))
    {
    }

    const std::string &element::name() const
    {
        return m_name;
    }

    void element::name(std::string name)
    {
        m_name = std::move(name);
    }

    const std::string &element::text() const
    {
        return m_text;
    }

    void element::text(std::string text)
    {
        m_text = std::move(text);
    }

    const std::string &element::attr(const std::string &key) const
    {
        auto it = m_attrs.find(key);
        if (it != m_attrs.end())
        {
            return it->second;
        }
        static const std::string empty;
        return empty; // missing key: shared static empty, element stays unmutated
    }

    bool element::has_attr(const std::string &key) const
    {
        return m_attrs.find(key) != m_attrs.end();
    }

    void element::attr(const std::string &key, std::string value)
    {
        if (!value.empty())
        {
            m_attrs[key] = std::move(value);
            return;
        }
        m_attrs.erase(key); // blank value deletes the attribute
    }

    const std::map<std::string, std::string> &element::attrs() const
    {
        return m_attrs;
    }

    const std::vector<element> &element::children() const
    {
        return m_children;
    }

    const element &element::child_at(std::size_t index) const
    {
        if (index < m_children.size())
        {
            return m_children[index];
        }
        return null();
    }

    const element &element::child(const std::string &name) const
    {
        for (const auto &it : m_children)
        {
            if (it.name() == name)
            {
                return it; // first match
            }
        }
        return null();
    }

    const element &element::operator[](std::size_t index) const
    {
        return child_at(index);
    }

    const element &element::operator[](const std::string &name) const
    {
        return child(name);
    }

    void element::append(element child)
    {
        m_children.push_back(std::move(child));
    }

    std::size_t element::size() const
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

    element::iterator element::begin()
    {
        return m_children.begin();
    }

    element::iterator element::end()
    {
        return m_children.end();
    }

    element::const_iterator element::begin() const
    {
        return m_children.begin();
    }

    element::const_iterator element::cbegin() const
    {
        return m_children.cbegin();
    }

    element::const_iterator element::cend() const
    {
        return m_children.cend();
    }

    namespace
    {
        /**
         * @brief escape xml entities; extra_quote == '"' also escapes quotes
         *        (attribute values are emitted in double quotes; names and
         *        keys are escaped in the same mode, so a user-constructed
         *        tree with unusual names cannot emit malformed xml. Parsed
         *        names and keys never contain any escaped char, so the
         *        serialization of parsed trees is unaffected.)
         */
        std::string escape(const std::string &in, char extra_quote)
        {
            std::string out;
            out.reserve(in.size());
            for (char c : in)
            {
                if (c == '&')
                {
                    out += "&amp;";
                }
                else if (c == '<')
                {
                    out += "&lt;";
                }
                else if (c == '>')
                {
                    out += "&gt;";
                }
                else if (extra_quote == '"' && c == '"')
                {
                    out += "&quot;";
                }
                else
                {
                    out += c;
                }
            }
            return out;
        }
    }

    std::string to_string(const element &el)
    {
        if (el.name().empty())
        {
            return "";
        }
        std::ostringstream os;
        const std::string name = escape(el.name(), '"'); // escape once, use in both tags
        // before tag
        os << "<" << name;
        for (const auto &it : el.attrs())
        {
            os << " " << escape(it.first, '"') << "=\"" << escape(it.second, '"') << "\"";
        }
        if (el.children().empty() && el.text().empty())
        {
            os << " />"; // self-closing
            return os.str();
        }
        os << ">";
        // text comes before children so mixed text+children round-trips
        if (!el.text().empty())
        {
            os << escape(el.text(), '\0');
        }
        // children
        for (const auto &it : el.children())
        {
            os << to_string(it);
        }
        // after tag
        os << "</" << name << ">";
        return os.str();
    }

    std::ostream &operator<<(std::ostream &os, const element &el)
    {
        os << to_string(el);
        return os;
    }

    const element &element::null()
    {
        static const element null;
        return null;
    }
}
