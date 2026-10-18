#pragma once
#include <cstddef>
#include <map>
#include <ostream>
#include <string>
#include <vector>

namespace avant
{
    namespace xml
    {
        class element
        {
        public:
            typedef std::vector<element>::iterator iterator;
            typedef std::vector<element>::const_iterator const_iterator;

        private:
            std::string m_name{};
            std::string m_text{};
            std::vector<element> m_children{}; // child nodes
            std::map<std::string, std::string> m_attrs{};

        public:
            element() = default;
            element(std::string name);
            element(std::string name, std::string text);

            /**
             * @brief get name
             *
             * @return const string&
             */
            const std::string &name() const;

            /**
             * @brief set name (moved in)
             *
             * @param name
             */
            void name(std::string name);

            /**
             * @brief get text
             *
             * @return const string&
             */
            const std::string &text() const;

            /**
             * @brief set text (moved in)
             *
             * @param text
             */
            void text(std::string text);

            /**
             * @brief get attr value (read-only; a missing key yields a shared
             *        static empty string, the element is not mutated)
             *
             * @param key
             * @return const string&
             */
            const std::string &attr(const std::string &key) const;

            /**
             * @brief whether the attribute exists
             *
             * @param key
             */
            bool has_attr(const std::string &key) const;

            /**
             * @brief set attr (a blank value deletes the attribute)
             *
             * @param key
             * @param value
             */
            void attr(const std::string &key, std::string value);

            /**
             * @brief all attributes
             */
            const std::map<std::string, std::string> &attrs() const;

            /**
             * @brief all child elements
             */
            const std::vector<element> &children() const;

            /**
             * @brief get child element by index (null element if out of range)
             *
             * @param index
             * @return const element&
             */
            const element &child_at(std::size_t index) const;

            /**
             * @brief get first child element by name (null element if absent)
             *
             * @param name
             * @return const element&
             */
            const element &child(const std::string &name) const;

            const element &operator[](std::size_t index) const;
            const element &operator[](const std::string &name) const;

            /**
             * @brief append child element (moved in)
             *
             * @param child
             */
            void append(element child);

            /**
             * @brief number of child elements
             *
             * @return size_t
             */
            std::size_t size() const;

            /**
             * @brief clear name, text, attributes and all children
             */
            void clear();

            iterator begin();
            iterator end();
            const_iterator begin() const;
            const_iterator cbegin() const;
            const_iterator cend() const;

            static const element &null();
        };

        /**
         * @brief serialize an element tree to xml text
         *
         * @param el
         * @return string
         */
        std::string to_string(const element &el);

        std::ostream &operator<<(std::ostream &os, const element &el);
    }
}
