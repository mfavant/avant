// homer::Url v0.3.0
// MIT License
// https://github.com/homer6/url

// This class takes inspiration and some source code from
// https://github.com/chriskohlhoff/urdl/blob/master/include/urdl/url.hpp

#pragma once
#include <string>
#include <string_view>
#include <map>

namespace avant::utility
{
    class url;

    bool operator==(const url &a, const url &b);
    bool operator!=(const url &a, const url &b);
    bool operator<(const url &a, const url &b);

    class url
    {
    public:
        url();
        url(const std::string &s);

        std::string get_scheme() const;
        std::string get_username() const;
        std::string get_password() const;
        std::string get_host() const;
        unsigned short get_port() const;
        std::string get_path() const;
        std::string get_query() const;
        [[nodiscard]]
        const std::multimap<std::string, std::string> &get_query_parameters() const;
        std::string get_fragment() const;
        std::string get_full_path() const; // path+query+fragment
        void from_string(const std::string &s);
        friend bool operator==(const url &a, const url &b);
        friend bool operator!=(const url &a, const url &b);
        friend bool operator<(const url &a, const url &b);
        void set_secure(bool secure);
        bool is_ipv6() const;
        bool is_secure() const;
        std::string to_string() const;
        explicit operator std::string() const;

        static bool unescape_path(const std::string &in, std::string &out);

    protected:
        std::string_view capture_up_to(const std::string_view right_delimiter, const std::string &error_message = "");
        bool move_before(const std::string_view right_delimiter);
        bool exists_forward(const std::string_view right_delimiter);

    protected:
        std::string scheme;
        std::string authority;
        std::string user_info;
        std::string username;
        std::string password;
        std::string host;
        std::string port;
        std::string path;
        std::string query;
        std::multimap<std::string, std::string> query_parameters;
        std::string fragment;

        bool secure{false};
        bool ipv6_host{false};
        bool only_path_query_fragment{false};
        bool authority_present = false;

        std::string whole_url_storage;
        size_t left_position{0};
        size_t right_position{0};
        std::string_view parse_target;
    };
}