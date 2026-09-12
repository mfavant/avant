#include "utility/url.h"
#include <cstring>
#include <cctype>
#include <cstdlib>
#include <algorithm>
#include <stdexcept>
#include <climits>

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
        long long value = 0;
        try
        {
            value = std::stoll(this->port, nullptr, 10);
        }
        catch (const std::exception &)
        {
            // non-numeric / overflow: fall through to the scheme default
        }
        if (value > 0 && value <= USHRT_MAX)
        {
            return static_cast<unsigned short>(value);
        }
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
           a.fragment == b.fragment &&
           a.ipv6_host == b.ipv6_host &&
           a.secure == b.secure;
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

    if (a.fragment < b.fragment)
        return true;
    if (b.fragment < a.fragment)
        return false;

    if (a.ipv6_host < b.ipv6_host)
        return true;
    if (b.ipv6_host < a.ipv6_host)
        return false;

    return a.secure < b.secure;
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
    this->right_position = this->parse_target.find(right_delimiter, this->left_position);

    if (this->right_position == std::string_view::npos)
    {
        if (error_message.size())
        {
            throw std::runtime_error(error_message);
        }
        // delimiter absent: take the rest of the target
        return this->parse_target.substr(this->left_position);
    }

    std::string_view captured = this->parse_target.substr(this->left_position, this->right_position - this->left_position);

    return captured;
}

bool url::move_before(const std::string_view right_delimiter)
{
    size_t position = this->parse_target.find(right_delimiter, this->left_position);
    if (position != std::string_view::npos)
    {
        this->left_position = position;
        return true;
    }
    return false;
}

