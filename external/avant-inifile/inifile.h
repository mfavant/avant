#pragma once

#include <iosfwd>
#include <map>
#include <string>
#include <string_view>

#include "value.h"

namespace avant
{
    namespace inifile
    {
        /**
         * @brief inifile parser
         *
         */
        class inifile
        {
        public:
            inifile() = default;
            inifile(const std::string &filename);
            ~inifile() = default;

            bool load(const std::string &filename);
            bool save(const std::string &filename);
            void clear();

            // get (non-mutating: a missing section or key yields this
            // instance's default-constructed entry; const, since writing
            // through the reference would land in that entry, not in this
            // file)
            const value &get(const std::string &section, const std::string &key);

            // set (the stored text is the string form; type overloads would be
            // ambiguous for literals — `set("s","k","1")` cannot tell bool from
            // string — so the single string form is the whole API)
            void set(const std::string &section, const std::string &key, const std::string &value);

            // has
            bool has(const std::string &section);
            bool has(const std::string &section, const std::string &key);

            // remove
            void remove(const std::string &section);
            void remove(const std::string &section, const std::string &key);

            // operator[section]: an existing section is returned by reference;
            // a missing section yields this instance's own stand-in empty map
            // — writes there stay in this instance, are never persisted by
            // save(), and are wiped on the next load(); use set() to store
            // values. Note that ini["s"]["k"] on an *existing* section still
            // default-constructs a missing key (std::map operator[] semantics).
            std::map<std::string, value> &operator[](const std::string &key);

            // out
            std::ostream &operator<<(std::ostream &os);

        private:
            /**
             * @brief delete " \t\r\n" start or end from s
             *
             * @param s
             * @return string result
             */
            static std::string trim(std::string_view s);

        private:
            std::string m_filename;
            std::map<std::string, std::map<std::string, value>> m_inifile{};
            // per-instance stand-in for sections that do not exist; cleared on
            // every load() / clear() so stale entries never leak
            std::map<std::string, value> m_missing_section{};
            // per-instance stand-in for values that do not exist
            value m_default_value{};
        };
    }
}
