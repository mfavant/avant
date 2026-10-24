#include "document.h"

#include <cctype>
#include <fstream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>

#include <sys/stat.h>

namespace avant::xml
{
    namespace
    {
        /**
         * @brief append cp to out as utf-8 (identity for ascii)
         */
        void append_utf8(std::string &out, unsigned long cp)
        {
            if (cp < 0x80)
            {
                out += static_cast<char>(cp);
            }
            else if (cp < 0x800)
            {
                out += static_cast<char>(0xC0 | (cp >> 6));
                out += static_cast<char>(0x80 | (cp & 0x3F));
            }
            else if (cp < 0x10000)
            {
                out += static_cast<char>(0xE0 | (cp >> 12));
                out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                out += static_cast<char>(0x80 | (cp & 0x3F));
            }
            else
            {
                out += static_cast<char>(0xF0 | (cp >> 18));
                out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
                out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                out += static_cast<char>(0x80 | (cp & 0x3F));
            }
        }

        /**
         * @brief try to decode a numeric character reference at s[i] (s[i]
         *        must be '&'); appends the decoded char to out
         *
         * @return the number of input chars consumed (0 if s[i] does not
         *         start a well-formed "&#ddd;" / "&#xhhh;" reference: a
         *         dangling "&#x4", "&#x;" without digits, a NUL, or an
         *         out-of-range / surrogate code point)
         */
        std::size_t decode_numeric_ref(const std::string &s, std::size_t i, std::string &out)
        {
            if (i + 1 >= s.size() || s[i + 1] != '#')
            {
                return 0; // not "&#..."
            }
            std::size_t j = i + 2;
            int base = 10;
            if (j < s.size() && (s[j] == 'x' || s[j] == 'X'))
            {
                base = 16;
                ++j;
            }
            unsigned long cp = 0;
            const std::size_t digits_start = j;
            while (j < s.size())
            {
                const char c = s[j];
                int digit = (c >= '0' && c <= '9')                 ? c - '0'
                            : (base == 16 && c >= 'a' && c <= 'f') ? c - 'a' + 10
                            : (base == 16 && c >= 'A' && c <= 'F') ? c - 'A' + 10
                                                                   : -1;
                if (digit < 0)
                {
                    break;
                }
                if (cp <= 0x10FFFF) // stop accumulating out of range (no overflow on long runs)
                {
                    cp = cp * static_cast<unsigned long>(base) + static_cast<unsigned long>(digit);
                }
                ++j;
            }
            if (j == digits_start)
            {
                return 0; // "&#;" / "&#x;" has no digits
            }
            if (j >= s.size() || s[j] != ';')
            {
                return 0; // dangling reference (no terminating ';')
            }
            if (cp == 0 || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
            {
                return 0; // not a valid xml character; leave as-is
            }
            append_utf8(out, cp);
            return j + 1 - i;
        }

        /**
         * @brief decode the five predefined xml entities and the numeric
         *        character references in place; any other '&' sequence is
         *        left as-is. Decoding (not just passing through) keeps the
         *        parse -> to_string round-trip semantically stable, since
         *        to_string escapes every '&'.
         */
        void decode_entities(std::string &s)
        {
            static const std::pair<const char *, const char *> k_entities[] = {
                {"&amp;", "&"},
                {"&lt;", "<"},
                {"&gt;", ">"},
                {"&quot;", "\""},
                {"&apos;", "'"},
            };
            std::string out;
            out.reserve(s.size());
            for (std::size_t i = 0; i < s.size();)
            {
                if (s[i] == '&')
                {
                    bool matched = false;
                    for (const auto &e : k_entities)
                    {
                        // compare clamps to the remaining length, so a trailing
                        // partial entity ("&am") simply does not match
                        if (s.compare(i, std::char_traits<char>::length(e.first), e.first) == 0)
                        {
                            out += e.second;
                            i += std::char_traits<char>::length(e.first);
                            matched = true;
                            break;
                        }
                    }
                    if (!matched)
                    {
                        const std::size_t consumed = decode_numeric_ref(s, i, out);
                        if (consumed != 0)
                        {
                            i += consumed;
                        }
                        else
                        {
                            out += s[i];
                            ++i;
                        }
                    }
                    continue;
                }
                out += s[i];
                ++i;
            }
            s = std::move(out);
        }
    }