bool url::exists_forward(const std::string_view right_delimiter)
{
    size_t position = this->parse_target.find(right_delimiter, this->left_position);
    if (position != std::string_view::npos)
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
    this->only_path_query_fragment = false;
    this->authority_present = false;
    this->authority.clear();
    this->user_info.clear();
    this->scheme.clear();
    this->username.clear();
    this->password.clear();
    this->host.clear();
    this->port.clear();
    this->path.clear();
    this->query.clear();
    this->fragment.clear();
    this->ipv6_host = false;
    this->secure = false;
    this->query_parameters.clear();

    if (s.empty())
    {
        return;
    }

    if (s[0] == '/')
    {
        this->only_path_query_fragment = true;
    }
    else
    {
        // scheme
        this->scheme = this->capture_up_to(":", "Expected : in url");
        std::transform(this->scheme.begin(), this->scheme.end(),
                       this->scheme.begin(), [](std::string_view::value_type c)
                       { return static_cast<char>(std::tolower(c)); });
        this->left_position += this->scheme.size() + 1; // skip ":"

        // authority
        if (this->move_before("//"))
        {
            this->authority_present = true;
            this->left_position += 2; // skip "//"
        }
    }

    if (this->authority_present || this->only_path_query_fragment)
    {
        // authority 必须终止于最先出现的 '/'、'?' 或 '#' 之一（字符集合语义，
        // 这里直接用 find_first_of）。原代码只按 '/' 截断，导致
        // "http://host?query" 的 authority 被污染成 "host?query"。
        size_t authority_end = this->parse_target.find_first_of("/?#", this->left_position);
        this->right_position = authority_end;

        if (authority_end == std::string_view::npos)
        {
            this->authority = this->parse_target.substr(this->left_position);
        }
        else
        {
            this->authority = this->parse_target.substr(this->left_position,
                                                        authority_end - this->left_position);
        }

        // path 是否存在取决于 authority 的终止符是否为 '/'
        bool path_exists = (authority_end != std::string_view::npos &&
                            this->parse_target[authority_end] == '/');
        if (path_exists)
        {
            this->left_position = authority_end; // path 从 '/' 开始
        }

        // query 分隔符必须出现在 fragment 分隔符之前（RFC 3986：首个 '#' 之后全部属于 fragment）
        size_t query_pos = this->parse_target.find('?', this->left_position);
        size_t fragment_pos = this->parse_target.find('#', this->left_position);
        bool has_query = (query_pos != std::string_view::npos &&
                          (fragment_pos == std::string_view::npos || query_pos < fragment_pos));

        if (has_query) // exist query
        {
            if (path_exists)
            {
                this->path = this->capture_up_to("?");
            }
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
                if (path_exists)
                {
                    this->path = this->capture_up_to("#");
                }
                this->move_before("#");
                this->left_position += 1;                  // skip "#"
                this->fragment = this->capture_up_to("#"); // 首个 '#' 之后全部
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
        // 无 authority 的 opaque URI（如 mailto:user@example.com）：整体归入 path
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

    // detect ipv6 用 move_before 精确定位 '['
    if (this->move_before("["))
    {
        this->left_position += 1; // skip "["
        this->host = this->capture_up_to("]", "malformed ipv6");
        this->left_position = this->right_position + 1; // skip "]"
        this->ipv6_host = true;

        // ']' 之后允许出现 ":port"，如 "[::1]:8080"。原代码会静默丢弃端口。
        if (this->exists_forward(":"))
        {
            this->move_before(":");
            this->left_position += 1; // skip ":"
            this->port = this->capture_up_to("#");
        }
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
            this->host = this->capture_up_to(":");
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

    // 填充 query_parameters 。键值均做 URL 解码，
    // application/x-www-form-urlencoded 约定 '+' 表示空格；
    // 某一段解码失败时保留原文，避免丢参数。
    if (!this->query.empty())
    {
        size_t begin = 0;
        while (begin != std::string::npos && begin < this->query.size())
        {
            size_t end = this->query.find('&', begin);
            std::string pair = (end == std::string::npos)
                                   ? this->query.substr(begin)
                                   : this->query.substr(begin, end - begin);

            if (!pair.empty())
            {
                std::string key;
                std::string value;
                size_t eq = pair.find('=');
                if (eq == std::string::npos)
                {
                    key = pair; // 只有 key 没有 '=' 的参数，value 记为空
                }
                else
                {
                    key = pair.substr(0, eq);
                    value = pair.substr(eq + 1);
                }

                std::replace(key.begin(), key.end(), '+', ' ');
                std::replace(value.begin(), value.end(), '+', ' ');

                std::string decoded_key;
                std::string decoded_value;
                if (this->unescape_path(key, decoded_key) &&
                    this->unescape_path(value, decoded_value))
                {
                    this->query_parameters.emplace(decoded_key, decoded_value);
                }
                else
                {
                    this->query_parameters.emplace(key, value);
                }
            }

            begin = (end == std::string::npos) ? std::string::npos : end + 1;
        }
    }

    // update secure
    if (this->scheme == "ssh" || this->scheme == "https" || this->port == "443")
    {
        this->secure = true;
    }
    if (this->scheme == "postgres" || this->scheme == "postgresql")
    {
        auto it = this->query_parameters.find("ssl");
        if (it != this->query_parameters.end() && it->second == "true")
        {
            this->secure = true;
        }
    }
}

bool url::unescape_path(const std::string &in, std::string &out)
{
    out.clear();
    out.reserve(in.size());

    const auto hex_nibble = [](char c) -> int
    {
        if (c >= '0' && c <= '9')
            return c - '0';
        if (c >= 'a' && c <= 'f')
            return c - 'a' + 10;
        if (c >= 'A' && c <= 'F')
            return c - 'A' + 10;
        return -1;
    };

    for (size_t i = 0; i < in.size(); ++i)
    {
        if (in[i] == '%')
        {
            if (i + 3 > in.size())
            {
                return false;
            }
            const int hi = hex_nibble(in[i + 1]);
            const int lo = hex_nibble(in[i + 2]);
            if (hi < 0 || lo < 0)
            {
                return false;
            }
            out += static_cast<char>((hi << 4) | lo);
            i += 2;
        }
        else
        {
            out += in[i];
        }
    }

    return true;
}
