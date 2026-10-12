#pragma once

#include <cstddef>
#include <map>
#include <ostream>
#include <string>
#include <vector>

namespace avant::json
{
    class parser;

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

        json() = default;
        json(bool value);
        json(long long value);
        // Throws std::logic_error for NaN/Inf (JSON has no representation).
        json(double value);
        json(const char *value);
        json(const std::string &value);
        json(const json &other);
        // Move ops leave the source node as null (a moved-from json is
        // always null, never a stale type tag with an empty payload).
        json(json &&other) noexcept;
        ~json() noexcept = default;

        json &operator=(const json &other);
        // As with the move ctor, leaves the source node as null.
        json &operator=(json &&other) noexcept;
        json &operator=(bool value);
        json &operator=(long long value);
        json &operator=(double value);
        json &operator=(const std::string &value);
        json &operator=(const char *value);

        [[nodiscard]] json_type type() const;
        [[nodiscard]] bool is_null() const;
        [[nodiscard]] bool is_bool() const;
        [[nodiscard]] bool is_integer() const;
        [[nodiscard]] bool is_double() const;
        [[nodiscard]] bool is_string() const;
        [[nodiscard]] bool is_array() const;
        [[nodiscard]] bool is_object() const;

        // as_integer also accepts a whole-number double that fits in long long,
        // and as_double also accepts an integer; both throw for values that
        // would not convert losslessly (3.7, 2^63, ...). as_bool/as_string are
        // strict: they only accept their own type.
        [[nodiscard]] bool as_bool() const;
        [[nodiscard]] long long as_integer() const;
        [[nodiscard]] double as_double() const;
        [[nodiscard]] std::string as_string() const;

        void copy(const json &other);

        // Number of elements (array) or keys (object); throws for scalars.
        [[nodiscard]] std::size_t size() const;

        [[nodiscard]] bool empty() const;

        // Resets the node to null (type tag included) and empties the payload.
        void clear() noexcept;

        bool remove(std::size_t index);
        bool remove(const char *key);
        bool remove(const std::string &key);

        [[nodiscard]] bool has(std::size_t index) const;
        [[nodiscard]] bool has(const char *key) const;
        [[nodiscard]] bool has(const std::string &key) const;

        [[nodiscard]] const json &get(std::size_t index) const;
        [[nodiscard]] const json &get(const char *key) const;
        [[nodiscard]] const json &get(const std::string &key) const;

        void set(const json &other);
        void set(bool value);
        void set(long long value);
        void set(double value);
        void set(const std::string &value);

        json &append(const json &value);

        bool operator==(const json &other) const;
        bool operator!=(const json &other) const;

        json &operator[](std::size_t index);
        const json &operator[](std::size_t index) const;

        json &operator[](const char *key);
        const json &operator[](const char *key) const;

        json &operator[](const std::string &key);
        const json &operator[](const std::string &key) const;

        // Explicit conversion of the node to the underlying scalar value;
        // throws if the node is not that type.
        explicit operator bool() const;
        explicit operator long long() const;
        explicit operator double() const;
        explicit operator std::string() const;

        [[nodiscard]] static const json &null();

        // Replace this node with the parsed value of str. Throws std::logic_error
        // on malformed input.
        void parse(const std::string &str);
        [[nodiscard]] std::string to_string() const;

        typedef std::vector<json>::iterator iterator;
        typedef std::vector<json>::const_iterator const_iterator;

        // Array iteration. begin()/end() on a non-array observe the (empty)
        // array view and therefore range-for over a non-array is a no-op.
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
        // Constructs a node of the given container type with an empty payload
        // (all other members take their default in-class initializers). The
        // parser (a friend) uses this to build an empty array/object before
        // filling it. Kept private so callers cannot construct a node by bare
        // type tag and bypass the scalar/value constructors; it is not part of
        // the public API.
        json(json_type type);

        // Serialize to a stream (compact, single line). Not noexcept:
        // to_string allocates (std::bad_alloc) and can throw std::logic_error
        // for a non-finite double; let those propagate instead of terminating.
        friend std::ostream &operator<<(std::ostream &os, const json &value)
        {
            return os << value.to_string();
        }

        // The parser builds the container nodes via the private type-ctor.
        friend class parser;

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