    bool document::load_file(const std::string &path)
    {
        // regular files only: operator<<(rdbuf) reads until EOF, and a device
        // (/dev/zero), fifo, or directory stream never reaches one — the size
        // guard below runs *after* the read, so it cannot stop an unbounded
        // read (old code: load_file("/dev/zero") hung forever, growing the
        // buffer). A directory additionally faked a 0-byte success on Linux
        // (the seek fails -> tellg is -1 -> the size check is skipped)
        struct stat st{};
        if (::stat(path.c_str(), &st) != 0 || !S_ISREG(st.st_mode))
        {
            return false;
        }
        // binary: byte-exact read (we handle BOM/UTF-8 ourselves); open in a
        // try, since a NUL inside the path makes open() throw instead of set
        // failbit — load_file must stay a pure bool
        std::ifstream in;
        try
        {
            in.open(path, std::ios::binary);
        }
        catch (const std::ios_base::failure &)
        {
            return false;
        }
        if (!in)
        {
            return false; // unreadable file: surface it, do not fake an empty doc
        }
        // expected byte count: a failed read may leave the stream with no
        // error bits at all, so the bytes actually read are compared against
        // the file size (the state check is a backstop, not the authority)
        in.seekg(0, std::ios::end);
        const std::streamoff expected = in.tellg();
        in.seekg(0, std::ios::beg);
        std::ostringstream buf;
        buf << in.rdbuf(); // whole file, newlines preserved
        if (in.fail() || in.bad() ||
            (expected >= 0 && buf.str().size() != static_cast<std::size_t>(expected)))
        {
            return false; // read error, or truncated/grown mid-read: a half doc is worse than none
        }
        load_string(buf.str());
        return true;
    }

    void document::load_string(const std::string &str)
    {
        m_str = str;
        m_idx = 0;
    }

    void document::skip_white_spaces()
    {
        while (m_idx < m_str.size())
        {
            char ch = m_str[m_idx];
            if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n')
            {
                ++m_idx;
            }
            else
            {
                break;
            }
        }
    }

    element document::parse()
    {
        m_idx = 0; // re-parsable: reset to the start of the loaded document
        // UTF-8 BOM (editors add it even when the rest of the file is ascii)
        if (m_str.size() >= 3 &&
            m_str[0] == static_cast<char>(0xEF) &&
            m_str[1] == static_cast<char>(0xBB) &&
            m_str[2] == static_cast<char>(0xBF))
        {
            m_idx = 3;
        }
        skip_white_spaces();
        if (m_idx >= m_str.size())
        {
            throw std::logic_error("xml document is empty");
        }
        // xml declaration: <?xml ... ?> (compare clamps, so short strings are safe).
        // The declaration target is exactly "xml": a name char right after it
        // makes this a processing instruction (<?xml-stylesheet?> etc.), which
        // the pre-root loop below handles on its own
        if (m_str.compare(m_idx, 5, "<?xml") == 0)
        {
            const bool is_declaration = m_idx + 5 >= m_str.size() ||
                                        m_str[m_idx + 5] == '?' ||
                                        m_str[m_idx + 5] == ' ' ||
                                        m_str[m_idx + 5] == '\t' ||
                                        m_str[m_idx + 5] == '\r' ||
                                        m_str[m_idx + 5] == '\n';
            if (is_declaration)
            {
                if (!parse_declaration())
                {
                    throw std::logic_error("xml declaration is not terminated with ?>");
                }
                skip_white_spaces();
            }
        }
        // content allowed between the declaration and the root: comments and
        // processing instructions in any mix (the same content is legal after
        // the root, see the trailing loop below); anything else is an error
        for (;;)
        {
            if (m_str.compare(m_idx, 4, "<!--") == 0)
            {
                if (!parse_comment())
                {
                    throw std::logic_error("xml comment is not terminated with -->");
                }
            }
            else if (m_idx + 1 < m_str.size() && m_str[m_idx] == '<' && m_str[m_idx + 1] == '?')
            {
                if (!parse_declaration())
                {
                    throw std::logic_error("xml processing instruction is not terminated with ?>");
                }
            }
            else
            {
                break;
            }
            skip_white_spaces();
            if (m_idx >= m_str.size())
            {
                throw std::logic_error("xml document has no root element");
            }
        }
        if (m_idx >= m_str.size())
        {
            throw std::logic_error("xml document has no root element");
        }
        if (!is_element_start())
        {
            throw std::logic_error("xml document root must be an element");
        }
        element root = parse_element(0);
        // legal trailing content: comments and processing instructions
        // (e.g. <?xml-stylesheet ... ?>); anything else is an error
        for (;;)
        {
            skip_white_spaces();
            if (m_idx >= m_str.size())
            {
                break; // clean end
            }
            if (m_str.compare(m_idx, 4, "<!--") == 0)
            {
                if (!parse_comment())
                {
                    throw std::logic_error("xml comment is not terminated with -->");
                }
                continue;
            }
            if (m_idx + 1 < m_str.size() && m_str[m_idx] == '<' && m_str[m_idx + 1] == '?')
            {
                if (!parse_declaration())
                {
                    throw std::logic_error("xml processing instruction is not terminated with ?>");
                }
                continue;
            }
            throw std::logic_error("xml has trailing content after root element");
        }
        return root;
    }

