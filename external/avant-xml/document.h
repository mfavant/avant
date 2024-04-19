#pragma once
#include <iostream>
#include <string>

#include "element.h"

namespace avant
{
    namespace xml
    {
        using std::string;
        /**
         * @brief xml document object
         *
         */
        class document
        {
        public:
            document();
            ~document() = default;

            /**
             * @brief to load xml file
             *
             * @param path
             */
            void load_file(const string &path);

            /**
             * @brief to load xml string
             *
             * @param str
             */
            void load_string(const string &str);

            /**
             * @brief skip index now if it's blank space、\t、\r、\n
             *
             */
            void skip_white_spaces();

            /**
             * @brief to construct node tree by element from xml string
             *
             * @return element
             */
            element parse();

        private:
            /**
             * @brief m_index skip to the index ?> next
             *
             * @return true
             * @return false
             */
            bool parse_declaration();

            /**
             * @brief skiping <!-- -->
             *
             * @return true
             * @return false
             */
            bool parse_comment();

            /**
             * @brief tag name can contain number、alpha、:、_、-
             *
             * @return string tag name
             */
            string parse_element_name();

            /**
             * @brief skiping to the next < and collecting the text skiped
             *
             * @return string
             */
            string parse_element_text();

            /**
             * @brief collecting the key from start tag
             *
             * @return string
             */
            string parse_element_attr_key();

            /**
             * @brief coolecting the value from start tag
             *
             * @return string
             */
            string parse_element_attr_val();

        private:
            string m_str{};  // xml context string
            size_t m_idx{0}; // analysis char index
        };
    }
}