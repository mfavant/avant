#pragma once

#include <string>

namespace avant
{
    namespace inifile
    {
        /**
         * @brief string store int、bool、double、string type
         *
         */
        class value
        {
        public:
            value() = default;
            value(bool value);
            value(int value);
            value(double value);
            value(const std::string &value);
            value(const char *value);

            value &operator=(bool value);
            value &operator=(int value);
            value &operator=(double value);
            value &operator=(const std::string &value);
            value &operator=(const char *value);

            operator bool() const;
            operator int() const;
            operator double() const;
            operator std::string() const;

            bool operator==(const value &other) const;

        private:
            std::string m_value{};
        };
    }
}