    bool document::parse_declaration()
    {
        std::size_t pos = m_str.find("?>", m_idx);
        if (pos == std::string::npos)
        {
            return false;
        }
        m_idx = pos + 2; // skip <?...?>
        return true;
    }

    bool document::parse_comment()
    {
        // the cursor is on the '<' of the "<!--" opening (verified by the
        // caller); the search starts past the opening so a terminator can
        // never overlap it, i.e. "<!--->" is correctly treated as unterminated
        std::size_t pos = m_str.find("-->", m_idx + 4);
        if (pos == std::string::npos)
        {
            return false;
        }
        m_idx = pos + 3;
        return true;
    }

    bool document::is_element_start() const
    {
        // the +1 read requires at least one byte past the '<' to exist
        return m_idx + 1 < m_str.size() &&
               m_str[m_idx] == '<' &&
               (std::isalpha(static_cast<unsigned char>(m_str[m_idx + 1])) ||
                m_str[m_idx + 1] == '_');
    }

    element document::parse_element(std::size_t depth)
    {
        if (depth >= K_MAX_DEPTH)
        {
            throw std::logic_error("xml nesting is too deep (limit " +
                                   std::to_string(K_MAX_DEPTH) + ")");
        }
        ++m_idx; // skip '<' (caller verified is_element_start)
        element elem;
        elem.name(parse_element_name());

        // attributes until '>' (or a self-closing ' />')
        std::set<std::string> seen_attrs{}; // duplicate detection, even for empty values
        for (;;)
        {
            skip_white_spaces();
            if (m_idx >= m_str.size())
            {
                throw std::logic_error("xml element <" + elem.name() +
                                       "> start tag is not terminated");
            }
            if (m_str[m_idx] == '>')
            {
                ++m_idx;
                break;
            }
            if (m_str[m_idx] == '/')
            {
                if (m_idx + 1 < m_str.size() && m_str[m_idx + 1] == '>')
                {
                    m_idx += 2; // self-closing tag
                    return elem;
                }
                throw std::logic_error("xml element <" + elem.name() +
                                       "> has a stray '/' in its start tag");
            }
            std::string key = parse_element_attr_key();
            if (key.empty())
            {
                throw std::logic_error("xml element <" + elem.name() +
                                       "> start tag is malformed");
            }
            skip_white_spaces();
            if (m_idx >= m_str.size() || m_str[m_idx] != '=')
            {
                throw std::logic_error("xml element <" + elem.name() +
                                       "> attribute '" + key + "' has no value");
            }
            ++m_idx; // skip '='
            // a set (not elem.has_attr): attr_store() overwrites, so without
            // this a duplicate would be silently replaced (even when the
            // first value is empty)
            if (!seen_attrs.insert(key).second)
            {
                throw std::logic_error("xml element <" + elem.name() +
                                       "> has duplicate attribute '" + key + "'");
            }
            std::string val = parse_element_attr_val();
            decode_entities(val);
            // attr_store (not attr): a blank value must be kept, so x=""
            // round-trips — the public attr() treats blank as deletion
            elem.attr_store(std::move(key), std::move(val));
        }

        // text before the first child (may be empty)
        std::string text = parse_element_text(elem.name());
        decode_entities(text);
        if (!text.empty())
        {
            elem.text(std::move(text));
        }

        // children / comments until the matching end tag. Every iteration
        // either advances m_idx or throws, so this loop cannot spin.
        for (;;)
        {
            skip_white_spaces();
            if (m_idx >= m_str.size())
            {
                throw std::logic_error("xml element <" + elem.name() +
                                       "> is not closed");
            }
            if (m_str[m_idx] != '<')
            {
                throw std::logic_error("xml element <" + elem.name() +
                                       "> has text after child elements");
            }
            if (m_idx + 1 < m_str.size() && m_str[m_idx + 1] == '/')
            {
                const std::string end_tag = "</" + elem.name() + ">";
                if (m_str.size() - m_idx < end_tag.size())
                {
                    // truncated at end of input: compare would clamp and
                    // report a misleading "mismatch"
                    throw std::logic_error("xml element <" + elem.name() +
                                           "> is not closed");
                }
                if (m_str.compare(m_idx, end_tag.size(), end_tag) != 0)
                {
                    throw std::logic_error("xml end tag does not match <" +
                                           elem.name() + ">");
                }
                m_idx += end_tag.size();
                return elem;
            }
            if (m_str.compare(m_idx, 4, "<!--") == 0)
            {
                if (!parse_comment())
                {
                    throw std::logic_error("xml comment is not terminated with -->");
                }
                continue;
            }
            if (m_str.compare(m_idx, 9, "<![CDATA[") == 0)
            {
                throw std::logic_error("xml CDATA sections are not supported (inside element <" +
                                       elem.name() + ">)");
            }
            if (is_element_start())
            {
                elem.append(parse_element(depth + 1));
                // any text after a child is not representable in this model
                std::string tail = parse_element_text(elem.name());
                if (!tail.empty())
                {
                    throw std::logic_error("xml element <" + elem.name() +
                                           "> has text after child elements");
                }
                continue;
            }
            throw std::logic_error("xml element <" + elem.name() +
                                   "> is malformed: expected an element or end tag");
        }
    }

