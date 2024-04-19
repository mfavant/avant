#pragma once
#include <map>
#include <vector>
#include <string>
#include <iostream>

namespace avant
{
    namespace xml
    {
        using std::string;
        class element
        {
        private:
            string m_name{};
            string m_text{};
            std::vector<element> m_children{}; // child nodes
            std::map<string, string> m_attrs{};

        public:
            element() = default;
            ~element() = default;
            element(const string &name);
            element(const string &name, const string &text);

            /**
             * @brief get name
             *
             * @return const string&
             */
            const string &name() const;

            /**
             * @brief set name
             *
             * @param name
             */
            void name(const string &name);

            /**
             * @brief get text
             *
             * @return const string&
             */
            const string &text() const;

            /**
             * @brief set text
             *
             * @param text
             */
            void text(const string &text);

            /**
             * @brief get attr
             *
             * @param key
             * @return string&
             */
            string &attr(const string &key);

            /**
             * @brief set attr
             *
             * @param key
             * @param value
             */
            void attr(const string &key, const string &value);

            /**
             * @brief get child element by index
             *
             * @param index
             * @return const element&
             */
            const element &operator[](int index) const;

            /**
             * @brief get child element by name
             *
             * @param name
             * @return const element&
             */
            const element &operator[](const string &name) const;

            /**
             * @brief append child element
             *
             * @param child
             */
            void append(const element &child);

            /**
             * @brief get numbers of the child
             *
             * @return int
             */
            int size();

            /**
             * @brief clear all child
             *
             */
            void clear();

            typedef std::vector<element>::iterator iterator;
            typedef std::vector<element>::const_iterator const_iterator;

            iterator begin();
            iterator end();
            const_iterator cbegin() const;
            const_iterator cend() const;
            std::ostream &operator<<(std::ostream &os);

            static const element &null();
            static string to_string(const element &el);
        };
    }
}
