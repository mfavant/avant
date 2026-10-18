#pragma once
#include <cstddef>
#include <string>

#include "element.h"

namespace avant
{
    namespace xml
    {
        /**
         * @brief xml document object
         *
         */
        class document
        {
        public:
            document() = default;
            ~document() = default;

            /**
             * @brief load an xml file
             *
             * @param path
             * @return false if the path is not a regular file (a device, fifo
             *         or directory has no EOF, so the whole-file read would
             *         never terminate), or if the file could not be opened,
             *         or could not be read in full (a truncated read would
             *         yield a half doc)
             */
            bool load_file(const std::string &path);

            /**
             * @brief load an xml string
             *
             * @param str
             */
            void load_string(const std::string &str);

            /**
             * @brief construct the node tree from the loaded xml string
             *
             * Resets the parse position, so it can be called repeatedly on the
             * same loaded document, and skips a UTF-8 BOM if the input begins
             * with one. Comments and processing instructions are legal both
             * before and after the root element; CDATA sections are not
             * supported. Text and attribute values are entity-decoded (the
             * five predefined entities and numeric character references);
             * any other '&' sequence is left as-is. Empty attribute values
             * (x="") are preserved.
             *
             * Throws std::logic_error on malformed input: empty document,
             * missing or non-element root, CDATA section, duplicate
             * attribute, unterminated declaration/comment/processing
             * instruction, unclosed or mismatched end tag, unterminated
             * attribute value, nesting deeper than k_max_depth, or mixed
             * text after child elements.
             *
             * @return element root node
             */
            element parse();

        private:
            static constexpr std::size_t k_max_depth = 128; // nesting limit

            /**
             * @brief skip white spaces (space, \t, \r, \n)
             */
            void skip_white_spaces();

            /**
             * @brief skip to just past the terminating "?>" of a "<?...?>"
             *        (xml declaration or processing instruction)
             *
             * @return false if unterminated
             */
            bool parse_declaration();

            /**
             * @brief skip to just past the terminating "-->" of a comment
             *        (the cursor must be on the '<' of the "<!--" opening)
             *
             * @return false if unterminated
             */
            bool parse_comment();

            /**
             * @brief whether the cursor is on a valid element start tag
             *        ('<' followed by alpha or '_')
             */
            bool is_element_start() const;

            /**
             * @brief parse one element (recursive); the cursor must be on its
             *        '<' (is_element_start() checked by the caller)
             *
             * @param depth nesting depth, guarded by k_max_depth
             * @return element
             */
            element parse_element(std::size_t depth);

            /**
             * @brief collect the tag name (alpha, number, :, _, -); the cursor
             *        must be on the first name char (the caller verified
             *        is_element_start, so the name is never empty)
             *
             * @return string tag name
             */
            std::string parse_element_name();

            /**
             * @brief collect text up to the next '<' (leading whitespace is
             *        ignorable and stripped; trailing whitespace is kept, so
             *        only whitespace-only text comes out empty; throws at end
             *        of input)
             *
             * @param owner element name, for the error message
             * @return string
             */
            std::string parse_element_text(const std::string &owner);

            /**
             * @brief collect the attribute key from the start tag
             *
             * @return string
             */
            std::string parse_element_attr_key();

            /**
             * @brief collect the quoted attribute value from the start tag
             *        ('"' or '\'')
             *
             * @return string
             */
            std::string parse_element_attr_val();

        private:
            std::string m_str{};  // xml context string
            std::size_t m_idx{0}; // analysis char index
        };
    }
}