    std::string document::parse_element_name()
    {
        // precondition: the cursor is on the first name char (alpha or '_'),
        // verified by is_element_start in parse_element (the only call site)
        std::string res(1, m_str[m_idx]); // first char
        ++m_idx;
        while (m_idx < m_str.size() &&
               (std::isalnum(static_cast<unsigned char>(m_str[m_idx])) ||
                m_str[m_idx] == '_' || m_str[m_idx] == '-' || m_str[m_idx] == ':'))
        {
            res.push_back(m_str[m_idx]);
            ++m_idx;
        }
        return res;
    }

    std::string document::parse_element_text(const std::string &owner)
    {
        skip_white_spaces();
        const std::size_t end = m_str.find('<', m_idx);
        if (end == std::string::npos)
        {
            throw std::logic_error("xml element <" + owner + "> is not closed");
        }
        std::string res = m_str.substr(m_idx, end - m_idx);
        m_idx = end;
        return res;
    }

    std::string document::parse_element_attr_key()
    {
        skip_white_spaces();
        std::string res;
        if (m_idx < m_str.size() &&
            (std::isalpha(static_cast<unsigned char>(m_str[m_idx])) || m_str[m_idx] == '_'))
        {
            res.push_back(m_str[m_idx]); // first char
            ++m_idx;
            while (m_idx < m_str.size() &&
                   (std::isalnum(static_cast<unsigned char>(m_str[m_idx])) ||
                    m_str[m_idx] == '_' || m_str[m_idx] == '-' || m_str[m_idx] == ':'))
            {
                res.push_back(m_str[m_idx]);
                ++m_idx;
            }
        }
        return res;
    }

    std::string document::parse_element_attr_val()
    {
        skip_white_spaces();
        if (m_idx >= m_str.size() || (m_str[m_idx] != '"' && m_str[m_idx] != '\''))
        {
            throw std::logic_error("xml element attribute value should be in quotes");
        }
        const char quote = m_str[m_idx];
        const std::size_t end = m_str.find(quote, m_idx + 1);
        if (end == std::string::npos)
        {
            throw std::logic_error("xml element attribute value is not terminated");
        }
        std::string res = m_str.substr(m_idx + 1, end - m_idx - 1);
        m_idx = end + 1; // skip right quote
        return res;
    }
}
