#pragma once
#include <string>
#include <vector>
#include <map>

namespace avant::json
{
    class json
    {
    public:
        enum class json_type
        {
            json_null = 0,
            json_bool,
            json_integer,
            json_double,
            json_string,
            json_array,
            json_object
        };

        json();
        json(json_type json_type);
        json(bool value);
        json(long long value);
        json(double value);
        json(const char *value);
        json(const std::string &value);
        json(const json &other);
        ~json();

        json_type type() const;
        bool is_null() const;
        bool is_bool() const;
        bool is_integer() const;
        bool is_double() const;
        bool is_string() const;
        bool is_array() const;
        bool is_object() const;

        bool as_bool() const;
        long long as_integer() const;
        double as_double() const;
        std::string as_string() const;

        void copy(const json &other);

        // array size
        size_t size() const;

        bool empty() const;

        void clear();

        bool remove(size_t index);
        bool remove(const char *key);
        bool remove(const std::string &key);

        bool has(size_t index) const;
        bool has(const char *key) const;
        bool has(const std::string &key) const;

        const json &get(size_t index) const;
        const json &get(const char *key) const;
        const json &get(const std::string &key) const;

        void set(const json &other);
        void set(bool value);
        void set(long long value);
        void set(double value);
        void set(const std::string &value);

        json &append(const json &value);

        json &operator=(const json &other);
        json &operator=(bool value);
        json &operator=(long long value);
        json &operator=(double value);
        json &operator=(const std::string &value);
        json &operator=(const char *value);

        bool operator==(const json &other);
        bool operator!=(const json &other);

        json &operator[](size_t index);
        const json &operator[](size_t index) const;

        json &operator[](const char *key);
        const json &operator[](const char *key) const;

        json &operator[](const std::string &key);
        const json &operator[](const std::string &key) const;

        explicit operator bool();
        explicit operator long long();
        explicit operator double();
        explicit operator std::string();
        explicit operator std::string() const;

        static json const &null();

        void parse(const std::string &str);
        std::string to_string() const;

        typedef std::vector<json>::iterator iterator;
        typedef std::vector<json>::const_iterator const_iterator;

        iterator begin()
        {
            return m_array_value.begin();
        }

        iterator end()
        {
            return m_array_value.end();
        }

        const_iterator begin() const
        {
            return m_array_value.begin();
        }

        const_iterator end() const
        {
            return m_array_value.end();
        }

    private:
        json_type m_json_type{json_type::json_null};
        long long m_integer_value{0};
        double m_double_value{0.};
        bool m_bool_value{false};
        std::string m_string_value{};

        std::vector<json> m_array_value{};
        std::map<std::string, json> m_object_value{};
    };
}