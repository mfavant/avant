#include "utility/url.h"
#include <cstring>
#include <cctype>
#include <cstdlib>
#include <algorithm>
#include <stdexcept>

using avant::utility::url;

url::url()
{
}

url::url(const std::string &s)
{
    this->from_string(s);
}

std::string url::get_scheme() const
{
    return this->scheme;
}

std::string url::get_username() const
{
    return this->username;
}

std::string url::get_password() const
{
    return this->password;
}

std::string url::get_host() const
{
    return this->host;
}

unsigned short url::get_port() const
{
    if (this->port.size() > 0)
    {
        return std::atoi(this->port.c_str());
    }
    if (this->scheme == "https")
        return 443;
    if (this->scheme == "http")
        return 80;
    if (this->scheme == "ssh")
        return 22;
    if (this->scheme == "ftp")
        return 21;
    if (this->scheme == "mysql")
        return 3306;
    if (this->scheme == "mongo")
        return 27017;
    if (this->scheme == "mongo+srv")
        return 27017;
    if (this->scheme == "kafka")
        return 9092;
    if (this->scheme == "postgres")
        return 5432;
    if (this->scheme == "postgresql")
        return 5432;
    if (this->scheme == "redis")
        return 6379;
    if (this->scheme == "zookeeper")
        return 2181;
    if (this->scheme == "ldap")
        return 389;
    if (this->scheme == "ldaps")
        return 636;
    return 0;
}

std::string url::get_path() const
{
    std::string tmp_path;
    unescape_path(this->path, tmp_path);
    return tmp_path;
}

std::string url::get_query() const
{
    return this->query;
}

const std::multimap<std::string, std::string> &url::get_query_parameters() const
{
    return this->query_parameters;
}

std::string url::get_fragment() const
{
    return this->fragment;
}

// path+query+fragment
std::string url::get_full_path() const
{
    std::string full_path;

    if (this->get_path().size())
    {
        full_path += this->get_path();
    }

    if (this->get_query().size())
    {
        full_path += "?" + this->get_query();
    }

    if (this->get_fragment().size())
    {
        full_path += "#" + this->get_fragment();
    }

    return full_path;
}

bool url::is_ipv6() const
{
    return this->ipv6_host;
}

void url::set_secure(bool secure)
{
    this->secure = secure;
}

bool url::is_secure() const
{
    return this->secure;
}

bool avant::utility::operator==(const url &a, const url &b)
{
    return a.scheme == b.scheme &&
           a.username == b.username &&
           a.password == b.password &&
           a.host == b.host &&
           a.port == b.port &&
           a.path == b.path &&
           a.query == b.query &&
           a.fragment == b.fragment;
}

bool avant::utility::operator!=(const url &a, const url &b)
{
    return !(a == b);
}

bool avant::utility::operator<(const url &a, const url &b)
{
    if (a.scheme < b.scheme)
        return true;
    if (b.scheme < a.scheme)
        return false;

    if (a.username < b.username)
        return true;
    if (b.username < a.username)
        return false;

    if (a.password < b.password)
        return true;
    if (b.password < a.password)
        return false;

    if (a.host < b.host)
        return true;
    if (b.host < a.host)
        return false;

    if (a.port < b.port)
        return true;
    if (b.port < a.port)
        return false;

    if (a.path < b.path)
        return true;
    if (b.path < a.path)
        return false;

    if (a.query < b.query)
        return true;
    if (b.query < a.query)
        return false;

    return a.fragment < b.fragment;
}

std::string url::to_string() const
{
    return this->whole_url_storage;
}

url::operator std::string() const
{
    return this->to_string();
}

std::string_view url::capture_up_to(const std::string_view right_delimiter, const std::string &error_message /*= ""*/)
{
    this->right_position = this->parse_target.find_first_of(right_delimiter, this->left_position);

    if (right_position == std::string::npos && error_message.size())
    {
        throw std::runtime_error(error_message);
    }
    std::string_view captured = this->parse_target.substr(this->left_position, this->right_position - this->left_position);

    return captured;
}

bool url::move_before(const std::string_view right_delimiter)
{
    size_t position = this->parse_target.find_first_of(right_delimiter, this->left_position);
    if (position != std::string::npos)
    {
        this->left_position = position;
        return true;
    }
    return false;
}

bool url::exists_forward(const std::string_view right_delimiter)
{
    size_t position = this->parse_target.find_first_of(right_delimiter, this->left_position);
    if (position != std::string::npos)
    {
        return true;
    }
    return false;
}

