#pragma once
#include <string>
namespace avant
{
    namespace inifile
    {
        using namespace std;
        /**
         * @brief string store int、bool、double、string type
         *
         */
        class value
        {
        public:
            value() = default;
            value(const bool &value);
            value(const int &value);
            value(const double &value);
            value(const string &value);
            ~value() = default;

            value &operator=(const bool &value);
            value &operator=(const int &value);
            value &operator=(const double &value);
            value &operator=(const string &value);

            operator bool() const;
            operator int() const;
            operator double() const;
            operator string() const;

            bool operator==(const value &other) const;

        private:
            string m_value{};
        };
    }
}