#pragma once
#include <string>
#include <map>
#include <iostream>

#include "value.h"

namespace avant
{
    namespace inifile
    {
        using namespace std;
        /**
         * @brief inifile parser
         *
         */
        class inifile
        {
        public:
            inifile() = default;
            inifile(const string &filename);
            ~inifile() = default;

            bool load(const string &filename);
            void save(const string &filename);
            void clear();

            // get
            value &get(const string &section, const string &key);

            // set
            void set(const string &section, const string &key, bool value);
            void set(const string &section, const string &key, int value);
            void set(const string &section, const string &key, double value);
            void set(const string &section, const string &key, const string &value);

            // has
            bool has(const string &section);
            bool has(const string &section, const string &key);

            // remove
            void remove(const string &section);
            void remove(const string &section, const string &key);

            // operator[key]
            std::map<string, value> &operator[](const string &key)
            {
                return m_inifile[key];
            }

            // out
            ostream &operator<<(ostream &os);

        private:
            /**
             * @brief delete " \r\n" start or end with s
             *
             * @param s
             * @return string result
             */
            string trim(string s);

        private:
            string m_filename{};
            std::map<string, map<string, value>> m_inifile{};
        };
    }
}