void url::from_string(const std::string &s)
{
    this->whole_url_storage = s; // copy
    // reset target
    this->parse_target = this->whole_url_storage;
    this->left_position = 0;
    this->right_position = 0;
    this->authority_present = false;

    // scheme
    this->scheme = this->capture_up_to(":", "Expected : in url");
    std::transform(this->scheme.begin(), this->scheme.end(),
                   this->scheme.begin(), [](std::string_view::value_type c)
                   { return std::tolower(c); });
    this->left_position += this->scheme.size() + 1; // skip ":"

    // authority
    if (this->move_before("//"))
    {
        this->authority_present = true;
        this->left_position += 2; // skip "//"
    }

    if (this->authority_present)
    {
        this->authority = this->capture_up_to("/");
        bool path_exists = true;
        if (this->move_before("/"))
        {
            path_exists = true;
        }

        if (this->exists_forward("?")) // exist query
        {
            this->path = this->capture_up_to("?");
            this->move_before("?");
            this->left_position += 1; // skip "?"

            if (this->exists_forward("#")) // exist fragment
            {
                this->query = this->capture_up_to("#");
                this->move_before("#");
                this->left_position += 1;                  // skip "#"
                this->fragment = this->capture_up_to("#"); // part after #
            }
            else // no fragment
            {
                this->query = this->capture_up_to("#");
            }
        }
        else // no query
        {
            if (this->exists_forward("#")) // exist fragment
            {
                this->path = this->capture_up_to("#");
                this->move_before("#");
                this->left_position += 1; // skip "#"
                this->fragment = this->capture_up_to("#");
            }
            else // no fragment
            {
                if (path_exists)
                {
                    this->path = this->capture_up_to("#");
                }
            }
        }
    }
    else
    {
        this->path = this->capture_up_to("#");
    }

    // parse authority
    // reset target
    this->parse_target = this->authority;
    this->left_position = 0;
    this->right_position = 0;

    if (this->exists_forward("@"))
    {
        this->user_info = this->capture_up_to("@");
        this->move_before("@");
        this->left_position += 1; // skip "@"
    }
    else
    {
        // no user_info
    }

    // detect ipv6
    if (this->exists_forward("["))
    {
        this->left_position += 1; // skip "["
        this->host = this->capture_up_to("]", "malformed ipv6");
        this->left_position += 1; // skip "]"
        this->ipv6_host = true;
    }
    else
    {
        if (this->exists_forward(":")) // exist port
        {
            this->host = this->capture_up_to(":");
            this->move_before(":");
            this->left_position += 1;              // skip ":"
            this->port = this->capture_up_to("#"); // part after :
        }
        else // no port
        {
            this->port = this->capture_up_to(":");
        }
    }

    // parse user_info
    // reset target
    this->parse_target = this->user_info;
    this->left_position = 0;
    this->right_position = 0;
    if (this->exists_forward(":")) // exist password
    {
        this->username = this->capture_up_to(":");
        this->move_before(":");
        this->left_position += 1;                  // skip ":"
        this->password = this->capture_up_to("#"); // part after ":"
    }
    else // no password
    {
        this->username = this->capture_up_to(":");
    }

    // update secure
    if (this->scheme == "ssh" || this->scheme == "https" || this->port == "443")
    {
        this->secure = true;
    }
    if (this->scheme == "postgres" || this->scheme == "postgresql")
    {
        this->parse_target = this->query;
        this->left_position = 0;
        this->right_position = 0;
        if (this->exists_forward("ssl=true"))
        {
            this->secure = true;
        }
    }
}

bool url::unescape_path(const std::string &in, std::string &out)
{
    out.clear();
    out.reserve(in.size());

    for (size_t i = 0; i < in.size(); ++i)
    {
        switch (in[i])
        {
        case '%':
        {
            if (i + 3 <= in.size())
            {
                unsigned int value = 0;
                for (size_t j = i + 1; j < i + 3; ++j)
                {
                    switch (in[j])
                    {
                    case '0':
                    case '1':
                    case '2':
                    case '3':
                    case '4':
                    case '5':
                    case '6':
                    case '7':
                    case '8':
                    case '9':
                        value += in[j] - '0';
                        break;

                    case 'a':
                    case 'b':
                    case 'c':
                    case 'd':
                    case 'e':
                    case 'f':
                        value += in[j] - 'a' + 10;
                        break;

                    case 'A':
                    case 'B':
                    case 'C':
                    case 'D':
                    case 'E':
                    case 'F':
                        value += in[j] - 'A' + 10;
                        break;

                    default:
                        return false;
                    }

                    if (j == i + 1)
                        value = value << 4;
                }

                out += static_cast<char>(value);
                i += 2;
            }
            else
            {
                return false;
            }
        }
        break;

        case '-':
        case '_':
        case '.':
        case '!':
        case '~':
        case '*':
        case '\'':
        case '(':
        case ')':
        case ':':
        case '@':
        case '&':
        case '=':
        case '+':
        case '$':
        case ',':
        case '/':
        case ';':
        {
            out += in[i];
        }
        break;

        default:
        {
            if (!std::isalnum(in[i]))
                return false;
            out += in[i];
        }
        break;
        }
    }

    return true;
}
